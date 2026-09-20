#pragma once
// Fake I2C bus with a fake MPU-6050 behind it. Tests set mock::imuPresent and
// mock::accel{X,Y,Z} (in g); reads of ACCEL_XOUT_H return those as raw counts.
#include "Arduino.h"
#include "config.h"

namespace mock {
inline bool  imuPresent = true;
inline float accelX = 0.0f, accelY = 0.0f, accelZ = 1.0f;
}  // namespace mock

class TwoWire {
 public:
  void begin(int, int) {}
  void beginTransmission(uint8_t) { reg_ = -1; }
  size_t write(uint8_t b) {
    if (reg_ < 0) reg_ = b;
    return 1;
  }
  uint8_t endTransmission(bool = true) { return mock::imuPresent ? 0 : 2; }  // 2 = NACK
  uint8_t requestFrom(int, int n) {
    if (!mock::imuPresent) return 0;
    const float g[3] = {mock::accelX, mock::accelY, mock::accelZ};
    len_ = 0;
    for (int i = 0; i < 3; i++) {
      // Saturates exactly like the real part: the configured full-scale range
      // is the most any axis can ever report, which is why FALL_IMPACT_G has
      // to sit below it.
      float c = g[i] * MPU_LSB_PER_G;
      int16_t raw = c > 32767 ? 32767 : (c < -32768 ? -32768 : (int16_t)c);
      buf_[len_++] = (uint8_t)((uint16_t)raw >> 8);
      buf_[len_++] = (uint8_t)(raw & 0xFF);
    }
    pos_ = 0;
    return n;
  }
  int read() { return pos_ < len_ ? buf_[pos_++] : -1; }

 private:
  int     reg_ = -1;
  uint8_t buf_[6];
  int     len_ = 0, pos_ = 0;
};
inline TwoWire Wire;
