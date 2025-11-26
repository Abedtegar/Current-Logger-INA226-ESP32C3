# Current Logger

Deskripsi
- `SerialLogger.py` menerima data CSV dari perangkat melalui BLE, menyimpan setiap baris ke file CSV, dan menampilkan plot real-time dari beberapa seri (bus voltage, shunt mV, current mA, power mW).
- Saat program dijalankan, sebuah folder baru dibuat di `logs/YYYYMMDD_HHMMSS/` dan file `data_log.csv` disimpan di dalamnya.

Dependensi
- Python 3.8+ (disarankan 3.10/3.11)
- Paket Python yang diperlukan tercantum di `requirements.txt`:
  - `bleak` (BLE client)
  - `matplotlib` (plotting)

Instalasi (PowerShell)
```powershell
# (opsional) buat virtual environment
python -m venv .venv
.\.venv\Scripts\Activate.ps1

# instal dependensi
python -m pip install -r .\requirements.txt
```

Menjalankan
```powershell
# jalankan script
python .\SerialLogger.py
```

Lokasi log
- Log tersimpan di: `logs/<YYYYMMDD_HHMMSS>/data_log.csv`.
- CSV berisi header: `device_timestamp, pc_timestamp, bus_voltage, shunt_voltage_mV, current_mA, power_mW`.

Catatan teknis
- `ADDRESS` dan `CHAR_UUID` ditetapkan di `SerialLogger.py` — ubah sesuai perangkat Anda bila perlu.
- Script mencoba melakukan reconnect otomatis jika koneksi BLE gagal atau terputus.
- Ploting menggunakan `pc_timestamp` (waktu PC saat data diterima) sebagai sumbu-x.

Troubleshooting
- Jika muncul error `ModuleNotFoundError: No module named 'matplotlib'`, jalankan `python -m pip install -r .\requirements.txt`.
- Jika BLE tidak terkoneksi: periksa `ADDRESS`/`CHAR_UUID`, pastikan Bluetooth diaktifkan dan perangkat dalam jangkauan.

Opsi lanjutan (bisa ditambahkan)
- Argparse untuk `--address`, `--char`, `--out-dir`, `--no-plot`.
- Simpan snapshot plot berkala ke file PNG.

Jika Anda mau, saya bisa menambahkan CLI (`argparse`) supaya pengaturan tidak perlu diedit langsung di file.