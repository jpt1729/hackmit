#include <unity.h>
#include "activity.h"
#include "config.h"
#include "test_support.h"

DeviceState g_state;

static const uint32_t SAMPLE_MS = 1000 / ACT_SAMPLE_HZ;

void setUp() {
  resetMocks();
  eventsInit();
  activityInit();
}
void tearDown() {}

static void still(uint32_t ms) {
  mock::accelX = 0; mock::accelY = 0; mock::accelZ = 1.0f;
  runFor(ms, activityTick);
}

// Wrist motion: magnitude swings 0.7g <-> 1.3g every sample.
static void moving(uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += SAMPLE_MS) {
    mock::accelZ = (t / SAMPLE_MS) % 2 ? 1.3f : 0.7f;
    runFor(SAMPLE_MS, activityTick);
  }
  mock::accelZ = 1.0f;
}

// One sharp peak (held for a single sample), then back to 1g.
static void peak() {
  mock::accelX = 2.0f; mock::accelZ = 1.0f;   // |a| ~= 2.24g
  runFor(SAMPLE_MS, activityTick);
  mock::accelX = 0.0f;
}

static void shake(uint32_t gapMs, int peaks = SHAKE_COUNT) {
  for (int i = 0; i < peaks; i++) {
    peak();
    if (i < peaks - 1) still(gapMs - SAMPLE_MS);
  }
}

// ---------- classification ----------

void test_missing_imu_is_safe() {
  mock::imuPresent = false;
  activityInit();
  moving(5000);
  shake(300);
  TEST_ASSERT_EQUAL(RESTING, g_state.activity);
  TEST_ASSERT_FALSE(activityAckConsume());
}

void test_still_is_resting() {
  still(10000);
  TEST_ASSERT_EQUAL(RESTING, g_state.activity);
}

void test_motion_is_moving_within_a_second() {
  moving(1000);
  TEST_ASSERT_EQUAL(MOVING, g_state.activity);
}

void test_back_to_resting_after_motion_stops() {
  moving(3000);
  still(5000);
  TEST_ASSERT_EQUAL(RESTING, g_state.activity);
}

void test_sensor_noise_is_not_motion() {
  // +/-0.02g jitter, typical MPU-6050 noise on a table.
  for (int i = 0; i < 100; i++) {
    mock::accelZ = (i % 2) ? 1.02f : 0.98f;
    runFor(SAMPLE_MS, activityTick);
  }
  TEST_ASSERT_EQUAL(RESTING, g_state.activity);
}

void test_sleeping_after_long_stillness() {
  still((SLEEP_STILL_MIN * 60000UL) - 5000);
  TEST_ASSERT_EQUAL(RESTING, g_state.activity);
  still(10000);
  TEST_ASSERT_EQUAL(SLEEPING, g_state.activity);
  moving(1000);
  TEST_ASSERT_EQUAL(MOVING, g_state.activity);
}

// ---------- shake-to-acknowledge ----------

void test_shake_acks_once() {
  shake(300);
  TEST_ASSERT_TRUE(activityAckConsume());
  TEST_ASSERT_FALSE(activityAckConsume());
}

void test_slow_taps_are_not_a_shake() {
  shake(800);   // 3 peaks spanning 1.6 s > SHAKE_WINDOW_MS
  TEST_ASSERT_FALSE(activityAckConsume());
}

void test_too_few_peaks_are_not_a_shake() {
  shake(300, SHAKE_COUNT - 1);
  TEST_ASSERT_FALSE(activityAckConsume());
}

void test_peaks_below_threshold_are_ignored() {
  for (int i = 0; i < 5; i++) {
    mock::accelX = 1.2f;                       // |a| ~= 1.56g < SHAKE_G
    runFor(SAMPLE_MS, activityTick);
    mock::accelX = 0;
    still(200);
  }
  TEST_ASSERT_FALSE(activityAckConsume());
}

void test_consumed_gesture_cannot_ack_twice() {
  shake(300);
  TEST_ASSERT_TRUE(activityAckConsume());
  still(150);                                   // clear the 120 ms min peak gap
  peak();                                       // one more peak right after
  TEST_ASSERT_FALSE(activityAckConsume());      // must not re-use old peaks
}

void test_saturated_axis_still_detects_shake() {
  // Real wrist shakes pin one axis at the +/-2g rail; the mock clamps like the IMU.
  for (int i = 0; i < SHAKE_COUNT; i++) {
    mock::accelX = 3.0f;
    runFor(SAMPLE_MS, activityTick);
    mock::accelX = 0;
    still(200);
  }
  TEST_ASSERT_TRUE(activityAckConsume());
}

// ---------- night wandering ----------

void test_wander_at_night_emits_one_event_per_cooldown() {
  mock::epoch = at(2, 0);
  moving(60000);
  TEST_ASSERT_TRUE(g_state.wanderFlag);
  TEST_ASSERT_EQUAL(1, countEvents("wander"));
  moving((WANDER_COOLDOWN_MIN - 2) * 60000UL);
  TEST_ASSERT_EQUAL(1, countEvents("wander"));
  moving(3 * 60000UL);
  TEST_ASSERT_EQUAL(2, countEvents("wander"));
}

void test_wander_flag_clears_when_still() {
  mock::epoch = at(2, 0);
  moving(2000);
  TEST_ASSERT_TRUE(g_state.wanderFlag);
  still(5000);
  TEST_ASSERT_FALSE(g_state.wanderFlag);
}

void test_no_wander_during_day() {
  mock::epoch = at(14, 0);
  moving(5000);
  TEST_ASSERT_FALSE(g_state.wanderFlag);
  TEST_ASSERT_EQUAL(0, countEvents("wander"));
}

void test_no_wander_at_window_end() {
  mock::epoch = at(WANDER_END_H, 0);
  moving(5000);
  TEST_ASSERT_EQUAL(0, countEvents("wander"));
}

void test_no_wander_without_clock() {
  mock::epoch = 0;   // NTP never arrived; hour would read as 00:xx
  moving(5000);
  TEST_ASSERT_FALSE(g_state.wanderFlag);
  TEST_ASSERT_EQUAL(0, countEvents("wander"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_missing_imu_is_safe);
  RUN_TEST(test_still_is_resting);
  RUN_TEST(test_motion_is_moving_within_a_second);
  RUN_TEST(test_back_to_resting_after_motion_stops);
  RUN_TEST(test_sensor_noise_is_not_motion);
  RUN_TEST(test_sleeping_after_long_stillness);
  RUN_TEST(test_shake_acks_once);
  RUN_TEST(test_slow_taps_are_not_a_shake);
  RUN_TEST(test_too_few_peaks_are_not_a_shake);
  RUN_TEST(test_peaks_below_threshold_are_ignored);
  RUN_TEST(test_consumed_gesture_cannot_ack_twice);
  RUN_TEST(test_saturated_axis_still_detects_shake);
  RUN_TEST(test_wander_at_night_emits_one_event_per_cooldown);
  RUN_TEST(test_wander_flag_clears_when_still);
  RUN_TEST(test_no_wander_during_day);
  RUN_TEST(test_no_wander_at_window_end);
  RUN_TEST(test_no_wander_without_clock);
  return UNITY_END();
}
