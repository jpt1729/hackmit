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
  g_state.tasksTotal = 0;   // no routine loaded: the ring falls back to idle
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


// ---------------------------------------------------------------- ring progress

void test_a_loaded_routine_shows_progress_instead_of_idle() {
  ledsBootDone();
  g_state.tasksTotal = 3;
  g_state.tasksDone = 1;
  TEST_ASSERT_EQUAL(LED_PROGRESS, modeAfter());
}

void test_progress_yields_to_a_prompt_and_an_alert() {
  ledsBootDone();
  g_state.tasksTotal = 3;
  g_state.pendingPrompt = 0;
  TEST_ASSERT_EQUAL(LED_PROMPT, modeAfter());
  g_state.awayFromHome = true;
  TEST_ASSERT_EQUAL(LED_ALERT, modeAfter());
}

void test_a_fall_outranks_everything_else() {
  ledsBootDone();
  g_state.tasksTotal = 3;
  g_state.tasksDone = 3;
  g_state.fallStage = FALL_CONFIRMING;
  TEST_ASSERT_EQUAL(LED_ALERT, modeAfter());
}

void test_progress_yields_to_sleep_and_to_a_bare_wrist() {
  ledsBootDone();
  g_state.tasksTotal = 3;
  g_state.activity = SLEEPING;
  TEST_ASSERT_EQUAL(LED_NIGHT, modeAfter());
  g_state.worn = false;
  TEST_ASSERT_EQUAL(LED_OFFBODY, modeAfter());
}

void test_an_empty_day_lights_no_pixels() {
  TEST_ASSERT_EQUAL(0, ledsProgressPixels(0, 3));
  TEST_ASSERT_EQUAL(0, ledsProgressPixels(0, 12));
}

void test_a_finished_day_lights_the_whole_ring() {
  TEST_ASSERT_EQUAL(LED_COUNT, ledsProgressPixels(3, 3));
  TEST_ASSERT_EQUAL(LED_COUNT, ledsProgressPixels(12, 12));
}

// One done out of twelve rounds to zero, but the wearer did something - it has
// to light a pixel or the ring is lying to them.
void test_one_task_done_always_lights_at_least_one_pixel() {
  TEST_ASSERT_EQUAL(1, ledsProgressPixels(1, 12));
  TEST_ASSERT_EQUAL(1, ledsProgressPixels(1, 40));
}

// The mirror image: nearly-done must not read as finished.
void test_an_unfinished_day_never_fills_the_ring() {
  TEST_ASSERT_EQUAL(LED_COUNT - 1, ledsProgressPixels(11, 12));
  TEST_ASSERT_EQUAL(LED_COUNT - 1, ledsProgressPixels(39, 40));
}

void test_progress_scales_between_the_ends() {
  TEST_ASSERT_EQUAL(4, ledsProgressPixels(1, 3));
  TEST_ASSERT_EQUAL(8, ledsProgressPixels(2, 3));
  TEST_ASSERT_EQUAL(6, ledsProgressPixels(6, 12));
}

void test_no_routine_means_no_pixels_and_no_divide_by_zero() {
  TEST_ASSERT_EQUAL(0, ledsProgressPixels(0, 0));
  TEST_ASSERT_EQUAL(0, ledsProgressPixels(5, 0));
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
  RUN_TEST(test_a_loaded_routine_shows_progress_instead_of_idle);
  RUN_TEST(test_progress_yields_to_a_prompt_and_an_alert);
  RUN_TEST(test_a_fall_outranks_everything_else);
  RUN_TEST(test_progress_yields_to_sleep_and_to_a_bare_wrist);
  RUN_TEST(test_an_empty_day_lights_no_pixels);
  RUN_TEST(test_a_finished_day_lights_the_whole_ring);
  RUN_TEST(test_one_task_done_always_lights_at_least_one_pixel);
  RUN_TEST(test_an_unfinished_day_never_fills_the_ring);
  RUN_TEST(test_progress_scales_between_the_ends);
  RUN_TEST(test_no_routine_means_no_pixels_and_no_divide_by_zero);
  return UNITY_END();
}
