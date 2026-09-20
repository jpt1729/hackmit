#pragma once
#include <Arduino.h>

void   locationInit();
void   locationTick();

// One blocking WiFi scan, as JSON, for GET /scan. This is how
// tools/fingerprint_trainer.py learns a room: the measurement has to come from
// the band's own antenna, because that is the antenna that will do the
// guessing. It is a setup-time endpoint - the scan stalls the loop for a
// couple of seconds - and it works even in builds with ENABLE_LOCATION=0, so a
// room table can be trained before room tracking is switched on.
String locationScanJson();
