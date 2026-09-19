#include <unity.h>
#include "gps.h"
#include "config.h"
#include "test_support.h"
#include <string>

DeviceState g_state;

void setUp() {
  resetMocks();
  eventsInit();
  gpsInit();
}
void tearDown() {}

// ---------------------------------------------------------------- NMEA builders
// Checksums are computed here so the fixtures stay readable and stay correct.
static std::string nmea(const std::string& body) {
  uint8_t sum = 0;
  for (char c : body) sum ^= (uint8_t)c;
  char tail[8];
  snprintf(tail, sizeof(tail), "*%02X\r\n", sum);
  return "$" + body + tail;
}

// Decimal degrees -> "ddmm.mmmm" / "dddmm.mmmm" plus its hemisphere letter.
static std::string ddmm(double deg, bool isLat) {
  char buf[24];
  double a = deg < 0 ? -deg : deg;
  int whole = (int)a;
  double minutes = (a - whole) * 60.0;
  snprintf(buf, sizeof(buf), isLat ? "%02d%07.4f,%c" : "%03d%07.4f,%c",
           whole, minutes, isLat ? (deg < 0 ? 'S' : 'N') : (deg < 0 ? 'W' : 'E'));
  return buf;
}

static std::string gga(double lat, double lon, int quality = 1, int sats = 8) {
  char buf[128];
  snprintf(buf, sizeof(buf), "GPGGA,123519.00,%s,%s,%d,%02d,0.9,12.3,M,46.9,M,,",
           ddmm(lat, true).c_str(), ddmm(lon, false).c_str(), quality, sats);
  return nmea(buf);
}

static std::string rmc(const char* time, const char* date, char status = 'A') {
  char buf[128];
  snprintf(buf, sizeof(buf), "GPRMC,%s,%c,%s,%s,0.06,31.66,%s,,,A",
           time, status, ddmm(HOME_LAT, true).c_str(), ddmm(HOME_LON, false).c_str(), date);
  return nmea(buf);
}

// Push bytes through the fake UART, exactly as the loop would drain them.
static void feed(const std::string& s) {
  Serial2.inject(s.c_str());
  mock::advance(50);
  gpsTick();
}

// A point `metres` north of home (1 degree of latitude is ~111.32 km).
static double latNorthOfHome(double metres) { return HOME_LAT + metres / 111320.0; }

static void feedFixes(int n, double metresFromHome) {
  for (int i = 0; i < n; i++) feed(gga(latNorthOfHome(metresFromHome), HOME_LON));
}

// ---------------------------------------------------------------- parsing

void test_no_fix_at_boot() {
  TEST_ASSERT_FALSE(g_state.fix);
  TEST_ASSERT_FALSE(gpsFixValid());
  TEST_ASSERT_EQUAL(0, g_state.sats);
  TEST_ASSERT_TRUE(g_state.distanceHomeM < 0);
}

void test_gga_sets_position_and_fix() {
  feed(gga(HOME_LAT, HOME_LON));
  TEST_ASSERT_TRUE(gpsFixValid());
  TEST_ASSERT_EQUAL(8, g_state.sats);
  TEST_ASSERT_DOUBLE_WITHIN(0.0001, HOME_LAT, g_state.lat);
  TEST_ASSERT_DOUBLE_WITHIN(0.0001, HOME_LON, g_state.lon);
  TEST_ASSERT_FLOAT_WITHIN(15.0f, 0.0f, g_state.distanceHomeM);
}

void test_southern_and_western_hemispheres() {
  feed(gga(-33.8688, 151.2093));          // Sydney: S and E
  TEST_ASSERT_DOUBLE_WITHIN(0.0001, -33.8688, g_state.lat);
  TEST_ASSERT_DOUBLE_WITHIN(0.0001, 151.2093, g_state.lon);
}

void test_bad_checksum_is_ignored() {
  std::string bad = gga(HOME_LAT, HOME_LON);
  bad[bad.size() - 3] = (bad[bad.size() - 3] == '0') ? '1' : '0';   // corrupt the checksum
  feed(bad);
  TEST_ASSERT_FALSE(g_state.fix);
}

void test_quality_zero_is_not_a_fix() {
  feed(gga(HOME_LAT, HOME_LON, 0, 3));
  TEST_ASSERT_FALSE(g_state.fix);
}

void test_truncated_sentence_does_not_break_the_next_one() {
  Serial2.inject("$GPGGA,1235");           // power-up garbage, no terminator
  feed(gga(HOME_LAT, HOME_LON));
  TEST_ASSERT_TRUE(gpsFixValid());
}

void test_split_across_ticks() {
  std::string s = gga(HOME_LAT, HOME_LON);
  Serial2.inject(s.substr(0, 20).c_str());
  mock::advance(50);
  gpsTick();
  TEST_ASSERT_FALSE(g_state.fix);
  feed(s.substr(20));
  TEST_ASSERT_TRUE(gpsFixValid());
}

void test_fix_goes_stale_when_the_module_goes_quiet() {
  feed(gga(HOME_LAT, HOME_LON));
  TEST_ASSERT_TRUE(g_state.fix);
  runFor(GPS_STALE_MS + 1000, gpsTick, 250);
  TEST_ASSERT_FALSE(g_state.fix);
  TEST_ASSERT_FALSE(gpsFixValid());
  TEST_ASSERT_TRUE(g_state.distanceHomeM < 0);
}

// ---------------------------------------------------------------- geofence

void test_exit_needs_repeated_agreement() {
  feedFixes(GEOFENCE_CONFIRM - 1, 200.0);
  TEST_ASSERT_FALSE(g_state.awayFromHome);
  TEST_ASSERT_EQUAL(0, countEvents("geofence_exit"));

  feedFixes(1, 200.0);
  TEST_ASSERT_TRUE(g_state.awayFromHome);
  TEST_ASSERT_EQUAL(1, countEvents("geofence_exit"));
}

