#pragma once
#include <Preferences.h>
#include "config.h"

enum Zone { ZONE_AWAY, ZONE_SAFE, ZONE_NEAR };
inline const char* zoneName(Zone z) {
  return z == ZONE_NEAR ? "near" : (z == ZONE_SAFE ? "safe" : "away");
}

struct EdgeState {
  // ---- status ----
  bool  present = false, nearZone = false;
  bool  warn = false, alarm = false, darkNear = false, snoozed = false;
  float score = 0;
  float enterCm = DEFAULT_NEAR_ENTER_CM, exitCm = DEFAULT_NEAR_EXIT_CM;
  uint32_t snoozeUntil = 0;
  bool  alarmStarted = false;           // flag untuk main (dibaca lalu dihapus)

  // ---- penghitung jendela ringkasan ----
  uint32_t presentS = 0, nearS = 0, darkNearS = 0, alarmsWindow = 0, alarmTotal = 0;
  float    luxSum = 0;  uint32_t luxN = 0;
  uint32_t alarmTimes[ALARM_HISTORY];   // detik uptime saat alarm, UINT32_MAX = kosong
  uint8_t  alarmIdx = 0;

  EdgeState() { for (uint8_t i = 0; i < ALARM_HISTORY; i++) alarmTimes[i] = UINT32_MAX; }

  void load() {
    Preferences p; p.begin("jarak", true);
    enterCm = p.getFloat("enter", DEFAULT_NEAR_ENTER_CM);
    exitCm  = p.getFloat("exit",  DEFAULT_NEAR_EXIT_CM);
    p.end();
  }

  // Kalibrasi dari jarak duduk normal. false bila jarak tidak valid.
  bool calibrate(float normalCm) {
    if (normalCm < CAL_MIN_CM || normalCm > PRESENT_MAX_CM) return false;
    enterCm = normalCm * CAL_ENTER_RATIO;
    exitCm  = normalCm * CAL_EXIT_RATIO;
    Preferences p; p.begin("jarak", false);
    p.putFloat("enter", enterCm);
    p.putFloat("exit",  exitCm);
    p.end();
    return true;
  }

  // Dipanggil tiap sampel jarak (sudah difilter median; -1 = tidak valid)
  void updateZone(float dist) {
    if (dist < 0 || dist > PRESENT_MAX_CM) { present = false; nearZone = false; return; }
    present = true;
    if (!nearZone && dist < enterCm)      nearZone = true;
    else if (nearZone && dist > exitCm)   nearZone = false;
  }

  Zone zone() const { return !present ? ZONE_AWAY : (nearZone ? ZONE_NEAR : ZONE_SAFE); }

  void snooze(uint32_t nowMs) { snoozeUntil = nowMs + SNOOZE_MS; snoozed = true; alarm = false; }

  // Dipanggil tepat sekali per detik
  void tick(float lux, uint32_t nowMs) {
    snoozed  = (int32_t)(snoozeUntil - nowMs) > 0;
    darkNear = nearZone && lux >= 0 && lux < DARK_LUX;

    if (present) presentS++;
    if (nearZone) {
      nearS++;
      score += SCORE_UP * (darkNear ? DARK_FACTOR : 1.0f);
      if (darkNear) darkNearS++;
    } else {
      score -= SCORE_DOWN;
      if (score < 0) score = 0;
    }
    if (score > ALARM_T * SCORE_CAP_RATIO) score = ALARM_T * SCORE_CAP_RATIO;
    if (lux >= 0) { luxSum += lux; luxN++; }

    warn = score >= ALARM_T * WARN_RATIO;
    bool nowAlarm = nearZone && score >= ALARM_T && !snoozed;
    if (nowAlarm && !alarm) {
      alarmStarted = true; alarmTotal++; alarmsWindow++;
      alarmTimes[alarmIdx] = nowMs / 1000;
      alarmIdx = (alarmIdx + 1) % ALARM_HISTORY;
    }
    alarm = nowAlarm;
  }

  uint8_t alarmsLastHour(uint32_t nowS) const {
    uint8_t c = 0;
    for (uint8_t i = 0; i < ALARM_HISTORY; i++)
      if (alarmTimes[i] != UINT32_MAX && nowS - alarmTimes[i] <= 3600) c++;
    return c;
  }

  void resetWindow() { presentS = nearS = darkNearS = alarmsWindow = 0; luxSum = 0; luxN = 0; }
};
