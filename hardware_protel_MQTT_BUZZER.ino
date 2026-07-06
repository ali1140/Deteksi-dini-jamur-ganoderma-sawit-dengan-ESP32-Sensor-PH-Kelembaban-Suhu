#include <MAX6675.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_ADS1X15.h>
#include <WiFiManager.h>

const char* server_mqtt = "20.5.160.109";
const char* topik_mqtt = "sensor/data/csv_raw";
String id_sensor = "SENSOR001";

WiFiClient klien_esp;
PubSubClient klien_mqtt(klien_esp);

//MAX6675
#define pin_cs_max6675        15
#define pin_clk_max6675       14
#define pin_miso_max6675      12
MAX6675 thermoCouple(pin_cs_max6675, pin_miso_max6675, pin_clk_max6675);
//SD CARD
#define pin_cs_sd             5
#define pin_mosi_sd           23
#define pin_miso_sd           19
#define pin_clk_sd            18
SPIClass spi_sd(VSPI);
//RTC
#define pin_sda_i2c           21
#define pin_scl_i2c           22
TwoWire kabel_rtc(0);
RTC_DS3231 rtc;
//gps
#define pin_rx_gps            17
#define pin_tx_gps            16
#define baud_gps              9600
HardwareSerial serial_gps(2);
TinyGPSPlus gps;
//PIN LAIN
#define pin_kelembaban        35
#define pin_pemicu_log        27
#define pin_pemicu_kirim      26
#define pin_buzzer            33
#define pin_led_wifi          25
#define pin_baterai           34
#define pin_led_baterai_rendah 2
#define pin_led_baterai_penuh  4

// Sensor pH (ADS1115)
Adafruit_ADS1115 ads;
const float SHUNT_RESISTANCE_OHM = 0.99;
const int MOVING_AVG_SIZE_PH = 15;
float readings_ph[MOVING_AVG_SIZE_PH];
int readIndex_ph = 0;
float total_ph = 0;
float nilai_ph_stabil = 0.0;

//KALIBRASI
const int nilai_kering = 2923;
const int nilai_basah = 1104;
const float tegangan_baterai_penuh = 4.2;
const float tegangan_baterai_kosong = 3.2;

const unsigned long WAKTU_STABILISASI_SENSOR = 10000;
bool sensorSudahStabil = false;

bool mode_offline = false;
int status_tombol_log = HIGH, status_terakhir_tombol_log = HIGH;
int status_tombol_kirim = HIGH, status_terakhir_tombol_kirim = HIGH;
unsigned long waktu_terakhir_debounce = 0;
unsigned long jeda_debounce = 50;
unsigned long waktu_terakhir_cek_baterai = 0;
const unsigned long interval_cek_baterai = 100;

void perbarui_led_baterai(int persen) {
  digitalWrite(pin_led_baterai_rendah, persen < 50);
  digitalWrite(pin_led_baterai_penuh, persen >= 50);
}
void bunyikan_sekali() { 
  digitalWrite(pin_buzzer, HIGH); 
  delay(150); 
  digitalWrite(pin_buzzer, LOW); 
  }
void bunyikan_error()  { 
  digitalWrite(pin_buzzer, HIGH); 
  delay(500); 
  digitalWrite(pin_buzzer, LOW);
  }
