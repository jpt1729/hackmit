#pragma once

#include "state.h"

namespace routine_anchor {

void activity_init();
void activity_tick(DeviceState& state);
bool ack_detected();

}  // namespace routine_anchor
