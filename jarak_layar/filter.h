#pragma once
#include "config.h"

// Filter median. Bacaan tidak valid (-1) diabaikan; butuh minimal 3 bacaan valid.
struct MedianFilter {
  float   buf[MEDIAN_WINDOW];
  uint8_t idx = 0, n = 0;

  float push(float v) {
    buf[idx] = v;
    idx = (idx + 1) % MEDIAN_WINDOW;
    if (n < MEDIAN_WINDOW) n++;

    float tmp[MEDIAN_WINDOW];
    uint8_t c = 0;
    for (uint8_t i = 0; i < n; i++) if (buf[i] >= 0) tmp[c++] = buf[i];
    if (c < 3) return -1.0f;

    for (uint8_t i = 1; i < c; i++) {            // insertion sort
      float k = tmp[i]; int8_t j = i - 1;
      while (j >= 0 && tmp[j] > k) { tmp[j + 1] = tmp[j]; j--; }
      tmp[j + 1] = k;
    }
    return tmp[c / 2];
  }
};
