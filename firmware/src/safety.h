#pragma once
#include <stdint.h>

// Turns the two safety states - away from home, night-time wandering - into
// something the wearer notices. The caregiver alert is the event log; this is
// the on-wrist half. Sensors stay sensors: nothing in gps.cpp or activity.cpp
// knows about the buzzer or the ring.
void     safetyInit();
void     safetyTick();
void     safetySilence();      // caregiver pressed "acknowledge" on the dashboard
uint8_t  safetyChimeCount();   // chimes played for the current episode (tests)
