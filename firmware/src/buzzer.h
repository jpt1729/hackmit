#pragma once
#include <stdint.h>

void buzzerInit();
void buzzerTick();
void buzzerGentle();    // prompt fired: soft two-note chime
void buzzerRemind();    // prompt unacknowledged: three notes, a little louder
void buzzerAlert();     // left the geofence / night wander: rising two-tone
void buzzerAck();       // acknowledgement blip
void buzzerStop();
bool buzzerBusy();
