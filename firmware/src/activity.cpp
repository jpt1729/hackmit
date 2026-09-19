#include "activity.h"

#include <Arduino.h>

namespace routine_anchor {

void activity_init() {
  // Placeholder: initialize IMU and motion tracking.
}

void activity_tick(DeviceState& state) {
  state.activity = RESTING;
  state.wanderFlag = false;
}

bool ack_detected() {
  return false;
}

}  // namespace routine_anchor
