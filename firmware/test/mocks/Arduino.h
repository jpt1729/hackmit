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

namespace mock {
inline uint32_t nowMs = 0;         // what millis() returns
inline time_t   epoch = 0;         // what time() returns (0 = clock not set)
inline int      adc[40] = {0};     // analogRead() per pin
inline int      pwm[40] = {0};     // last analogWrite() per pin
inline int      pwmWrites = 0;     // total analogWrite() calls
inline bool     serialEcho = false;

// Advance both clocks together, like real time passing.
inline void advance(uint32_t ms) {
  static uint32_t carry = 0;
  nowMs += ms;
  carry += ms;
  epoch += carry / 1000;
  carry %= 1000;
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
inline void pinMode(uint8_t, uint8_t) {}
inline int  analogRead(uint8_t pin) { return mock::adc[pin]; }
inline void analogWrite(uint8_t pin, int v) { mock::pwm[pin] = v; mock::pwmWrites++; }

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
