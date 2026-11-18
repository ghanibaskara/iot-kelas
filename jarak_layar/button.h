#pragma once
#include "config.h"

enum BtnEvent { BTN_NONE, BTN_SHORT, BTN_LONG };

// Event dikembalikan saat tombol DILEPAS. Tahan >= 3 detik = LONG (kalibrasi).
inline BtnEvent buttonUpdate(uint32_t nowMs) {
  static bool stable = HIGH, lastRead = HIGH;
  static uint32_t lastChange = 0, pressT = 0;

  bool r = digitalRead(PIN_BTN);
  if (r != lastRead) { lastRead = r; lastChange = nowMs; }

  if ((nowMs - lastChange) >= DEBOUNCE_MS && r != stable) {
    stable = r;
    if (stable == LOW) {
      pressT = nowMs;
    } else {
      return ((nowMs - pressT) >= LONG_PRESS_MS) ? BTN_LONG : BTN_SHORT;
    }
  }
  return BTN_NONE;
}
