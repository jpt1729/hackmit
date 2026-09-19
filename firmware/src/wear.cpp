#include "wear.h"

namespace routine_anchor {

void wear_init() {
  // Placeholder: initialize electrode pad input.
}

void wear_tick(DeviceState& state) {
  state.worn = true;
}

bool is_worn() {
  return true;
}

}  // namespace routine_anchor
