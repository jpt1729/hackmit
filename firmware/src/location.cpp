#include "location.h"

namespace routine_anchor {

void location_init() {
  // Placeholder: initialize WiFi scan / RSSI fingerprint state.
}

void location_tick(DeviceState& state) {
  state.room = UNKNOWN;
  state.rssiConfidence = 0;
}

Room get_room() {
  return UNKNOWN;
}

}  // namespace routine_anchor
