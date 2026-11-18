#pragma once
#include "config.h"

static uint32_t blipUntil = 0;

inline void actuatorBegin() {
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  digitalWrite(PIN_BUZZER, LOW);
}

// Bunyi pendek sebagai umpan balik (snooze, kalibrasi)
inline void actuatorBlip(uint32_t ms, uint32_t nowMs) { blipUntil = nowMs + ms; }

inline void actuatorUpdate(bool warn, bool alarm, uint32_t nowMs) {
  digitalWrite(PIN_LED, (warn || alarm) ? HIGH : LOW);
  bool buz = false;
  if (alarm) buz = (nowMs % 1000) < 300;          // bip 300 ms tiap 1 detik
  if ((int32_t)(blipUntil - nowMs) > 0) buz = true;
  digitalWrite(PIN_BUZZER, buz ? HIGH : LOW);
}
