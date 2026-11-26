// Pengukur Suhu Solder — ESP32 + MAX6675 + LCD I2C 16x2 + Buzzer
//
// Alat kecil pengukur suhu ujung solder: termokopel tipe K ditempel di
// ujung solder, modul MAX6675 mengubah tegangan termokopel jadi suhu
// digital, ESP32 menampilkannya di LCD 16x2 lengkap dengan status
// (DINGIN / PAS / PANAS). Kalau suhu turun di bawah ambang, buzzer
// berbunyi — tandanya solder belum cukup panas untuk menyambung.
//
// Angka acuan (datasheet + praktik menyolder):
//   Timah Sn63/Pb37 (eutektik) meleleh di 183 C; timah bebas timbal
//   SAC305 meleleh di 217 C. Suhu kerja solder yang baik 300-350 C.
//   Di bawah 250 C sambungan rawan dingin (cold joint, retak).
//   Di atas 380 C fluks gosong dan komponen/pad bisa rusak.
//   MAX6675 (Maxim Integrated): termokopel tipe K, rentang 0-1024 C,
//   resolusi 0,25 C, antarmuka SPI, kompensasi cold-junction internal.
//   Modul MAX6675 TIDAK butuh library eksternal: protokolnya cuma baca
//   16 bit lewat SPI — bit 14..3 = suhu x 4, bit 2 = 1 bila termokopel
//   putus/tidak terpasang. Karena itu sketch ini hanya memakai SPI.h
//   (bawaan core ESP32) + LiquidCrystal_I2C.
//
// Wiring:
//   MAX6675 VCC -> ESP32 3V3
//   MAX6675 GND -> ESP32 GND
//   MAX6675 SCK -> ESP32 GPIO18 (SCK VSPI)
//   MAX6675 SO  -> ESP32 GPIO19 (MISO VSPI)
//   MAX6675 CS  -> ESP32 GPIO5  (SS VSPI)
//   LCD 16x2 I2C (0x27): VCC -> ESP32 VIN (5 V), GND -> GND,
//     SDA -> GPIO21, SCL -> GPIO22
//   Buzzer + -> ESP32 GPIO25 (aktif HIGH)
//   Buzzer - -> ESP32 GND
//
// Library yang dibutuhkan (Arduino IDE / arduino-cli):
//   LiquidCrystal_I2C 1.1.2
//
// Compile:
//   arduino-cli compile --fqbn esp32:esp32:esp32 PengukurSuhuSolderMAX6675
//
// Skema + repo: https://github.com/oerban70-cloud/pengukur-suhu-solder-max6675

#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------------------------------------------------------------------------
// Konfigurasi — semua angka yang bisa diubah ada di sini, bukan di tengah kode
// ---------------------------------------------------------------------------

// Nyalakan buzzer peringatan atau tidak (false = tanpa buzzer)
#define USE_BUZZER true

// Pin SPI modul MAX6675 (pin default VSPI ESP32 DevKit)
const uint8_t PIN_MAX6675_CS  = 5;   // Chip Select
const uint8_t PIN_MAX6675_SCK = 18;  // Clock
const uint8_t PIN_MAX6675_SO  = 19;  // Data keluar dari MAX6675 (MISO)

// Pin buzzer aktif (HIGH = bunyi)
const uint8_t PIN_BUZZER = 25;

// Pin bus I2C (pin default ESP32 DevKit)
const uint8_t PIN_I2C_SDA = 21;
const uint8_t PIN_I2C_SCL = 22;

// Alamat I2C LCD 16x2
const uint8_t ADDR_LCD = 0x27;

// --- Ambang suhu (derajat Celsius) ---
// Di bawah ambang ini solder terlalu dingin: sambungan rawan cold joint.
const float SUHU_MIN_BAIK_C = 250.0;
// Di atas ambang ini solder terlalu panas: fluks gosong, pad bisa lepas.
const float SUHU_MAKS_BAIK_C = 380.0;

// Seberapa sering suhu dibaca dan LCD diperbarui (milidetik)
const unsigned long INTERVAL_BACA_MS = 500;

// Durasi bunyi buzzer tiap peringatan (milidetik)
const unsigned long DURASI_BUNYI_MS = 200;

// --- Konstanta MAX6675 (datasheet) ---
// Suhu = (bit 14..3) x 0,25 C. Bit 2 = 1 berarti termokopel putus.
const float RESOLUSI_MAX6675_C = 0.25;
const uint16_t MASK_SUHU_MAX6675 = 0x7FF8;  // bit 14..3
const uint16_t BIT_TERMOKOPEL_PUTUS = 0x0004;  // bit 2

