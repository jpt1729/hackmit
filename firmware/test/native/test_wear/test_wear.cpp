#include <unity.h>
#include "wear.h"
#include "activity.h"
#include "config.h"
#include "test_support.h"

DeviceState g_state;

void setUp() {
  resetMocks();
  eventsInit();
  activityInit();
  wearInit();
}
void tearDown() {}

// Wear is inferred from the IMU, so every case here is really "what does the
// accelerometer look like in this situation".
static void run(uint32_t ms) {
  runFor(ms, [] { activityTick(); wearTick(); }, 100);
}

// A device on a body: never perfectly still, even when the person is not
// "moving" (amplitude stays under MOVE_THRESH_G on purpose).
static void onBody(uint32_t ms, float amplitude = 0.04f) {
  for (uint32_t t = 0; t < ms; t += 100) {
    mock::accelZ = 1.0f + ((t / 100) % 2 ? amplitude : -amplitude);
    mock::advance(100);
    activityTick();
    wearTick();
  }
}

// A device on a nightstand: the same number every sample.
static void onTable(uint32_t ms) {
  mock::accelZ = 1.0f;
  run(ms);
}

void test_starts_worn_without_events() {
  onBody(10000);
  TEST_ASSERT_TRUE(g_state.worn);
  TEST_ASSERT_EQUAL(0, countEvents("wear_on") + countEvents("wear_off"));
}

void test_micro_motion_counts_as_worn() {
  // Sitting still or asleep: tiny motion, well below the "moving" threshold.
  onBody(WEAR_OFF_STILL_MS + 30000, WEAR_MICRO_G * 3.0f);
  TEST_ASSERT_TRUE(g_state.worn);
  TEST_ASSERT_NOT_EQUAL(MOVING, g_state.activity);
  TEST_ASSERT_EQUAL(0, countEvents("wear_off"));
}

void test_brief_stillness_is_not_removal() {
  onTable(WEAR_OFF_STILL_MS - 30000);
  TEST_ASSERT_TRUE(g_state.worn);
  TEST_ASSERT_EQUAL(0, countEvents("wear_off"));
}

void test_dead_still_means_taken_off() {
  onTable(WEAR_OFF_STILL_MS + 5000);
  TEST_ASSERT_FALSE(g_state.worn);
  TEST_ASSERT_EQUAL(1, countEvents("wear_off"));
}

void test_put_back_on() {
  onTable(WEAR_OFF_STILL_MS + 5000);
  TEST_ASSERT_FALSE(g_state.worn);
  onBody(WEAR_ON_DEBOUNCE_MS + 4000);
  TEST_ASSERT_TRUE(g_state.worn);
  TEST_ASSERT_EQUAL(1, countEvents("wear_off"));
  TEST_ASSERT_EQUAL(1, countEvents("wear_on"));
}

void test_a_nudge_of_the_table_does_not_look_like_wearing() {
  onTable(WEAR_OFF_STILL_MS + 5000);
  onBody(700);                                  // someone walks past and bumps it
  onTable(5000);
  TEST_ASSERT_FALSE(g_state.worn);
  TEST_ASSERT_EQUAL(0, countEvents("wear_on"));
}

void test_no_imu_fails_open() {
  // A dead IMU must not mute every prompt for the rest of the day.
  mock::imuPresent = false;
  activityInit();
  wearInit();
  run(WEAR_OFF_STILL_MS + 10000);
  TEST_ASSERT_TRUE(g_state.worn);
  TEST_ASSERT_EQUAL(0, countEvents("wear_off"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_starts_worn_without_events);
  RUN_TEST(test_micro_motion_counts_as_worn);
  RUN_TEST(test_brief_stillness_is_not_removal);
  RUN_TEST(test_dead_still_means_taken_off);
  RUN_TEST(test_put_back_on);
  RUN_TEST(test_a_nudge_of_the_table_does_not_look_like_wearing);
  RUN_TEST(test_no_imu_fails_open);
  return UNITY_END();
}
