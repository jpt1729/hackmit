#pragma once
#include <stdint.h>

void     gpsInit();
void     gpsTick();
void     gpsFeed(char c);        // one raw NMEA byte; the tests drive this directly
bool     gpsFixValid();          // fix, enough satellites, and not stale
uint32_t gpsFixAgeMs();          // since the last valid position, UINT32_MAX if never
bool     gpsClockWasSet();       // true once the wall clock came from GPS
float    gpsDistanceM(double lat1, double lon1, double lat2, double lon2);
float    gpsBearingDeg(double lat1, double lon1, double lat2, double lon2);
const char* gpsCompass(float bearingDeg);   // "N", "NE", ... for the OLED
const char* gpsHomeHeading();               // direction home, "" without a fix
