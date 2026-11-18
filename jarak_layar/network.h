#pragma once
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <sys/time.h>
#include <time.h>
#include "config.h"
#include "secrets.h"

// Jaringan berjalan di task terpisah (core 0) supaya reconnect/TLS yang lambat
// tidak pernah menghambat logika edge dan alarm (core 1).

struct NetMsg { uint8_t kind; char payload[MSG_MAX]; };   // kind 0 = telemetri, 1 = atribut

static QueueHandle_t   netQ;
static WiFiClientSecure tlsClient;
static PubSubClient     mqtt(tlsClient);
static volatile bool     netConnected = false;
static volatile uint32_t netPublished = 0;

inline bool netIsConnected()      { return netConnected; }
inline uint32_t netPublishedCount() { return netPublished; }
inline uint32_t netPending()      { return netQ ? uxQueueMessagesWaiting(netQ) : 0; }

// Waktu epoch (ms). 0 bila NTP belum sinkron.
inline uint64_t netNowMs() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  if (tv.tv_sec < 1700000000) return 0;
  return (uint64_t)tv.tv_sec * 1000ULL + tv.tv_usec / 1000;
}

// Masuk antrean. Bila penuh, data TERLAMA dibuang (buffer melingkar).
inline void netEnqueue(uint8_t kind, const char* payload) {
  NetMsg m; m.kind = kind;
  strlcpy(m.payload, payload, sizeof(m.payload));
  if (xQueueSend(netQ, &m, 0) != pdTRUE) {
    NetMsg old; xQueueReceive(netQ, &old, 0);
    xQueueSend(netQ, &m, 0);
  }
}

// values = isi JSON tanpa kurung kurawal, contoh: "\"lux\":120.5,\"zone\":\"near\""
inline void netSendTelemetry(const char* values) {
  char out[MSG_MAX];
  uint64_t ts = netNowMs();
  if (ts) snprintf(out, sizeof out, "{\"ts\":%llu,\"values\":{%s}}", (unsigned long long)ts, values);
  else    snprintf(out, sizeof out, "{%s}", values);   // NTP belum siap: server yang memberi cap waktu
  netEnqueue(0, out);
}

inline void netSendAttributes(const char* json) { netEnqueue(1, json); }

static void netTask(void*) {
  uint32_t lastWifi = 0, lastMqtt = 0;
  bool ntpStarted = false;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  lastWifi = millis();

  for (;;) {
    uint32_t now = millis();

    if (WiFi.status() != WL_CONNECTED) {
      netConnected = false;
      if (now - lastWifi > 20000) {
        Serial.println(F("[NET] WiFi putus, mencoba lagi..."));
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        lastWifi = now;
      }
      vTaskDelay(pdMS_TO_TICKS(250));
      continue;
    }

    if (!ntpStarted) {
      configTime(0, 0, "pool.ntp.org", "time.google.com");
      ntpStarted = true;
      Serial.println(F("[NET] WiFi OK, sinkron NTP..."));
    }
    if (netNowMs() == 0) { vTaskDelay(pdMS_TO_TICKS(250)); continue; }   // TLS butuh jam benar

    if (!mqtt.connected()) {
      netConnected = false;
      if (lastMqtt == 0 || now - lastMqtt > 5000) {
        lastMqtt = now;
        Serial.println(F("[NET] Menyambung MQTT TLS 8883..."));
        if (mqtt.connect(MQTT_CLIENT_ID, TB_TOKEN, NULL)) {
          Serial.println(F("[NET] MQTT tersambung"));
          netConnected = true;
        } else {
          char e[100] = "";
          tlsClient.lastError(e, sizeof e);
          Serial.printf("[NET] MQTT gagal state=%d tls='%s'\n", mqtt.state(), e);
        }
      }
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    netConnected = true;
    NetMsg m; uint8_t n = 0;
    while (n < 10 && xQueueReceive(netQ, &m, 0) == pdTRUE) {
      bool ok = mqtt.publish(m.kind == 0 ? TELEMETRY_TOPIC : ATTRIBUTES_TOPIC, m.payload);
      if (!ok) { xQueueSendToFront(netQ, &m, 0); break; }   // gagal: kembalikan, kirim ulang nanti
      netPublished++; n++;
    }
    mqtt.loop();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

inline void netBegin() {
  netQ = xQueueCreate(QUEUE_LEN, sizeof(NetMsg));
  tlsClient.setCACert(ROOT_CA);          // verifikasi CA. JANGAN pakai setInsecure().
  tlsClient.setTimeout(10);
  mqtt.setServer(TB_HOST, MQTT_PORT);
  mqtt.setBufferSize(512);
  mqtt.setKeepAlive(30);
  mqtt.setSocketTimeout(10);
  xTaskCreatePinnedToCore(netTask, "net", 12288, NULL, 1, NULL, 0);
}
