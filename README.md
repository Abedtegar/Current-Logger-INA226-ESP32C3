# Current Logger — INA226 + ESP32-C3

Deskripsi singkat
-----------------
Proyek ini mengumpulkan pengukuran tegangan, shunt (mV), arus, dan daya dari modul INA226 yang terhubung ke ESP32-C3. Data dikirimkan secara bersamaan lewat Serial (USB) dan BLE (GATT Notification). Ada juga skrip Python yang berfungsi sebagai BLE client untuk menerima notifikasi, menyimpan CSV timestamped, dan menampilkan grafik realtime.

Struktur proyek
---------------
- `src/main.cpp` — firmware ESP32-C3 (INA226, BLE, kalibrasi)
- `logs/SerialLogger.py` — Python BLE client untuk logging dan plotting
- `logs/requirements.txt` — dependensi Python (bleak, matplotlib)
- `platformio.ini` — konfigurasi PlatformIO
- `logs/<timestamp>/data_log.csv` — folder keluaran CSV per sesi

Perangkat keras
---------------
- Board: ESP32-C3 (mis. Supermini / XIAO ESP32-C3)
- Sensor: INA226 (I2C)
- Sambungan I2C yang digunakan di `main.cpp`:
  - `SDA_PIN = 8`
  - `SCL_PIN = 9`
- Pin lainnya (indicator/buzzer): `BUZZ_PIN = 2`, `LED_PIN = 1`, `ALERT_PIN = 10`

Alur kerja firmware (ESP32)
---------------------------
1. Inisialisasi Serial (115200) dan I2C.
2. Inisialisasi INA226.
3. Kalibrasi arus otomatis menggunakan `INA.setMaxCurrentShunt(maxCurrent, shunt, normalized)`.
   - `maxCurrent` (A): batas arus maksimum yang ingin diukur.
   - `shunt` (Ohm): nilai resistor shunt yang dipasang.
   - `normalized` (bool): jika `true` LSB dinormalisasi untuk kompatibilitas.
4. Inisialisasi BLE GATT server dengan satu service + characteristic (READ + NOTIFY).
   - Service UUID: `180A` (16-bit, contoh)
   - Characteristic UUID: `2A58` (16-bit, contoh)
5. Loop utama membaca INA226, menerapkan koreksi tegangan (multiplier & offset), dan mengirim satu baris data:
   `timestamp \t bus_V \t shunt_mV \t current_mA \t power_mW`
   - Data dicetak ke Serial dan dikirim lewat NOTIFY ke klient BLE jika terhubung.
6. Penanganan BLE: jika client disconnect, firmware akan menghentikan sebentar dan memulai ulang advertising agar perangkat lain dapat pairing ulang.

Format data
-----------
Setiap baris yang dikirim memiliki format (tab-separated):

```
HH:MM:SS\t bus_voltage(V) \t shunt_voltage(mV) \t current(mA) \t power(mW)
```

Kalibrasi (ringkas + rumus)
---------------------------
- Kalibrasi arus (shunt):
  - Jika INA226 membaca `I_ina` (mA) dan multimeter (DMM) membaca `I_dmm` (mA) pada beban yang sama,
    maka:
    ```
    shunt_new = shunt_old * (I_ina / I_dmm)
    ```
- Kalibrasi tegangan bus (multiplier & offset):
  - Jika INA226 membaca `V_ina` dan DMM membaca `V_dmm`:
    ```
    multiplier = V_dmm / V_ina
    calibrated_V = V_ina * multiplier + offset
    ```
- Current zero offset: lepaskan beban dan catat rata-rata arus (`I_zero`), lalu gunakan nilai itu untuk mengkompensasi pembacaan (dapat diatur manual di kode atau via metode `configure()` dari library INA226).

Di mana mengubah parameter kalibrasi
----------------------------------
- Buka `src/main.cpp` dan ubah variabel pada bagian setup:
  - `maxCurrent` (A)
  - `shunt` (Ohm)
  - `normalized` (bool)
  - `g_bus_voltage_multiplier` dan `g_bus_voltage_offset` untuk kalibrasi tegangan

Build dan upload firmware
-------------------------
Menggunakan PlatformIO (VSCode) — cara cepat di terminal:

```powershell
cd <project-folder>
# build
platformio run
# upload (pastikan board terhubung dan port benar)
platformio run --target upload
# buka serial monitor
platformio device monitor --baud 115200
```

Python logger (BLE client)
---------------------------
Skrip: `logs/SerialLogger.py` — menggunakan `bleak` untuk berlangganan notifikasi BLE dan menyimpan CSV.

1. Install dependensi Python:
```bash
python -m pip install -r logs/requirements.txt
```
2. Ubah alamat MAC `ADDRESS` dan `CHAR_UUID` di bagian atas `logs/SerialLogger.py` jika diperlukan (atau gunakan aplikasi BLE scanner untuk menemukan device/characteristic).
3. Jalankan skrip:
```bash
python logs/SerialLogger.py
```
4. Output akan dibuat di folder `logs/YYYYMMDD_HHMMSS/data_log.csv`.

Catatan implementasi / tips
--------------------------
- BLE: firmware menggunakan BLE GATT notifications. Jika Anda ingin mengubah UUID menjadi unik, ganti `SERVICE_UUID` dan `CHARACTERISTIC_UUID` ke UUID 128-bit.
- Jika ingin mengatur kalibrasi lewat BLE, saya bisa menambahkan characteristic `WRITE` untuk menerima parameter kalibrasi dan menyimpannya ke NVS.
- File `logs/` saat ini di-committed. Untuk mencegah log lokal ikut ter-commit di masa depan, tambahkan aturan `.gitignore` seperti:

```
# ignore runtime logs
logs/*/data_log.csv
logs/*/
```


# Current-Logger-INA226-ESP32C3