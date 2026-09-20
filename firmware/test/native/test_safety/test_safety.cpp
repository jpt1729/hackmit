#include <unity.h>
#include "safety.h"
#include "buzzer.h"
#include "leds.h"
#include "config.h"
#include "test_support.h"

DeviceState g_state;

void setUp() {
  resetMocks();
  eventsInit();
  buzzerInit();
  ledsInit();
  ledsBootDone();
  safetyInit();
}
void tearDown() {}

static void run(uint32_t ms) {
  runFor(ms, [] { safetyTick(); buzzerTick(); ledsTick(); }, 50);
}

// Patterns run for up to a second, so a "was it quiet?" window has to start
// after the last one has finished playing.
static void settle() { run(1500); }

static bool sounded(uint32_t ms) {
  int before = mock::toneWrites;
  run(ms);
  return mock::toneWrites > before;
}

static int chimesDuring(uint32_t ms) {
  int before = safetyChimeCount();
  run(ms);
  return safetyChimeCount() - before;
}

void test_quiet_while_everything_is_normal() {
  TEST_ASSERT_FALSE(sounded(SAFETY_REPEAT_MS * 3));
  TEST_ASSERT_EQUAL(LED_IDLE, ledsMode());
}

void test_leaving_home_chimes_once_immediately() {
  g_state.awayFromHome = true;
  g_state.distanceHomeM = 210.0f;
  safetyTick();
  TEST_ASSERT_EQUAL(1, safetyChimeCount());
  TEST_ASSERT_TRUE(mock::toneFreq > 0);
  ledsTick();
  TEST_ASSERT_EQUAL(LED_ALERT, ledsMode());
}

void test_chimes_repeat_on_a_cooldown_then_stop() {
  g_state.awayFromHome = true;
  run(100);
  TEST_ASSERT_EQUAL(1, safetyChimeCount());

  TEST_ASSERT_EQUAL(0, chimesDuring(SAFETY_REPEAT_MS - 10000));   // not yet
  TEST_ASSERT_EQUAL(1, chimesDuring(11000));                      // now
  TEST_ASSERT_EQUAL(2, safetyChimeCount());

  run(SAFETY_REPEAT_MS * (SAFETY_MAX_CHIMES + 3));
  TEST_ASSERT_EQUAL(SAFETY_MAX_CHIMES, safetyChimeCount());
  settle();
  TEST_ASSERT_FALSE(sounded(SAFETY_REPEAT_MS * 2));       // it stops nagging
}

void test_silence_stops_the_chime_but_not_the_state() {
  g_state.awayFromHome = true;
  run(100);
  TEST_ASSERT_TRUE(mock::toneFreq > 0);
  safetySilence();
  TEST_ASSERT_EQUAL(0, mock::toneFreq);
  TEST_ASSERT_FALSE(sounded(SAFETY_REPEAT_MS * 3));
  TEST_ASSERT_TRUE(g_state.awayFromHome);                 // the alert stands
  ledsTick();
  TEST_ASSERT_EQUAL(LED_ALERT, ledsMode());
}

void test_coming_home_acknowledges_and_resets() {
  g_state.awayFromHome = true;
  run(SAFETY_REPEAT_MS + 1000);
  TEST_ASSERT_EQUAL(2, safetyChimeCount());
  settle();

  g_state.awayFromHome = false;
  TEST_ASSERT_TRUE(sounded(500));                         // "back home" blip
  TEST_ASSERT_EQUAL(0, safetyChimeCount());
  run(16000);                                             // the alert override expires
  TEST_ASSERT_EQUAL(LED_IDLE, ledsMode());

  // Leaving again starts a fresh episode, silence included.
  g_state.awayFromHome = true;
  TEST_ASSERT_TRUE(sounded(100));
  TEST_ASSERT_EQUAL(1, safetyChimeCount());
}

void test_night_wander_nudges_once_then_leaves_them_alone() {
  g_state.wanderFlag = true;
  TEST_ASSERT_TRUE(sounded(100));
  settle();
  TEST_ASSERT_FALSE(sounded(SAFETY_WANDER_MS - 30000));
  TEST_ASSERT_TRUE(sounded(31000));
}

void test_away_takes_priority_over_wander() {
  g_state.awayFromHome = true;
  g_state.wanderFlag = true;
  run(100);
  TEST_ASSERT_EQUAL(1, safetyChimeCount());   // one chime, not two overlapping
  ledsTick();
  TEST_ASSERT_EQUAL(LED_ALERT, ledsMode());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_quiet_while_everything_is_normal);
  RUN_TEST(test_leaving_home_chimes_once_immediately);
  RUN_TEST(test_chimes_repeat_on_a_cooldown_then_stop);
  RUN_TEST(test_silence_stops_the_chime_but_not_the_state);
  RUN_TEST(test_coming_home_acknowledges_and_resets);
  RUN_TEST(test_night_wander_nudges_once_then_leaves_them_alone);
  RUN_TEST(test_away_takes_priority_over_wander);
  return UNITY_END();
}
