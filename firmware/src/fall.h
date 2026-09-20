#pragma once
#include <stdint.h>

// The escalation ladder that sits between the accelerometer and an ambulance.
//
// activity.cpp decides that something looked like a fall. It is often wrong -
// an accelerometer cannot tell a fall from a dropped arm with certainty - so
// nothing here contacts anyone until the wearer has been given two chances to
// say they are fine:
//
//   detected -> "are you OK? shake to cancel"  (FALL_CANCEL_MS)
//            -> caregiver alerted              (FALL_EMS_MS)
//            -> emergency services
//
// A shake at any stage, or the caregiver clearing it from the dashboard, drops
// straight back to FALL_NONE.
void fallInit();
void fallTick();
void fallCancel();            // shake on the wrist, or POST /fall/cancel
bool fallActive();            // a fall is somewhere on the ladder
