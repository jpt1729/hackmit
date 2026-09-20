#include <unity.h>
#include "fall.h"
#include "activity.h"
#include "config.h"
#include "test_support.h"

DeviceState g_state;

static const uint32_t SAMPLE_MS = 1000 / ACT_SAMPLE_HZ;

void setUp() {
  resetMocks();
  eventsInit();
  activityInit();
  fallInit();
  g_state.worn = true;
}
void tearDown() {}

static void tickBoth() {
  activityTick();
  fallTick();
}

// Hold the wrist at a given magnitude for `ms`, running both ticks.
static void hold(float g, uint32_t ms) {
  mock::accelX = 0; mock::accelY = 0; mock::accelZ = g;
  runFor(ms, tickBoth, SAMPLE_MS);
}

// The full signature: the arm goes light, lands hard, then stops moving.
static void simulateFall() {
  hold(0.2f, 300);                        // freefall
  hold(3.0f, 200);                        // impact
  hold(1.0f, FALL_SETTLE_MS + FALL_STILL_MS + 500);   // lying still
}

static void shake() {
  for (int i = 0; i < SHAKE_COUNT + 1; i++) {
    hold(2.5f, SAMPLE_MS * 2);
    hold(1.0f, SAMPLE_MS * 2);
  }
}

void test_quiet_wrist_never_reports_a_fall() {
  hold(1.0f, 10000);
  TEST_ASSERT_FALSE(fallActive());
  TEST_ASSERT_EQUAL(FALL_NONE, g_state.fallStage);
}

// The single most important negative case: this is what setting a mug down or
// dropping into a chair looks like, and it must not call an ambulance.
void test_impact_without_freefall_is_not_a_fall() {
  hold(3.0f, 200);
  hold(1.0f, FALL_SETTLE_MS + FALL_STILL_MS + 500);
  TEST_ASSERT_FALSE(fallActive());
}

void test_freefall_with_a_soft_landing_is_not_a_fall() {
  hold(0.2f, 300);
  hold(1.1f, FALL_SETTLE_MS + FALL_STILL_MS + 500);
  TEST_ASSERT_FALSE(fallActive());
}

// Someone who gets up was not hurt.
void test_getting_up_after_the_impact_clears_the_candidate() {
  hold(0.2f, 300);
  hold(3.0f, 200);
  hold(1.0f, FALL_SETTLE_MS + 200);   // settle, then move again
  hold(1.4f, 1500);
  hold(1.0f, 3000);
  TEST_ASSERT_FALSE(fallActive());
}

void test_full_signature_opens_the_ladder_at_confirming() {
  simulateFall();
  TEST_ASSERT_EQUAL(FALL_CONFIRMING, g_state.fallStage);
  TEST_ASSERT_EQUAL(1, countEvents("fall_detected"));
  TEST_ASSERT_EQUAL(0, countEvents("fall_alert"));
  TEST_ASSERT_EQUAL(0, countEvents("fall_ems"));
}

// A watch knocked off a table produces a textbook signature.
void test_a_fall_while_not_worn_is_ignored() {
  g_state.worn = false;
  simulateFall();
  TEST_ASSERT_FALSE(fallActive());
  TEST_ASSERT_EQUAL(0, countEvents("fall_detected"));
}

void test_shake_during_confirming_cancels_before_anyone_is_called() {
  simulateFall();
  shake();
  TEST_ASSERT_EQUAL(FALL_NONE, g_state.fallStage);
  TEST_ASSERT_EQUAL(1, countEvents("fall_cancelled"));
  TEST_ASSERT_EQUAL(0, countEvents("fall_alert"));
  TEST_ASSERT_EQUAL(0, countEvents("fall_ems"));
}

void test_no_answer_escalates_to_the_caregiver() {
  simulateFall();
  runFor(FALL_CANCEL_MS + 500, tickBoth, 100);
  TEST_ASSERT_EQUAL(FALL_CAREGIVER, g_state.fallStage);
  TEST_ASSERT_EQUAL(1, countEvents("fall_alert"));
  TEST_ASSERT_EQUAL(0, countEvents("fall_ems"));
}

void test_still_no_answer_escalates_to_ems() {
  simulateFall();
  runFor(FALL_CANCEL_MS + FALL_EMS_MS + 1000, tickBoth, 100);
  TEST_ASSERT_EQUAL(FALL_EMS, g_state.fallStage);
  TEST_ASSERT_EQUAL(1, countEvents("fall_ems"));
}

void test_ems_is_only_called_once() {
  simulateFall();
  runFor(FALL_CANCEL_MS + FALL_EMS_MS * 3, tickBoth, 100);
  TEST_ASSERT_EQUAL(1, countEvents("fall_ems"));
}

void test_shake_after_the_caregiver_stage_still_cancels() {
  simulateFall();
  runFor(FALL_CANCEL_MS + 500, tickBoth, 100);
  TEST_ASSERT_EQUAL(FALL_CAREGIVER, g_state.fallStage);
  shake();
  TEST_ASSERT_EQUAL(FALL_NONE, g_state.fallStage);
  TEST_ASSERT_EQUAL(0, countEvents("fall_ems"));
}

void test_dashboard_can_clear_a_fall() {
  simulateFall();
  fallCancel();
  TEST_ASSERT_EQUAL(FALL_NONE, g_state.fallStage);
  TEST_ASSERT_EQUAL(1, countEvents("fall_cancelled"));
}

void test_cancel_is_harmless_when_nothing_is_wrong() {
  fallCancel();
  TEST_ASSERT_EQUAL(FALL_NONE, g_state.fallStage);
  TEST_ASSERT_EQUAL(0, countEvents("fall_cancelled"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_quiet_wrist_never_reports_a_fall);
  RUN_TEST(test_impact_without_freefall_is_not_a_fall);
  RUN_TEST(test_freefall_with_a_soft_landing_is_not_a_fall);
  RUN_TEST(test_getting_up_after_the_impact_clears_the_candidate);
  RUN_TEST(test_full_signature_opens_the_ladder_at_confirming);
  RUN_TEST(test_a_fall_while_not_worn_is_ignored);
  RUN_TEST(test_shake_during_confirming_cancels_before_anyone_is_called);
  RUN_TEST(test_no_answer_escalates_to_the_caregiver);
  RUN_TEST(test_still_no_answer_escalates_to_ems);
  RUN_TEST(test_ems_is_only_called_once);
  RUN_TEST(test_shake_after_the_caregiver_stage_still_cancels);
  RUN_TEST(test_dashboard_can_clear_a_fall);
  RUN_TEST(test_cancel_is_harmless_when_nothing_is_wrong);
  return UNITY_END();
}
