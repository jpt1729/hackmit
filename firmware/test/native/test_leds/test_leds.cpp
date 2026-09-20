#include <unity.h>
#include "leds.h"
#include "config.h"
#include "test_support.h"

DeviceState g_state;

// The ring itself needs hardware, so these tests cover the part that decides
// what it should be showing.
void setUp() {
  resetMocks();
  ledsInit();
}
void tearDown() {}

static LedMode modeAfter(uint32_t ms = LED_FRAME_MS + 10) {
  runFor(ms, ledsTick, 10);
  return ledsMode();
}

void test_boot_animation_until_setup_finishes() {
  TEST_ASSERT_EQUAL(LED_BOOT, modeAfter());
  ledsBootDone();
  TEST_ASSERT_EQUAL(LED_IDLE, modeAfter());
}

void test_idle_when_worn_awake_and_nothing_due() {
  ledsBootDone();
  TEST_ASSERT_EQUAL(LED_IDLE, modeAfter());
}

void test_pending_prompt_shows_the_prompt_colour() {
  ledsBootDone();
  g_state.pendingPrompt = 0;
  TEST_ASSERT_EQUAL(LED_PROMPT, modeAfter());
}

void test_away_from_home_outranks_a_pending_prompt() {
  ledsBootDone();
  g_state.pendingPrompt = 0;
  g_state.awayFromHome = true;
  TEST_ASSERT_EQUAL(LED_ALERT, modeAfter());
}

void test_night_wander_is_an_alert() {
  ledsBootDone();
  g_state.wanderFlag = true;
  TEST_ASSERT_EQUAL(LED_ALERT, modeAfter());
}

void test_off_body_and_sleeping_are_distinct() {
  ledsBootDone();
  g_state.worn = false;
  TEST_ASSERT_EQUAL(LED_OFFBODY, modeAfter());

  g_state.worn = true;
  g_state.activity = SLEEPING;
  TEST_ASSERT_EQUAL(LED_NIGHT, modeAfter());
}

void test_flash_overrides_then_expires() {
  ledsBootDone();
  ledsFlash(LED_ACK, 3000);
  TEST_ASSERT_EQUAL(LED_ACK, modeAfter());
  TEST_ASSERT_EQUAL(LED_ACK, modeAfter(2000));
  TEST_ASSERT_EQUAL(LED_IDLE, modeAfter(1500));
}

void test_flash_wins_even_over_an_alert() {
  ledsBootDone();
  g_state.awayFromHome = true;
  ledsFlash(LED_ACK, 2000);
  TEST_ASSERT_EQUAL(LED_ACK, modeAfter());
  TEST_ASSERT_EQUAL(LED_ALERT, modeAfter(2500));
}

void test_frame_rate_is_capped() {
  // The ring redraw is a blocking bit-banged write; it must not run every loop.
  ledsBootDone();
  runFor(1000, ledsTick, 1);
  g_state.pendingPrompt = 0;
  ledsTick();                                  // too soon after the last frame
  TEST_ASSERT_EQUAL(LED_IDLE, ledsMode());
  TEST_ASSERT_EQUAL(LED_PROMPT, modeAfter());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_boot_animation_until_setup_finishes);
  RUN_TEST(test_idle_when_worn_awake_and_nothing_due);
  RUN_TEST(test_pending_prompt_shows_the_prompt_colour);
  RUN_TEST(test_away_from_home_outranks_a_pending_prompt);
  RUN_TEST(test_night_wander_is_an_alert);
  RUN_TEST(test_off_body_and_sleeping_are_distinct);
  RUN_TEST(test_flash_overrides_then_expires);
  RUN_TEST(test_flash_wins_even_over_an_alert);
  RUN_TEST(test_frame_rate_is_capped);
  return UNITY_END();
}
