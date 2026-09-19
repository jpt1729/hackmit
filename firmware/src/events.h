#pragma once

#include <stdint.h>

#include "state.h"

namespace routine_anchor {

void events_init();
void events_push(const Event& evt);
uint32_t events_last_id();

}  // namespace routine_anchor
