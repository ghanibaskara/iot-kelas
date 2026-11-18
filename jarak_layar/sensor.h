#pragma once
#include <Wire.h>
#include <BH1750.h>
#include "config.h"

static BH1750 lightMeter;
static bool luxOk = false;

inline void sensorBegin() {
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);
  Wire.begin(PIN_SDA, PIN_SCL);
  luxOk = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  Serial.println(luxOk ? F("[SENSOR] BH1750 OK")
                       : F("[SENSOR] BH1750 tidak terdeteksi, lux dinonaktifkan"));
}

// Mengembalikan jarak (cm), atau -1 bila tidak valid
inline float sonarReadCm() {
  digitalWrite(PIN_TRIG, LOW);  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long us = pulseIn(PIN_ECHO, HIGH, ECHO_TIMEOUT_US);
  if (us == 0) return -1.0f;
  float cm = us * 0.0343f / 2.0f;
  if (cm < SONAR_MIN_CM || cm > SONAR_MAX_CM) return -1.0f;
  return cm;
}

// Mengembalikan lux, atau -1 bila sensor tidak ada
inline float luxRead() {
  if (!luxOk) return -1.0f;
  float l = lightMeter.readLightLevel();
  return (l < 0) ? -1.0f : l;
}
