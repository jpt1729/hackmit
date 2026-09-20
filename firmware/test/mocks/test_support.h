#pragma once
// Shared helpers for native test suites. Include AFTER the module headers.
#include <unity.h>
#include <string>
#include "Arduino.h"
#include "Wire.h"
#include "WiFi.h"
#include "state.h"
#include "events.h"

// Wall clock at the given local time on a fixed date (2026-09-19). Tests run
// with TZ=UTC so localtime_r() in the firmware sees exactly this hour/minute.
inline time_t at(int hour, int minute, int second = 0) {
  const time_t day0 = 1789776000;   // 2026-09-19 00:00:00 UTC
  return day0 + hour * 3600 + minute * 60 + second;
}

inline void resetMocks() {
  setenv("TZ", "UTC0", 1);
  tzset();
  mock::nowMs = 1000;               // not 0: firmware uses 0 as a "never" sentinel
  mock::epoch = 0;
  mock::carryMs = 0;
  memset(mock::adc, 0, sizeof(mock::adc));
  memset(mock::pwm, 0, sizeof(mock::pwm));
  memset(mock::pinLevel, 0, sizeof(mock::pinLevel));
  mock::pwmWrites = 0;
  mock::toneFreq = 0;
  mock::toneDuty = 0;
  mock::toneWrites = 0;
  mock::serial2Rx.clear();
  mock::imuPresent = true;
  mock::accelX = 0; mock::accelY = 0; mock::accelZ = 1.0f;
  mock::scanResults.clear();
  mock::scanState = WIFI_SCAN_RUNNING;
  mock::scansStarted = 0;
  mock::autoCompleteScans = true;
  g_state = DeviceState();
}

inline std::string eventsJson(uint32_t since = 0) {
  return std::string(eventsJsonSince(since).c_str());
}

// Number of events matching type (and detail, if given) in the log.
inline int countEvents(const char* type, const char* detail = nullptr) {
  std::string json = eventsJson();
  std::string needle = std::string("\"type\":\"") + type + "\"";
  if (detail) needle += std::string(",\"detail\":\"") + detail + "\"";
  int n = 0;
  for (size_t p = json.find(needle); p != std::string::npos; p = json.find(needle, p + 1)) n++;
  return n;
}

// Run a module's tick() repeatedly while advancing time in `stepMs` steps.
template <typename F>
inline void runFor(uint32_t ms, F tick, uint32_t stepMs = 10) {
  for (uint32_t t = 0; t < ms; t += stepMs) {
    mock::advance(stepMs);
    tick();
  }
}
