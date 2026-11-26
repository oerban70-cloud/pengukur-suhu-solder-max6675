# Pengukur Suhu Solder — ESP32 + MAX6675 + LCD I2C 16x2

Solderan retak terus padahal timahnya sudah cair? Kemungkinan besar ujung
soldermu belum cukup panas — atau malah kepanasan. Alat kecil ini mengukur
suhu ujung solder langsung dengan termokopel tipe K, modul MAX6675 mengubah
sinyalnya jadi angka digital, dan ESP32 menampilkannya di LCD 16x2 lengkap
dengan statusnya — DINGIN, PAS, atau PANAS. Kalau suhu turun di bawah
250 derajat, buzzer langsung bunyi sebagai peringatan.

Video penjelasan (60 detik):
link menyusul setelah upload

## Komponen

| Bagian | Keterangan |
|---|---|
| ESP32 DevKit | Otak alat |
| MAX6675 | Modul termokopel tipe K, 0-1024 C, resolusi 0,25 C, SPI |
| Termokopel tipe K | Ujung sensor ditempel di ujung solder |
| LCD 16x2 I2C (0x27) | Tampil suhu + status (bus I2C) |
| Buzzer aktif | Bunyi kalau suhu di bawah 250 C atau di atas 380 C (GPIO25) |
| Adaptor USB 5V 2A | Catu daya ESP32 + LCD |

## Wiring

| Dari | Ke |
|---|---|
| MAX6675 VCC | ESP32 3V3 |
| MAX6675 GND | ESP32 GND |
| MAX6675 SCK | ESP32 GPIO18 (SCK VSPI) |
| MAX6675 SO | ESP32 GPIO19 (MISO VSPI) |
| MAX6675 CS | ESP32 GPIO5 (SS VSPI) |
| Termokopel K + | MAX6675 + |
| Termokopel K - | MAX6675 - |
| LCD VCC | ESP32 VIN (5 V dari USB) |
| LCD SDA / SCL | ESP32 GPIO21 / GPIO22 (bus I2C) |
| LCD GND | ESP32 GND |
| Buzzer + | ESP32 GPIO25 (aktif HIGH) |
| Buzzer - | ESP32 GND |

Skema lengkap: [schematic.png](schematic.png)

## Cara kerja

1. Termokopel tipe K menghasilkan tegangan kecil yang sebanding dengan
   suhu ujung solder. Modul MAX6675 membacanya lewat SPI: 16 bit data,
   bit 14..3 adalah suhu dikali 4 (resolusi 0,25 C), bit 2 menandai
   termokopel putus. Modul ini tidak butuh library eksternal — sketch
   membaca SPI langsung.
2. ESP32 membaca suhu tiap 500 milidetik dan menampilkannya di LCD 16x2
   beserta status: DINGIN di bawah 250 C, PANAS di atas 380 C, PAS di
   antaranya (suhu kerja solder yang baik 300-350 C).
3. Kalau status bukan PAS, buzzer aktif berbunyi sekali tiap pembacaan —
   tandanya solder belum siap dipakai, atau malah kepanasan sampai fluks
   gosong. Matikan dengan mengubah `USE_BUZZER` jadi false.
4. Kalau termokopel putus atau tidak terpasang, LCD menampilkan
   "Termokopel?" dan buzzer berbunyi sebagai peringatan.

## Angka acuan

- Timah Sn63/Pb37 (eutektik) meleleh di 183 C; timah bebas timbal SAC305
  meleleh di 217 C.
- Suhu kerja solder yang baik: 300-350 C. Di bawah 250 C sambungan rawan
  dingin (cold joint, retak). Di atas 380 C fluks gosong dan pad bisa lepas.
- MAX6675: rentang 0-1024 C, resolusi 0,25 C, SPI maks 4,3 MHz
  (sketch memakai 2,5 MHz), kompensasi cold-junction internal.

## Konfigurasi (di awal sketch)

- `USE_BUZZER` (true) — nyalakan/matikan buzzer peringatan.
- `PIN_MAX6675_CS` (5) / `PIN_MAX6675_SCK` (18) / `PIN_MAX6675_SO` (19)
  — pin SPI modul MAX6675 (pin default VSPI ESP32).
- `PIN_BUZZER` (25) — pin buzzer aktif.
- `PIN_I2C_SDA` (21) / `PIN_I2C_SCL` (22) — pin bus I2C LCD.
- `SUHU_MIN_BAIK_C` (250) / `SUHU_MAKS_BAIK_C` (380) — ambang status.
- `INTERVAL_BACA_MS` (500) — seberapa sering suhu dibaca dan LCD diperbarui.

## Library yang dibutuhkan

| Library | Versi | Keterangan |
|---|---|---|
| LiquidCrystal_I2C | 1.1.2 | Layar LCD 16x2 I2C |

Library di atas juga tersedia sebagai ZIP di folder [`libraries/`](libraries/)
— install lewat Arduino IDE: Sketch → Include Library → Add .ZIP Library.
Modul MAX6675 tidak butuh library tambahan (SPI bawaan core ESP32).

## Compile

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 PengukurSuhuSolderMAX6675
```

Terverifikasi 26 Nov 2026: sketch 320.785 byte (24% flash),
RAM 24.712 byte (7%), ESP32 core 4.0.0.

## Lisensi

MIT — lihat [LICENSE](LICENSE).
