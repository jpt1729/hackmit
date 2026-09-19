#include "gps.h"
#include "config.h"
#include "events.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#if ENABLE_GPS

#define NMEA_MAX 96      // longest sentence we care about is ~82 chars

static char     line[NMEA_MAX];
static uint8_t  lineLen = 0;
static uint32_t lastFixMs = 0;
static bool     everFixed = false;
static bool     clockSet = false;
static uint8_t  geoConfirm = 0;
static int      utcDate = -1;    // ddmmyy from RMC, -1 until seen
static int      utcTime = -1;    // hhmmss

// Great-circle distance in metres. Equirectangular would drift at the poles;
// haversine is a dozen extra flops and we run it once a second.
float gpsDistanceM(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371000.0, D2R = M_PI / 180.0;
  double dLat = (lat2 - lat1) * D2R;
  double dLon = (lon2 - lon1) * D2R;
  double a = sin(dLat / 2) * sin(dLat / 2) +
             cos(lat1 * D2R) * cos(lat2 * D2R) * sin(dLon / 2) * sin(dLon / 2);
  return (float)(2.0 * R * atan2(sqrt(a), sqrt(1.0 - a)));
}

// Initial great-circle bearing, degrees clockwise from true north.
float gpsBearingDeg(double lat1, double lon1, double lat2, double lon2) {
  const double D2R = M_PI / 180.0;
  double dLon = (lon2 - lon1) * D2R;
  double y = sin(dLon) * cos(lat2 * D2R);
  double x = cos(lat1 * D2R) * sin(lat2 * D2R) - sin(lat1 * D2R) * cos(lat2 * D2R) * cos(dLon);
  double deg = atan2(y, x) / D2R;
  return (float)(deg < 0 ? deg + 360.0 : deg);
}

const char* gpsCompass(float bearingDeg) {
  static const char* POINTS[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int i = (int)((bearingDeg + 22.5f) / 45.0f) & 7;
  return POINTS[i];
}

const char* gpsHomeHeading() {
  if (!gpsFixValid()) return "";
  return gpsCompass(gpsBearingDeg(g_state.lat, g_state.lon, HOME_LAT, HOME_LON));
}

// "4821.6531" + 'N' -> 48.360885. Degrees are the leading 2 (lat) or 3 (lon)
// digits, the rest is minutes.
static double nmeaDegrees(const char* field, const char* hemi, int degDigits) {
  if (!field || !*field || !hemi || !*hemi) return 1e9;
  char deg[4] = {0};
  strncpy(deg, field, degDigits);
  double value = atof(deg) + atof(field + degDigits) / 60.0;
  if (*hemi == 'S' || *hemi == 'W') value = -value;
  return value;
}

static bool checksumOk(const char* s, uint8_t len) {
  if (len < 9 || s[0] != '$') return false;
  int star = -1;
  for (int i = len - 1; i > 0; i--)
    if (s[i] == '*') { star = i; break; }
  if (star < 1 || star + 2 >= len) return false;      // need two hex digits after it
  uint8_t sum = 0;
  for (int i = 1; i < star; i++) sum ^= (uint8_t)s[i];
  return sum == (uint8_t)strtol(s + star + 1, nullptr, 16);
}

// Days since 1970-01-01 (Howard Hinnant's days_from_civil). Avoids timegm(),
// which is a GNU extension the ESP32 toolchain does not reliably provide.
static long daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (long)era * 146097 + (long)doe - 719468;
}

// GPS time arrives as UTC. The device already knows its own TZ offset, so feed
// the RTC UTC and let localtime_r() do the rest, exactly like NTP does.
static void setClockFromGps() {
  if (utcDate < 0 || utcTime < 0 || timeValid()) return;
  int day = utcDate / 10000, month = (utcDate / 100) % 100, year = 2000 + utcDate % 100;
  int hour = utcTime / 10000, minute = (utcTime / 100) % 100, sec = utcTime % 100;
  if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23) return;

  time_t utc = daysFromCivil(year, month, day) * 86400L + hour * 3600L + minute * 60L + sec;
  if (utc < 1700000000) return;             // garbage date: ignore it
  struct timeval tv = {utc, 0};
  settimeofday(&tv, nullptr);
  clockSet = true;
  Serial.printf("[gps] clock set from satellites: %04d-%02d-%02d %02d:%02d UTC\n",
                year, month, day, hour, minute);
}

