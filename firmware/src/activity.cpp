#include "activity.h"
#include "config.h"
#include "events.h"
#include <Wire.h>
#include <math.h>
#include <string.h>

static bool     imuOk = false;
static uint32_t lastSampleMs = 0;
static float    ema = 0.0f;
static uint32_t stillSinceMs = 0;

static bool     ackFlag = false;
static uint32_t peakTimes[SHAKE_COUNT];
static uint8_t  peakIdx = 0;
static uint32_t lastPeakMs = 0;

static uint32_t lastWanderMs = 0;

static bool readAccelMagnitude(float& mag) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom((int)MPU_ADDR, 6) != 6) return false;
  float sumSq = 0.0f;
  for (int i = 0; i < 3; i++) {
    int hi = Wire.read();
    int lo = Wire.read();
    float g = (int16_t)((hi << 8) | lo) / 16384.0f;
    sumSq += g * g;
  }
  mag = sqrtf(sumSq);
  return true;
}

static bool isNight() {
  time_t t = time(nullptr);
  struct tm tmv;
  localtime_r(&t, &tmv);
  return tmv.tm_hour >= WANDER_START_H && tmv.tm_hour < WANDER_END_H;
}

void activityInit() {
  lastSampleMs = 0;
  ema = 0.0f;
  ackFlag = false;
  memset(peakTimes, 0, sizeof(peakTimes));
  peakIdx = 0;
  lastPeakMs = 0;
  lastWanderMs = 0;
  stillSinceMs = millis();

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  imuOk = Wire.endTransmission() == 0;
  if (!imuOk) Serial.println("[activity] MPU6050 not found");
}

void activityTick() {
  uint32_t now = millis();
  if (!imuOk || now - lastSampleMs < 1000 / ACT_SAMPLE_HZ) return;
  lastSampleMs = now;

  float mag;
  if (!readAccelMagnitude(mag)) return;

  if (mag > SHAKE_G && now - lastPeakMs > 120) {
    lastPeakMs = now;
    peakTimes[peakIdx] = now;
    peakIdx = (peakIdx + 1) % SHAKE_COUNT;
    uint32_t oldest = peakTimes[peakIdx];
    if (oldest && now - oldest < SHAKE_WINDOW_MS) ackFlag = true;
  }

  ema += (fabsf(mag - 1.0f) - ema) * ACT_EMA_ALPHA;
  bool moving = ema > MOVE_THRESH_G;
  if (moving) stillSinceMs = now;
  bool asleep = now - stillSinceMs > SLEEP_STILL_MIN * 60000UL;
  g_state.activity = moving ? MOVING : asleep ? SLEEPING : RESTING;

  g_state.wanderFlag = moving && timeValid() && isNight();
  if (g_state.wanderFlag && (lastWanderMs == 0 || now - lastWanderMs > WANDER_COOLDOWN_MIN * 60000UL)) {
    lastWanderMs = now;
    addEvent("wander", "");
  }
}

bool activityAckConsume() {
  if (!ackFlag) return false;
  ackFlag = false;
  memset(peakTimes, 0, sizeof(peakTimes));
  return true;
}
