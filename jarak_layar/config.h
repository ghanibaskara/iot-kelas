#pragma once
#include <Arduino.h>

// ================= MODE =================
#define DEMO_MODE      1     // 1 = T pendek (30 dtk) untuk demo, 0 = produksi (300 dtk)
#define TEST_RAW_MODE  0     // 1 = kirim TIAP SAMPEL (bahan tabel perbandingan pesan "tanpa edge")
#define FW_VERSION     "1.0.0"

// ================= PIN =================
#define PIN_TRIG    5
#define PIN_ECHO    18      // lewat pembagi tegangan 1K/2K
#define PIN_SDA     21
#define PIN_SCL     22
#define PIN_LED     25
#define PIN_BUZZER  26
#define PIN_BTN     27      // INPUT_PULLUP, sisi lain ke GND

// ================= SENSOR =================
const uint32_t SONAR_INTERVAL_MS = 200;     // 5 Hz
const uint32_t LUX_INTERVAL_MS   = 3000;
const uint8_t  MEDIAN_WINDOW     = 5;
const float    SONAR_MIN_CM      = 2.0f;
const float    SONAR_MAX_CM      = 400.0f;
const uint32_t ECHO_TIMEOUT_US   = 30000;

// ================= ZONA =================
const float PRESENT_MAX_CM        = 150.0f; // lebih jauh dari ini = tidak hadir
const float DEFAULT_NEAR_ENTER_CM = 50.0f;
const float DEFAULT_NEAR_EXIT_CM  = 60.0f;
const float CAL_ENTER_RATIO       = 0.70f;  // hasil kalibrasi: masuk = 70% jarak normal
const float CAL_EXIT_RATIO        = 0.85f;  // keluar = 85% jarak normal
const float CAL_MIN_CM            = 30.0f;  // jarak normal minimal agar kalibrasi diterima

// ================= SKOR (leaky bucket) =================
const float SCORE_UP     = 1.0f;     // per detik di zona dekat
const float SCORE_DOWN   = 0.5f;     // per detik di luar zona dekat
const float DARK_LUX     = 50.0f;
const float DARK_FACTOR  = 1.5f;
const float WARN_RATIO   = 0.5f;     // LED menyala di 0,5 T
const float SCORE_CAP_RATIO = 1.5f;  // skor dibatasi 1,5 T supaya tidak menumpuk tanpa batas
#if DEMO_MODE
const float    ALARM_T   = 30.0f;
const uint32_t SNOOZE_MS = 30000;
#else
const float    ALARM_T   = 300.0f;
const uint32_t SNOOZE_MS = 300000;
#endif

// ================= TOMBOL =================
const uint32_t DEBOUNCE_MS   = 40;
const uint32_t LONG_PRESS_MS = 3000;

// ================= RINGKASAN =================
const uint32_t SUMMARY_MS    = 60000;
const uint8_t  ALARM_HISTORY = 16;   // riwayat waktu alarm untuk hitung "alarm 1 jam"

// ================= JARINGAN =================
#define MQTT_PORT         8883
#define MQTT_CLIENT_ID    "esp32-jarak-layar"
#define TELEMETRY_TOPIC   "v1/devices/me/telemetry"
#define ATTRIBUTES_TOPIC  "v1/devices/me/attributes"
const uint8_t  QUEUE_LEN = 40;       // buffer luring (data terlama dibuang bila penuh)
#define MSG_MAX 256