void bunyikan_dua_kali() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(pin_buzzer, HIGH); 
    delay(100); 
    digitalWrite(pin_buzzer, LOW); 
    delay(100);
  }
}
void bunyikan_sinkronisasi_selesai() {
  tone(pin_buzzer, 880, 100); delay(120);
  tone(pin_buzzer, 1047, 150);
}
void hubungkan_ke_wifi() {
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  Serial.println("Mencoba menghubungkan ke WiFi...");
  if (!wm.autoConnect("SAKTI")) {
    Serial.println("Gagal terhubung. Mode Offline.");
    digitalWrite(pin_led_wifi, LOW);
    mode_offline = true;
  } else {
    Serial.println("WiFi Terhubung.");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    digitalWrite(pin_led_wifi, HIGH);
    mode_offline = false;
  }
}
void hubungkan_ulang_mqtt() {
  if (WiFi.status() != WL_CONNECTED) return;
  while (!klien_mqtt.connected()) {
    Serial.print("Menghubungkan ke MQTT...");
    if (klien_mqtt.connect("ESP32-KlienSensor")) {
      Serial.println("Terhubung.");
    } else {
      Serial.print("Gagal, kode error=");
      Serial.print(klien_mqtt.state());
      Serial.println(" Coba lagi 2 detik");
      delay(2000);
      break;
    }
  }
}
int baca_persentase_baterai() {
  int nilai_adc = analogRead(pin_baterai);
  float tegangan = (nilai_adc / 4095.0) * 3.3 * 2.0;
  float persen = ((tegangan - tegangan_baterai_kosong) / (tegangan_baterai_penuh - tegangan_baterai_kosong)) * 100.0;
  return constrain(persen, 0, 100);
}
float baca_suhu() { thermoCouple.read(); return thermoCouple.getCelsius(); }
String baca_tanggal_waktu() {
  DateTime sekarang = rtc.now();
  char buffer[25];
  sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d", sekarang.year(), sekarang.month(), sekarang.day(), sekarang.hour(), sekarang.minute(), sekarang.second());
  return String(buffer);
}
float baca_kelembaban_tanah() {
  int nilai = analogRead(pin_kelembaban);
  return constrain(map(nilai, nilai_kering, nilai_basah, 0, 100), 0, 100);
}
String baca_gps() {
  return gps.location.isValid() ? String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6) : "-000000,-000001";
}
float update_dan_baca_ph() {
  int16_t raw_adc = ads.readADC_Differential_0_1();
  float volt = (raw_adc * 0.256) / 32767.0;
  float amps = volt / SHUNT_RESISTANCE_OHM;
  float raw_pH = (amps <= 0) ? 0.0 : (amps <= 0.001 ? 5000.0 * amps : 1000.0 * amps + 4.0);

  total_ph -= readings_ph[readIndex_ph];
  readings_ph[readIndex_ph] = raw_pH;
  total_ph += raw_pH;
  readIndex_ph = (readIndex_ph + 1) % MOVING_AVG_SIZE_PH;
  return total_ph / MOVING_AVG_SIZE_PH;
}
void buat_file_log_dengan_header() {
  Serial.println("File log.csv tidak ditemukan, membuat file baru...");
  File file = SD.open("/log.csv", FILE_WRITE);
  if (file) { file.close(); Serial.println("  File berhasil dibuat."); }
  else Serial.println("  Gagal membuat file log.csv.");
}
void simpan_ke_sd(const String& baris) {
  if (!SD.exists("/log.csv")) buat_file_log_dengan_header();
  File file = SD.open("/log.csv", FILE_APPEND);
  if (file) { file.println(baris); file.close(); }
  else Serial.println("Gagal membuka log.csv.");
}
void kirim_csv_ke_mqtt() {
  if (mode_offline || !klien_mqtt.connected()) {
    Serial.println("Offline atau MQTT tidak terhubung.");
    bunyikan_error();
    return;
  }
  File file = SD.open("/log.csv", FILE_READ);
  if (!file || file.size() == 0) {
    Serial.println("File kosong atau tidak ada.");
    file.close();
    return;
  }

  bool sukses = true;
  Serial.println("Mengirim data CSV ke MQTT...");
  while (file.available()) {
    String baris = file.readStringUntil('\n');
    baris.trim();
    if (baris.length() > 0) {
      if (klien_mqtt.publish(topik_mqtt, baris.c_str())) {
        Serial.print("  Kirim: "); Serial.println(baris);
        bunyikan_dua_kali();
      } else {
        Serial.print("  Gagal: "); Serial.println(baris);
        sukses = false;
      }
      delay(200);
    }
  }
  file.close();

  if (sukses) {
    if (SD.remove("/log.csv")) {
      Serial.println("  File log.csv dihapus.");
      bunyikan_sinkronisasi_selesai();
    } else {
      Serial.println("  Gagal hapus log.csv.");
      bunyikan_error();
    }
  } else Serial.println("Sebagian data gagal terkirim.");
}

