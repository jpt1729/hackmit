#pragma once

#include "state.h"

namespace routine_anchor {

void location_init();
void location_tick(DeviceState& state);
Room get_room();

}  // namespace routine_anchor
