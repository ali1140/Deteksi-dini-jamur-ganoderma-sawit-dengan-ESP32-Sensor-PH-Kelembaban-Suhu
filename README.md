# Alat Deteksi Dini Jamur Ganoderma pada Kelapa Sawit (IoT)

<p align="center">
  <img src="https://img.shields.io/badge/Board-ESP32-blue" alt="ESP32">
  <img src="https://img.shields.io/badge/Protocol-MQTT-orange" alt="MQTT">
  <img src="https://img.shields.io/badge/Language-C++_(Arduino)-green" alt="C++">
</p>

## Deskripsi Sistem
Sistem Embedded dan Internet of Things (IoT) ini dirancang menggunakan mikrokontroler **ESP32** untuk melakukan deteksi dini terhadap serangan jamur patogen *Ganoderma boninense* pada perkebunan kelapa sawit. Perangkat keras beroperasi secara portabel sebagai *datalogger* untuk mengukur parameter lingkungan secara *real-time* tepat di titik lokasi pengambilan sampel.

![Skema Perangkat](assets/Desain%20Hardware_page1_1.jpeg)

## Fitur Utama
- **Pengukuran Suhu & Kelembaban**: Memanfaatkan sensor *Thermocouple* MAX6675 untuk akurasi suhu tinggi dan sensor analog untuk kelembaban tanah.
- **Pengukuran pH Tanah**: Menggunakan sensor pH analog yang dikonversi melalui modul **ADS1115 (16-bit ADC)** untuk menjamin presisi pembacaan data.
- **Perekaman Lokasi Geografis (GPS) & Waktu**: Terintegrasi dengan modul GPS dan RTC (DS3231) sehingga setiap rekaman data memiliki presisi titik koordinat (latitude, longitude) serta stempel waktu (*timestamp*) yang valid.
- **Sistem Komunikasi MQTT (Online/Offline)**:
  - **Mode Online**: Mengirimkan data pembacaan sensor secara langsung ke broker MQTT (`20.5.160.109`) melalui topik `sensor/data/csv_raw`.
  - **Mode Offline (Datalogger)**: Jika perangkat berada di area tanpa jangkauan jaringan nirkabel, sistem secara otomatis menyimpan data pengukuran ke dalam **SD Card** (`log.csv`). Ketika koneksi nirkabel kembali tersedia, perangkat dapat melakukan sinkronisasi dengan mengirimkan seluruh *log* data tersebut secara *batch*.
- **Konfigurasi Jaringan Nirkabel Otomatis**: Dilengkapi dengan sistem *WiFiManager* sehingga pengguna tidak perlu melakukan penyesuaian kredensial WiFi di dalam kode (*hardcode*). Jika gagal terhubung, ESP32 akan memancarkan *Access Point* konfigurasi mandiri dengan SSID **"SAKTI"**.
- **Manajemen Daya**: Sistem memantau kapasitas baterai secara berkelanjutan serta memberikan indikator visual (LED) dan indikator audio (Buzzer) sebagai respons terhadap setiap interaksi atau peringatan sistem.

## Perangkat Keras (Hardware)
- Mikrokontroler: **ESP32**
- Modul Tambahan: Modul SD Card (SPI), RTC DS3231 (I2C), Modul GPS (UART), Indikator Buzzer & LED
- Spesifikasi Sensor: 
  - Temperatur: **MAX6675**
  - Kelembaban Tanah: Analog Moisture Sensor
  - Keasaman Tanah: Analog pH Sensor + **ADS1115**

## Dependensi Perangkat Lunak
Untuk melakukan kompilasi kode sumber, pastikan *library* berikut terinstal pada lingkungan pengembangan (Arduino IDE):
- `MAX6675.h`
- `SD.h`, `SPI.h`, `Wire.h`
- `RTClib` (oleh Adafruit)
- `TinyGPSPlus`
- `PubSubClient`
- `Adafruit_ADS1X15`
- `WiFiManager`

## Panduan Penggunaan Perangkat
1. **Inisialisasi**: Hidupkan perangkat. ESP32 akan melakukan inisialisasi sensor dan mencoba terhubung ke profil WiFi terakhir yang tersimpan. Jika koneksi gagal, sistem akan otomatis beralih ke Mode Offline.
2. **Pengambilan Data (Log)**: Tekan tombol *Log* (Pin 27) untuk memulai pembacaan sensor, koordinat GPS, dan waktu. Sistem akan memvalidasi data dan menyimpannya dalam format CSV ke dalam SD Card. Buzzer akan memberikan konfirmasi satu kali.
3. **Pengiriman Data (Sinkronisasi)**: Saat perangkat terhubung dengan jaringan internet, tekan tombol *Kirim* (Pin 26). ESP32 akan memproses seluruh isi file `log.csv` dan mengunggahnya secara sekuensial ke server melalui protokol MQTT. Setelah konfirmasi pengiriman berhasil, penyimpanan lokal akan dikosongkan dan buzzer akan memberikan sinyal konfirmasi sinkronisasi berhasil.

## Kontributor
- **ali1140** - *Developer Utama*

---
*Dokumentasi ini disusun untuk memberikan tinjauan teknis mengenai arsitektur sistem deteksi dini Ganoderma.*