void setup() {
  delay(100);
  Serial.begin(115200);
  Serial.println("\n--- Setup Dimulai ---");

  pinMode(pin_kelembaban, INPUT);
  pinMode(pin_pemicu_log, INPUT);
  pinMode(pin_pemicu_kirim, INPUT);
  pinMode(pin_led_wifi, OUTPUT);
  pinMode(pin_buzzer, OUTPUT);
  pinMode(pin_led_baterai_rendah, OUTPUT);
  pinMode(pin_led_baterai_penuh, OUTPUT);

  digitalWrite(pin_buzzer, LOW);
  digitalWrite(pin_led_wifi, LOW);
  digitalWrite(pin_led_baterai_rendah, LOW);
  digitalWrite(pin_led_baterai_penuh, LOW);

  spi_sd.begin(pin_clk_sd, pin_miso_sd, pin_mosi_sd, -1);
  if (!SD.begin(pin_cs_sd, spi_sd)) { Serial.println("SD Card Gagal!"); while (1); }

  kabel_rtc.begin(pin_sda_i2c, pin_scl_i2c);
  if (!rtc.begin(&kabel_rtc)) { Serial.println("RTC Gagal!"); while (1); }
  if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  ads.setGain(GAIN_SIXTEEN);
  if (!ads.begin(ADS1X15_ADDRESS, &kabel_rtc)) { Serial.println("ADS1115 tidak ditemukan!"); while (1); }
  memset(readings_ph, 0, sizeof(readings_ph));

  serial_gps.begin(baud_gps, SERIAL_8N1, pin_rx_gps, pin_tx_gps);

  if (!SD.exists("/log.csv")) buat_file_log_dengan_header();

  hubungkan_ke_wifi();
  if (!mode_offline) {
    klien_mqtt.setServer(server_mqtt, 1883);
    hubungkan_ulang_mqtt();
    if (klien_mqtt.connected()) kirim_csv_ke_mqtt();
  }

  thermoCouple.begin();
  thermoCouple.setSPIspeed(4000000);
  perbarui_led_baterai(baca_persentase_baterai());

  Serial.print("--- Setup selesai, stabilisasi sensor ");
  Serial.print(WAKTU_STABILISASI_SENSOR / 1000);
  Serial.println(" detik ---");
}

// ---------- LOOP ----------
void loop() {
  while (serial_gps.available()) gps.encode(serial_gps.read());

  if (!mode_offline) {
    if (!klien_mqtt.connected()) hubungkan_ulang_mqtt();
    klien_mqtt.loop();
  }

  if (!mode_offline && WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi terputus. Mode Offline.");
    mode_offline = true;
    digitalWrite(pin_led_wifi, LOW);
  }

  if (mode_offline && WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi kembali. Mode Online.");
    mode_offline = false;
    digitalWrite(pin_led_wifi, HIGH);
    hubungkan_ulang_mqtt();
    if (klien_mqtt.connected()) kirim_csv_ke_mqtt();
  }

  if (!sensorSudahStabil && millis() >= WAKTU_STABILISASI_SENSOR) {
    sensorSudahStabil = true;
    Serial.println("--- Stabilisasi Selesai ---");
  } else if (sensorSudahStabil) {
    nilai_ph_stabil = update_dan_baca_ph();
  }

  if (millis() - waktu_terakhir_cek_baterai >= interval_cek_baterai) {
    waktu_terakhir_cek_baterai = millis();
    perbarui_led_baterai(baca_persentase_baterai());
  }

  int tombol_log = digitalRead(pin_pemicu_log);
  if (tombol_log != status_terakhir_tombol_log) waktu_terakhir_debounce = millis();
  if ((millis() - waktu_terakhir_debounce) > jeda_debounce) {
    if (tombol_log != status_tombol_log) {
      status_tombol_log = tombol_log;
      if (status_tombol_log == HIGH) {
        if (!sensorSudahStabil) {
          Serial.println("Sensor belum stabil.");
          bunyikan_error();
        } else {
          float suhu = baca_suhu();
          String waktu = baca_tanggal_waktu();
          float kelembaban = baca_kelembaban_tanah();
          String gps_data = baca_gps();
          float ph_log = nilai_ph_stabil;
          String baris = id_sensor + "," + (isnan(suhu) ? "ERR_SUHU" : String(suhu, 2)) + "," +
                         kelembaban + "," + (isnan(ph_log) ? "ERR_PH" : String(ph_log, 2)) + "," +
                         gps_data + "," + waktu;
          simpan_ke_sd(baris);
          Serial.println("Data disimpan: " + baris);
          bunyikan_sekali();
        }
      }
    }
  }
  status_terakhir_tombol_log = tombol_log;

  int tombol_kirim = digitalRead(pin_pemicu_kirim);
  if (tombol_kirim != status_terakhir_tombol_kirim) waktu_terakhir_debounce = millis();
  if ((millis() - waktu_terakhir_debounce) > jeda_debounce) {
    if (tombol_kirim != status_tombol_kirim) {
      status_tombol_kirim = tombol_kirim;
      if (status_tombol_kirim == HIGH) {
        if (mode_offline) {
          Serial.println("Offline. Tidak bisa kirim.");
          bunyikan_error();
        } else {
          Serial.println("Kirim data MQTT...");
          kirim_csv_ke_mqtt();
        }
      }
    }
  }
  status_terakhir_tombol_kirim = tombol_kirim;

  delay(100);
}
