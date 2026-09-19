#pragma once

namespace routine_anchor {

enum HapticPattern { GENTLE, REMIND };

void haptics_init();
void pulse(HapticPattern pattern);

}  // namespace routine_anchor