void test_one_bad_fix_does_not_trip_the_geofence() {
  feedFixes(1, 400.0);                      // a single wild fix, then back inside
  feedFixes(3, 5.0);
  TEST_ASSERT_FALSE(g_state.awayFromHome);
  TEST_ASSERT_EQUAL(0, countEvents("geofence_exit"));
}

void test_inside_the_radius_is_never_away() {
  feedFixes(6, GEOFENCE_RADIUS_M - 20.0);
  TEST_ASSERT_FALSE(g_state.awayFromHome);
}

void test_return_requires_coming_further_in_than_the_exit_radius() {
  feedFixes(GEOFENCE_CONFIRM, 200.0);
  TEST_ASSERT_TRUE(g_state.awayFromHome);

  // Just inside the fence but still in the hysteresis band: still "away".
  feedFixes(4, GEOFENCE_RADIUS_M - 10.0);
  TEST_ASSERT_TRUE(g_state.awayFromHome);
  TEST_ASSERT_EQUAL(0, countEvents("geofence_return"));

  feedFixes(GEOFENCE_CONFIRM, 5.0);
  TEST_ASSERT_FALSE(g_state.awayFromHome);
  TEST_ASSERT_EQUAL(1, countEvents("geofence_return"));
}

void test_too_few_satellites_does_not_move_the_geofence() {
  for (int i = 0; i < GEOFENCE_CONFIRM + 2; i++)
    feed(gga(latNorthOfHome(200.0), HOME_LON, 1, GPS_MIN_SATS - 1));
  TEST_ASSERT_TRUE(g_state.fix);            // position is still worth showing
  TEST_ASSERT_FALSE(gpsFixValid());         // but not worth acting on
  TEST_ASSERT_FALSE(g_state.awayFromHome);
  TEST_ASSERT_EQUAL(0, countEvents("geofence_exit"));
}

// ---------------------------------------------------------------- maths

void test_distance_matches_a_known_pair() {
  // Kendall/MIT to Boston Common, ~2.4 km.
  float d = gpsDistanceM(42.3620, -71.0840, 42.3550, -71.0656);
  TEST_ASSERT_FLOAT_WITHIN(120.0f, 1700.0f, d);
}

void test_bearing_and_compass_point() {
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, gpsBearingDeg(42.0, -71.0, 42.01, -71.0));
  TEST_ASSERT_EQUAL_STRING("N", gpsCompass(gpsBearingDeg(42.0, -71.0, 42.01, -71.0)));
  TEST_ASSERT_EQUAL_STRING("E", gpsCompass(gpsBearingDeg(42.0, -71.0, 42.0, -70.99)));
  TEST_ASSERT_EQUAL_STRING("S", gpsCompass(gpsBearingDeg(42.0, -71.0, 41.99, -71.0)));
  TEST_ASSERT_EQUAL_STRING("W", gpsCompass(gpsBearingDeg(42.0, -71.0, 42.0, -71.01)));
}

void test_heading_home_points_back() {
  feed(gga(latNorthOfHome(300.0), HOME_LON));      // north of home -> walk south
  TEST_ASSERT_EQUAL_STRING("S", gpsHomeHeading());
}

// ---------------------------------------------------------------- clock

void test_rmc_sets_the_clock_when_ntp_is_blocked() {
  TEST_ASSERT_FALSE(timeValid());
  feed(rmc("120000.00", "190926"));                 // 2026-09-19 12:00:00 UTC
  TEST_ASSERT_TRUE(timeValid());
  TEST_ASSERT_TRUE(gpsClockWasSet());
  TEST_ASSERT_EQUAL(at(12, 0), time(nullptr));
}

void test_rmc_without_a_valid_status_is_ignored() {
  feed(rmc("120000.00", "190926", 'V'));            // V = navigation warning
  TEST_ASSERT_FALSE(timeValid());
}

void test_gps_does_not_overwrite_a_clock_that_is_already_set() {
  mock::epoch = at(9, 0);
  feed(rmc("120000.00", "190926"));
  TEST_ASSERT_EQUAL(at(9, 0), time(nullptr));
  TEST_ASSERT_FALSE(gpsClockWasSet());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_no_fix_at_boot);
  RUN_TEST(test_gga_sets_position_and_fix);
  RUN_TEST(test_southern_and_western_hemispheres);
  RUN_TEST(test_bad_checksum_is_ignored);
  RUN_TEST(test_quality_zero_is_not_a_fix);
  RUN_TEST(test_truncated_sentence_does_not_break_the_next_one);
  RUN_TEST(test_split_across_ticks);
  RUN_TEST(test_fix_goes_stale_when_the_module_goes_quiet);
  RUN_TEST(test_exit_needs_repeated_agreement);
  RUN_TEST(test_one_bad_fix_does_not_trip_the_geofence);
  RUN_TEST(test_inside_the_radius_is_never_away);
  RUN_TEST(test_return_requires_coming_further_in_than_the_exit_radius);
  RUN_TEST(test_too_few_satellites_does_not_move_the_geofence);
  RUN_TEST(test_distance_matches_a_known_pair);
  RUN_TEST(test_bearing_and_compass_point);
  RUN_TEST(test_heading_home_points_back);
  RUN_TEST(test_rmc_sets_the_clock_when_ntp_is_blocked);
  RUN_TEST(test_rmc_without_a_valid_status_is_ignored);
  RUN_TEST(test_gps_does_not_overwrite_a_clock_that_is_already_set);
  return UNITY_END();
}
