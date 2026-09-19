#include <unity.h>
#include "wear.h"
#include "config.h"
#include "test_support.h"

DeviceState g_state;

static const int ON_SKIN = (WEAR_ADC_MIN + WEAR_ADC_MAX) / 2;
static const int OPEN_HIGH = 4095;
static const int OPEN_LOW = 0;

void setUp() {
  resetMocks();
  eventsInit();
  mock::adc[PIN_ELECTRODE] = ON_SKIN;
  wearInit();
}
void tearDown() {}

static void hold(int adc, uint32_t ms) {
  mock::adc[PIN_ELECTRODE] = adc;
  runFor(ms, wearTick, 50);
}

void test_electrode_is_on_adc1() {
  // ADC2 pins read garbage while WiFi is on.
  TEST_ASSERT_TRUE(PIN_ELECTRODE >= 32 && PIN_ELECTRODE <= 39);
}

void test_starts_worn_without_events() {
  hold(ON_SKIN, 10000);
  TEST_ASSERT_TRUE(g_state.worn);
  TEST_ASSERT_EQUAL(0, countEvents("wear_on") + countEvents("wear_off"));
}

void test_removal_is_debounced() {
  hold(OPEN_HIGH, WEAR_DEBOUNCE_MS - 500);
  TEST_ASSERT_TRUE(g_state.worn);
  hold(OPEN_HIGH, 1000);
  TEST_ASSERT_FALSE(g_state.worn);
  TEST_ASSERT_EQUAL(1, countEvents("wear_off"));
}

void test_low_rail_also_means_off() {
  hold(OPEN_LOW, WEAR_DEBOUNCE_MS + 500);
  TEST_ASSERT_FALSE(g_state.worn);
}

void test_put_back_on() {
  hold(OPEN_HIGH, WEAR_DEBOUNCE_MS + 500);
  hold(ON_SKIN, WEAR_DEBOUNCE_MS + 500);
  TEST_ASSERT_TRUE(g_state.worn);
  TEST_ASSERT_EQUAL(1, countEvents("wear_off"));
  TEST_ASSERT_EQUAL(1, countEvents("wear_on"));
}

void test_brief_contact_loss_is_ignored() {
  for (int i = 0; i < 5; i++) {
    hold(OPEN_HIGH, WEAR_DEBOUNCE_MS - 1000);   // sleeve rubs, strap shifts
    hold(ON_SKIN, 600);
  }
  TEST_ASSERT_TRUE(g_state.worn);
  TEST_ASSERT_EQUAL(0, countEvents("wear_off"));
}

void test_band_edges_are_exclusive() {
  hold(WEAR_ADC_MIN, WEAR_DEBOUNCE_MS + 500);
  TEST_ASSERT_FALSE(g_state.worn);
  hold(WEAR_ADC_MIN + 1, WEAR_DEBOUNCE_MS + 500);
  TEST_ASSERT_TRUE(g_state.worn);
  hold(WEAR_ADC_MAX, WEAR_DEBOUNCE_MS + 500);
  TEST_ASSERT_FALSE(g_state.worn);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_electrode_is_on_adc1);
  RUN_TEST(test_starts_worn_without_events);
  RUN_TEST(test_removal_is_debounced);
  RUN_TEST(test_low_rail_also_means_off);
  RUN_TEST(test_put_back_on);
  RUN_TEST(test_brief_contact_loss_is_ignored);
  RUN_TEST(test_band_edges_are_exclusive);
  return UNITY_END();
}
