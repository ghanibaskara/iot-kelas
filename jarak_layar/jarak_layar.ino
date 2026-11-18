/*
 * Pengingat Jarak Layar - Edge IoT (ESP32)
 * CIT60029 Internet of Things, Universitas Brawijaya
 * Pengingat kebiasaan jarak saja, tanpa klaim manfaat kesehatan.
 */
#include "config.h"
#include "secrets.h"
#include "sensor.h"
#include "filter.h"
#include "state.h"
#include "actuator.h"
#include "button.h"
#include "network.h"

static MedianFilter medFilter;
static EdgeState    st;
static float  distCm = -1, luxNow = -1;
static Zone   prevZone = ZONE_AWAY;
static bool   wasNetConn = false;
static uint32_t tSonar, tLux, tTick, tSummary;

// ---------- pengiriman ----------
static void sendEvent(const char* name) {
  if (TEST_RAW_MODE) return;
  char v[96];
  snprintf(v, sizeof v, "\"event\":\"%s\",\"score\":%.1f", name, st.score);
  netSendTelemetry(v);
}

static void sendAttributes() {
  char a[200];
  snprintf(a, sizeof a,
    "{\"fw_version\":\"%s\",\"alarm_T\":%.0f,\"near_enter_cm\":%.1f,\"near_exit_cm\":%.1f,\"mode\":\"%s\"}",
    FW_VERSION, ALARM_T, st.enterCm, st.exitCm, DEMO_MODE ? "demo" : "normal");
  netSendAttributes(a);
}

static void sendSummary(uint32_t nowMs) {
  if (TEST_RAW_MODE) return;
  char v[MSG_MAX - 48];
  size_t o = 0;
  o += snprintf(v + o, sizeof v - o,
    "\"zone\":\"%s\",\"score\":%.1f,\"present_s\":%u,\"near_s\":%u,\"alarm_count\":%u,"
    "\"alarm_total\":%u,\"alarms_1h\":%u,\"dark_near_s\":%u,\"dark_near\":%d",
    zoneName(st.zone()), st.score, (unsigned)st.presentS, (unsigned)st.nearS,
    (unsigned)st.alarmsWindow, (unsigned)st.alarmTotal,
    (unsigned)st.alarmsLastHour(nowMs / 1000), (unsigned)st.darkNearS, st.darkNear ? 1 : 0);
  if (distCm >= 0 && o < sizeof v)
    o += snprintf(v + o, sizeof v - o, ",\"distance_cm\":%.1f", distCm);
  if (st.luxN > 0 && o < sizeof v)
    o += snprintf(v + o, sizeof v - o, ",\"lux\":%.1f", st.luxSum / st.luxN);
  netSendTelemetry(v);
}

// ---------- setup ----------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== Pengingat Jarak Layar v%s | demo=%d raw_test=%d T=%.0f ===\n",
                FW_VERSION, DEMO_MODE, TEST_RAW_MODE, ALARM_T);

  actuatorBegin();
  pinMode(PIN_BTN, INPUT_PULLUP);
  sensorBegin();
  st.load();
  Serial.printf("[CAL] ambang masuk=%.1f cm, keluar=%.1f cm\n", st.enterCm, st.exitCm);

  // self-test singkat: LED + bip
  digitalWrite(PIN_LED, HIGH); digitalWrite(PIN_BUZZER, HIGH); delay(150);
  digitalWrite(PIN_BUZZER, LOW); delay(150); digitalWrite(PIN_LED, LOW);

  netBegin();
  tSonar = tLux = tTick = tSummary = millis();
}

// ---------- loop (logika edge, tidak bergantung jaringan) ----------
void loop() {
  uint32_t now = millis();

  // 1) Tombol
  BtnEvent b = buttonUpdate(now);
  if (b == BTN_SHORT) {
    st.snooze(now);
    actuatorBlip(80, now);
    sendEvent("snooze");
    Serial.printf("[BTN] snooze %lu dtk\n", (unsigned long)(SNOOZE_MS / 1000));
  } else if (b == BTN_LONG) {
    if (st.calibrate(distCm)) {
      actuatorBlip(400, now);
      sendEvent("calibrate");
      sendAttributes();
      Serial.printf("[CAL] normal=%.1f cm -> masuk=%.1f keluar=%.1f\n", distCm, st.enterCm, st.exitCm);
    } else {
      Serial.println(F("[CAL] ditolak: jarak tidak valid (harus 30-150 cm dan terbaca stabil)"));
    }
  }

  // 2) Jarak (5 Hz)
  if (now - tSonar >= SONAR_INTERVAL_MS) {
    tSonar = now;
    float raw = sonarReadCm();
    distCm = medFilter.push(raw);
    st.updateZone(distCm);

    Zone z = st.zone();
    if (z != prevZone) {
      prevZone = z;
      char nm[20]; snprintf(nm, sizeof nm, "zone_%s", zoneName(z));
      sendEvent(nm);
    }
#if TEST_RAW_MODE
    char v[48]; snprintf(v, sizeof v, "\"distance_cm\":%.1f", raw);
    netSendTelemetry(v);                         // tanpa edge: setiap sampel dikirim
#endif
  }

  // 3) Lux
  if (now - tLux >= LUX_INTERVAL_MS) { tLux = now; luxNow = luxRead(); }

  // 4) Skor & alarm (1 Hz)
  if (now - tTick >= 1000) {
    tTick += 1000;
    st.tick(luxNow, now);
    if (st.alarmStarted) {
      st.alarmStarted = false;
      sendEvent("alarm");
      Serial.println(F("[ALARM] buzzer berbunyi"));
    }
    Serial.printf("[%lus] d=%.1f lux=%.0f zona=%s skor=%.1f warn=%d alarm=%d snooze=%d net=%d antre=%u\n",
                  (unsigned long)(now / 1000), distCm, luxNow, zoneName(st.zone()), st.score,
                  st.warn, st.alarm, st.snoozed, netIsConnected(), (unsigned)netPending());
  }

  // 5) Ringkasan periodik (60 dtk)
  if (now - tSummary >= SUMMARY_MS) {
    tSummary += SUMMARY_MS;
    sendSummary(now);
    st.resetWindow();
    Serial.printf("[MSG] total terkirim=%u, menunggu di buffer=%u\n",
                  (unsigned)netPublishedCount(), (unsigned)netPending());
  }

  // 6) Atribut saat MQTT (re)connect
  bool nc = netIsConnected();
  if (nc && !wasNetConn) sendAttributes();
  wasNetConn = nc;

  // 7) Aktuator
  actuatorUpdate(st.warn, st.alarm, now);
}