// ---------------------------------------------------------------------------
// Objek global
// ---------------------------------------------------------------------------

LiquidCrystal_I2C lcd(ADDR_LCD, 16, 2);

// ---------------------------------------------------------------------------
// Baca suhu dari MAX6675 lewat SPI (tanpa library eksternal)
// Mengembalikan suhu dalam Celsius, atau NAN bila termokopel putus.
// ---------------------------------------------------------------------------
float bacaSuhuMax6675() {
  digitalWrite(PIN_MAX6675_CS, LOW);
  delayMicroseconds(1);
  // MAX6675 mengirim 16 bit: bit 15 dummy, bit 14..3 suhu, bit 2 putus.
  uint16_t mentah = SPI.transfer16(0x0000);
  digitalWrite(PIN_MAX6675_CS, HIGH);

  if (mentah & BIT_TERMOKOPEL_PUTUS) {
    return NAN;  // termokopel tidak terpasang / putus
  }
  uint16_t nilai = (mentah & MASK_SUHU_MAX6675) >> 3;
  return nilai * RESOLUSI_MAX6675_C;
}

// ---------------------------------------------------------------------------
// Status suhu: 0 = DINGIN, 1 = PAS, 2 = PANAS
// ---------------------------------------------------------------------------
uint8_t statusSuhu(float suhuC) {
  if (suhuC < SUHU_MIN_BAIK_C) return 0;
  if (suhuC > SUHU_MAKS_BAIK_C) return 2;
  return 1;
}

const char* teksStatus(uint8_t status) {
  if (status == 0) return "DINGIN";
  if (status == 2) return "PANAS!";
  return "PAS";
}

// ---------------------------------------------------------------------------
// Tampilkan suhu + status di LCD 16x2
//   Baris 1: "Suhu: 321.5 C"
//   Baris 2: "Status: PAS"
// ---------------------------------------------------------------------------
void tampilkanLcd(float suhuC, bool termokopelOk) {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (!termokopelOk) {
    lcd.print("Termokopel?");
    lcd.setCursor(0, 1);
    lcd.print("Cek kabel K");
    return;
  }
  lcd.print("Suhu: ");
  lcd.print(suhuC, 1);
  lcd.print(" C");
  lcd.setCursor(0, 1);
  lcd.print("Status: ");
  lcd.print(teksStatus(statusSuhu(suhuC)));
}

// ---------------------------------------------------------------------------
// Bunyikan buzzer sekali (non-blocking: pakai delay pendek di loop utama)
// ---------------------------------------------------------------------------
void bunyikanBuzzer() {
#if USE_BUZZER
  digitalWrite(PIN_BUZZER, HIGH);
  delay(DURASI_BUNYI_MS);
  digitalWrite(PIN_BUZZER, LOW);
#endif
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("Pengukur Suhu Solder — ESP32 + MAX6675 + LCD I2C");

  // SPI VSPI default: SCK 18, MISO 19, MOSI 23 (tidak dipakai MAX6675).
  // MAX6675 maks 4,3 MHz — pakai 2,5 MHz supaya aman.
  SPI.begin(PIN_MAX6675_SCK, PIN_MAX6675_SO, -1, PIN_MAX6675_CS);
  SPI.beginTransaction(SPISettings(2500000, MSBFIRST, SPI_MODE0));

  pinMode(PIN_MAX6675_CS, OUTPUT);
  digitalWrite(PIN_MAX6675_CS, HIGH);

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Suhu Solder");
  lcd.setCursor(0, 1);
  lcd.print("MAX6675 siap");
  delay(1000);
}

// ---------------------------------------------------------------------------
void loop() {
  static unsigned long terakhirBaca = 0;
  unsigned long sekarang = millis();
  if (sekarang - terakhirBaca < INTERVAL_BACA_MS) return;
  terakhirBaca = sekarang;

  float suhuC = bacaSuhuMax6675();
  bool termokopelOk = !isnan(suhuC);

  tampilkanLcd(suhuC, termokopelOk);

  if (termokopelOk) {
    uint8_t status = statusSuhu(suhuC);
    Serial.print("Suhu: ");
    Serial.print(suhuC, 1);
    Serial.print(" C, status: ");
    Serial.println(teksStatus(status));
    // Peringatan hanya saat suhu di luar rentang baik
    if (status != 1) {
      bunyikanBuzzer();
    }
  } else {
    Serial.println("Termokopel putus / tidak terpasang!");
    bunyikanBuzzer();
  }
}
