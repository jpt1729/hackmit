#pragma once
// Minimal Arduino shim for host-side (native) unit tests.
// Only what src/ actually uses. Hardware is replaced by the `mock::` knobs
// below, which tests set directly. Everything is header-only (C++17 inline
// variables) so no extra translation units are needed.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <string>
#include <vector>
#include <sys/time.h>

namespace mock {
inline uint32_t nowMs = 0;         // what millis() returns
inline time_t   epoch = 0;         // what time() returns (0 = clock not set)
inline uint32_t carryMs = 0;       // sub-second remainder held by advance()
inline int      adc[40] = {0};     // analogRead() per pin
inline int      pwm[40] = {0};     // last analogWrite() per pin
inline int      pwmWrites = 0;     // total analogWrite() calls
inline int      pinLevel[40] = {0};// last digitalWrite() per pin
inline int      toneFreq = 0;      // last ledcWriteTone() frequency, 0 = silent
inline int      toneDuty = 0;      // last ledcWrite() duty
inline int      toneWrites = 0;    // total ledc writes
inline std::string serial2Rx;      // bytes waiting on Serial2 (the GPS)
inline bool     serialEcho = false;

// Advance both clocks together, like real time passing.
inline void advance(uint32_t ms) {
  nowMs += ms;
  carryMs += ms;
  epoch += carryMs / 1000;
  carryMs %= 1000;
}
}  // namespace mock

inline uint32_t millis() { return mock::nowMs; }
inline void delay(uint32_t ms) { mock::advance(ms); }

// time() is libc; redirect it so tests control the wall clock.
inline time_t mock_time(time_t* t) {
  if (t) *t = mock::epoch;
  return mock::epoch;
}
#define time(x) mock_time(x)

#define INPUT  0x01
#define OUTPUT 0x03
#define LOW    0x00
#define HIGH   0x01
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t pin, uint8_t v) { mock::pinLevel[pin] = v; }
inline int  analogRead(uint8_t pin) { return mock::adc[pin]; }
inline void analogWrite(uint8_t pin, int v) { mock::pwm[pin] = v; mock::pwmWrites++; }

// LEDC (ESP32 core 2.x signature): what the piezo is actually driven with.
inline void   ledcSetup(uint8_t, uint32_t, uint8_t) {}
inline void   ledcAttachPin(uint8_t, uint8_t) {}
inline double ledcWriteTone(uint8_t, double freq) {
  mock::toneFreq = (int)freq;
  mock::toneWrites++;
  return freq;
}
inline void ledcWrite(uint8_t, uint32_t duty) {
  mock::toneDuty = (int)duty;
  if (duty == 0) mock::toneFreq = 0;
  mock::toneWrites++;
}

// time() is redirected above, so settimeofday() has to move the same clock.
inline int mock_settimeofday(const struct timeval* tv, const void*) {
  mock::epoch = tv->tv_sec;
  return 0;
}
#define settimeofday(tv, tz) mock_settimeofday(tv, tz)

template <typename T, typename L, typename H>
inline T constrain(T x, L lo, H hi) { return x < lo ? (T)lo : (x > hi ? (T)hi : x); }

class String {
 public:
  String() {}
  String(const char* s) : s_(s ? s : "") {}
  String(const std::string& s) : s_(s) {}
  explicit String(int v) : s_(std::to_string(v)) {}
  explicit String(unsigned v) : s_(std::to_string(v)) {}
  explicit String(long v) : s_(std::to_string(v)) {}
  explicit String(unsigned long v) : s_(std::to_string(v)) {}
  explicit String(unsigned char v) : s_(std::to_string(v)) {}

  void reserve(size_t n) { s_.reserve(n); }
  String& operator=(const char* s) { s_ = s ? s : ""; return *this; }
  String& operator+=(const String& o) { s_ += o.s_; return *this; }
  String& operator+=(const char* s) { s_ += s; return *this; }
  String& operator+=(char c) { s_ += c; return *this; }
  bool operator==(const char* s) const { return s_ == s; }

  const char* c_str() const { return s_.c_str(); }
  size_t length() const { return s_.size(); }
  long toInt() const { return atol(s_.c_str()); }
  bool equalsIgnoreCase(const String& o) const {
    return s_.size() == o.s_.size() && strcasecmp(s_.c_str(), o.s_.c_str()) == 0;
  }

 private:
  std::string s_;
};

class MockSerial {
 public:
  void begin(unsigned long) {}
  int printf(const char* fmt, ...) {
    if (!mock::serialEcho) return 0;
    va_list ap;
    va_start(ap, fmt);
    int n = vprintf(fmt, ap);
    va_end(ap);
    return n;
  }
  void print(const char* s) { if (mock::serialEcho) fputs(s, stdout); }
  void print(char c) { if (mock::serialEcho) putchar(c); }
  void println(const char* s = "") { if (mock::serialEcho) puts(s); }
};
inline MockSerial Serial;

// Serial2 is the GT-U7. Tests push NMEA into it with mock::serial2Rx or
// Serial2.inject(...), and gpsTick() drains it exactly like the real UART.
#define SERIAL_8N1 0x800001c
class MockSerial2 {
 public:
  void begin(unsigned long, uint32_t = 0, int8_t = -1, int8_t = -1) {}
  void inject(const char* s) { mock::serial2Rx += s; }
  int  available() { return (int)mock::serial2Rx.size(); }
  int  read() {
    if (mock::serial2Rx.empty()) return -1;
    int c = (unsigned char)mock::serial2Rx.front();
    mock::serial2Rx.erase(0, 1);
    return c;
  }
};
inline MockSerial2 Serial2;
