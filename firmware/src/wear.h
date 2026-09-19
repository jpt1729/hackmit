#pragma once

#include "state.h"

namespace routine_anchor {

void wear_init();
void wear_tick(DeviceState& state);
bool is_worn();

}  // namespace routine_anchor
