#include <unity.h>
#include "location.h"
#include "config.h"
#include "test_support.h"

DeviceState g_state;

void setUp() {
  resetMocks();
  eventsInit();
  locationInit();
}
void tearDown() {}

// Put the device at a reference fingerprint (optionally lowercased / shifted).
static void standIn(Room room, int offset = 0, bool lower = false) {
  mock::scanResults.clear();
  for (size_t r = 0; r < NUM_ROOM_FPS; r++) {
    if (ROOM_FPS[r].room != room) continue;
    for (uint8_t i = 0; i < ROOM_FPS[r].n; i++) {
      std::string b = ROOM_FPS[r].aps[i].bssid;
      if (lower) for (auto& c : b) c = tolower(c);
      mock::scanResults.push_back({b, ROOM_FPS[r].aps[i].rssi + offset});
    }
  }
  mock::scanResults.push_back({"11:22:33:44:55:66", -80});   // neighbour's AP, not in any fingerprint
}

// Wait out one scan interval and let the scan complete.
static void scan() {
  runFor(LOC_SCAN_INTERVAL_MS + 100, locationTick, 50);
}

void test_config_has_at_least_two_rooms() {
  TEST_ASSERT_GREATER_OR_EQUAL(2, NUM_ROOM_FPS);
  for (size_t r = 0; r < NUM_ROOM_FPS; r++) {
    TEST_ASSERT_GREATER_THAN(0, ROOM_FPS[r].n);
    TEST_ASSERT_LESS_OR_EQUAL(8, ROOM_FPS[r].n);
  }
}

void test_no_scan_before_interval() {
  runFor(LOC_SCAN_INTERVAL_MS - 1000, locationTick, 50);
  TEST_ASSERT_EQUAL(0, mock::scansStarted);
}

void test_detects_room_after_debounce() {
  standIn(KITCHEN);
  scan();
  TEST_ASSERT_EQUAL(ROOM_UNKNOWN, g_state.room);   // 1 scan isn't enough
  scan();
  TEST_ASSERT_EQUAL(KITCHEN, g_state.room);
  TEST_ASSERT_GREATER_OR_EQUAL(LOC_MIN_CONFIDENCE, g_state.roomConfidence);
  TEST_ASSERT_EQUAL(1, countEvents("room_change", "kitchen"));
}

void test_bssid_match_is_case_insensitive() {
  standIn(BEDROOM, 0, true);
  scan(); scan();
  TEST_ASSERT_EQUAL(BEDROOM, g_state.room);
}

void test_tolerates_rssi_drift() {
  standIn(KITCHEN, -6);     // body shadowing / different orientation
  scan(); scan();
  TEST_ASSERT_EQUAL(KITCHEN, g_state.room);
}

void test_room_change_needs_two_scans() {
  standIn(KITCHEN);
  scan(); scan();
  standIn(BEDROOM);
  scan();
  TEST_ASSERT_EQUAL(KITCHEN, g_state.room);
  scan();
  TEST_ASSERT_EQUAL(BEDROOM, g_state.room);
  TEST_ASSERT_EQUAL(1, countEvents("room_change", "bedroom"));
}

void test_flapping_between_rooms_holds() {
  standIn(KITCHEN);
  scan(); scan();
  for (int i = 0; i < 4; i++) {
    standIn(i % 2 ? KITCHEN : BEDROOM);
    scan();
  }
  TEST_ASSERT_EQUAL(KITCHEN, g_state.room);
  TEST_ASSERT_EQUAL(1, countEvents("room_change"));
}

void test_staying_put_emits_no_extra_events() {
  standIn(KITCHEN);
  for (int i = 0; i < 6; i++) scan();
  TEST_ASSERT_EQUAL(1, countEvents("room_change"));
}

void test_ambiguous_scan_holds_unknown() {
  // Halfway between the kitchen and bedroom fingerprints.
  mock::scanResults.clear();
  const RoomFP& k = ROOM_FPS[0];
  const RoomFP& b = ROOM_FPS[1];
  for (uint8_t i = 0; i < k.n; i++)
    mock::scanResults.push_back({k.aps[i].bssid, (k.aps[i].rssi + b.aps[i].rssi) / 2});
  scan(); scan(); scan();
  TEST_ASSERT_EQUAL(ROOM_UNKNOWN, g_state.room);
  TEST_ASSERT_EQUAL(0, countEvents("room_change"));
}

void test_no_known_aps_holds_unknown() {
  mock::scanResults = {{"99:99:99:99:99:99", -40}};
  scan(); scan(); scan();
  TEST_ASSERT_EQUAL(ROOM_UNKNOWN, g_state.room);
}

void test_empty_scan_is_safe() {
  mock::scanResults.clear();
  scan(); scan();
  TEST_ASSERT_EQUAL(ROOM_UNKNOWN, g_state.room);
}

void test_failed_scan_retries_next_interval() {
  mock::autoCompleteScans = false;
  runFor(LOC_SCAN_INTERVAL_MS + 100, locationTick, 50);
  TEST_ASSERT_EQUAL(1, mock::scansStarted);
  mock::scanState = WIFI_SCAN_FAILED;
  locationTick();
  mock::autoCompleteScans = true;
  standIn(KITCHEN);
  scan(); scan();
  TEST_ASSERT_EQUAL(3, mock::scansStarted);
  TEST_ASSERT_EQUAL(KITCHEN, g_state.room);
}

void test_slow_scan_is_not_restarted() {
  mock::autoCompleteScans = false;
  runFor(3 * LOC_SCAN_INTERVAL_MS, locationTick, 50);
  TEST_ASSERT_EQUAL(1, mock::scansStarted);        // still waiting on the first
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_config_has_at_least_two_rooms);
  RUN_TEST(test_no_scan_before_interval);
  RUN_TEST(test_detects_room_after_debounce);
  RUN_TEST(test_bssid_match_is_case_insensitive);
  RUN_TEST(test_tolerates_rssi_drift);
  RUN_TEST(test_room_change_needs_two_scans);
  RUN_TEST(test_flapping_between_rooms_holds);
  RUN_TEST(test_staying_put_emits_no_extra_events);
  RUN_TEST(test_ambiguous_scan_holds_unknown);
  RUN_TEST(test_no_known_aps_holds_unknown);
  RUN_TEST(test_empty_scan_is_safe);
  RUN_TEST(test_failed_scan_retries_next_interval);
  RUN_TEST(test_slow_scan_is_not_restarted);
  return UNITY_END();
}