static void updateGeofence() {
  float d = gpsDistanceM(g_state.lat, g_state.lon, HOME_LAT, HOME_LON);
  g_state.distanceHomeM = d;

  // Hysteresis: leaving takes the full radius, coming back takes a bit less,
  // so someone sitting on the porch at the boundary does not flap.
  bool outside = g_state.awayFromHome ? d > GEOFENCE_RADIUS_M - GEOFENCE_HYST_M
                                      : d > GEOFENCE_RADIUS_M;
  if (outside == g_state.awayFromHome) {
    geoConfirm = 0;
    return;
  }
  if (++geoConfirm < GEOFENCE_CONFIRM) return;

  geoConfirm = 0;
  g_state.awayFromHome = outside;
  char detail[24];
  snprintf(detail, sizeof(detail), "%dm", (int)d);
  addEvent(outside ? "geofence_exit" : "geofence_return", detail);
}

static void parseSentence(char* s, uint8_t len) {
  if (!checksumOk(s, len)) return;

  char* field[16] = {nullptr};
  uint8_t n = 0;
  field[n++] = s;
  for (uint8_t i = 0; i < len && n < 16; i++) {
    if (s[i] == ',' || s[i] == '*') {
      s[i] = 0;
      field[n++] = s + i + 1;
    }
  }

  const char* type = s + 3;     // skip "$GP" / "$GN" / "$GL"
  if (strncmp(type, "GGA", 3) == 0 && n >= 8) {
    int quality = atoi(field[6]);
    g_state.sats = (uint8_t)atoi(field[7]);
    double lat = nmeaDegrees(field[2], field[3], 2);
    double lon = nmeaDegrees(field[4], field[5], 3);
    if (quality > 0 && lat < 1e8 && lon < 1e8) {
      g_state.lat = lat;
      g_state.lon = lon;
      g_state.fix = true;
      lastFixMs = millis();
      if (!everFixed) {
        everFixed = true;
        Serial.printf("[gps] first fix: %.6f, %.6f (%u sats)\n", lat, lon, g_state.sats);
      }
      if (g_state.sats >= GPS_MIN_SATS) updateGeofence();
    }
  } else if (strncmp(type, "RMC", 3) == 0 && n >= 10) {
    if (field[1][0]) utcTime = atoi(field[1]);          // hhmmss(.sss)
    if (field[9][0]) utcDate = atoi(field[9]);          // ddmmyy
    if (field[2][0] == 'A') setClockFromGps();
  }
}

void gpsInit() {
  lineLen = 0;
  lastFixMs = 0;
  everFixed = false;
  clockSet = false;
  geoConfirm = 0;
  utcDate = utcTime = -1;
  g_state.fix = false;
  g_state.sats = 0;
  g_state.distanceHomeM = -1.0f;
  Serial2.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
}

void gpsFeed(char c) {
  if (c == '$') {                 // a new sentence always resets the buffer
    lineLen = 0;
    line[lineLen++] = c;
    return;
  }
  if (c == '\r' || c == '\n') {
    if (lineLen > 6) {
      line[lineLen] = 0;
      parseSentence(line, lineLen);
    }
    lineLen = 0;
    return;
  }
  if (lineLen && lineLen < NMEA_MAX - 1) line[lineLen++] = c;
}

void gpsTick() {
  // One second of NMEA at 9600 baud is ~960 bytes; cap the work per loop so a
  // backlog drains over several ticks instead of stalling haptics or the ring.
  for (int i = 0; i < 256 && Serial2.available(); i++) gpsFeed((char)Serial2.read());

  if (g_state.fix && millis() - lastFixMs > GPS_STALE_MS) {
    g_state.fix = false;
    g_state.distanceHomeM = -1.0f;
    geoConfirm = 0;
    Serial.println("[gps] fix lost");
  }
}

bool gpsFixValid() {
  return g_state.fix && g_state.sats >= GPS_MIN_SATS && millis() - lastFixMs <= GPS_STALE_MS;
}

uint32_t gpsFixAgeMs() { return everFixed ? millis() - lastFixMs : UINT32_MAX; }
bool     gpsClockWasSet() { return clockSet; }

#else

void     gpsInit() {}
void     gpsTick() {}
void     gpsFeed(char) {}
bool     gpsFixValid() { return false; }
float    gpsBearingDeg(double, double, double, double) { return 0.0f; }
const char* gpsCompass(float) { return ""; }
const char* gpsHomeHeading() { return ""; }
uint32_t gpsFixAgeMs() { return UINT32_MAX; }
bool     gpsClockWasSet() { return false; }
float    gpsDistanceM(double, double, double, double) { return -1.0f; }

#endif
