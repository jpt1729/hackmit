#include <unity.h>
#include "buzzer.h"
#include "config.h"
#include "test_support.h"
#include <vector>

DeviceState g_state;

void setUp() {
  resetMocks();
  buzzerInit();
}
void tearDown() {}

// Tick every 1 ms for `ms` and record (elapsed, frequency) on every change.
struct Edge { uint32_t t; int freq; };
static std::vector<Edge> record(uint32_t ms) {
  std::vector<Edge> edges;
  int freq = mock::toneFreq;
  for (uint32_t t = 1; t <= ms; t++) {
    mock::advance(1);
    buzzerTick();
    if (mock::toneFreq != freq) {
      freq = mock::toneFreq;
      edges.push_back({t, freq});
    }
  }
  return edges;
}

void test_init_is_silent() {
  TEST_ASSERT_EQUAL(0, mock::toneFreq);
  TEST_ASSERT_FALSE(buzzerBusy());
}

void test_volume_is_a_nudge_not_an_alarm() {
  // Duty is volume on a piezo and 50% is the loudest a square wave gets.
  const int half = 1 << (BUZZER_RES_BITS - 1);
  TEST_ASSERT_LESS_THAN(half, BUZZER_DUTY_GENTLE);
  TEST_ASSERT_LESS_THAN(half, BUZZER_DUTY_ALERT);
  TEST_ASSERT_LESS_THAN(BUZZER_DUTY_REMIND, BUZZER_DUTY_GENTLE);
  TEST_ASSERT_LESS_THAN(BUZZER_DUTY_ALERT, BUZZER_DUTY_REMIND);
}

void test_gentle_is_two_rising_notes() {
  buzzerGentle();
  TEST_ASSERT_EQUAL(988, mock::toneFreq);              // sounds immediately
  TEST_ASSERT_EQUAL(BUZZER_DUTY_GENTLE, mock::toneDuty);
  auto e = record(2000);
  TEST_ASSERT_EQUAL(3, e.size());
  TEST_ASSERT_EQUAL(200, e[0].t);  TEST_ASSERT_EQUAL(0, e[0].freq);     // rest
  TEST_ASSERT_EQUAL(320, e[1].t);  TEST_ASSERT_EQUAL(1319, e[1].freq);  // second note
  TEST_ASSERT_EQUAL(640, e[2].t);  TEST_ASSERT_EQUAL(0, e[2].freq);     // done
  TEST_ASSERT_FALSE(buzzerBusy());
}

void test_remind_is_three_notes_and_louder() {
  buzzerRemind();
  TEST_ASSERT_EQUAL(BUZZER_DUTY_REMIND, mock::toneDuty);
  auto e = record(2000);
  const uint32_t expectT[] = {160, 280, 440, 560, 880};
  const int      expectF[] = {0, 1319, 0, 1319, 0};
  TEST_ASSERT_EQUAL(5, e.size());
  for (int i = 0; i < 5; i++) {
    TEST_ASSERT_EQUAL(expectT[i], e[i].t);
    TEST_ASSERT_EQUAL(expectF[i], e[i].freq);
  }
}

void test_alert_alternates_two_tones_without_gaps() {
  buzzerAlert();
  TEST_ASSERT_EQUAL(BUZZER_DUTY_ALERT, mock::toneDuty);
  auto e = record(2000);
  TEST_ASSERT_EQUAL(4, e.size());
  TEST_ASSERT_EQUAL(1568, e[0].freq);
  TEST_ASSERT_EQUAL(1047, e[1].freq);
  TEST_ASSERT_EQUAL(1568, e[2].freq);
  TEST_ASSERT_EQUAL(0, e[3].freq);
  TEST_ASSERT_EQUAL(880, e[3].t);
}

void test_idle_tick_does_not_touch_the_pin() {
  int before = mock::toneWrites;
  record(500);
  TEST_ASSERT_EQUAL(before, mock::toneWrites);
}

void test_restart_mid_pattern_ends_silent() {
  buzzerGentle();
  record(100);
  buzzerRemind();                       // interrupt with the other pattern
  record(2000);
  TEST_ASSERT_EQUAL(0, mock::toneFreq);
  TEST_ASSERT_FALSE(buzzerBusy());
}

void test_stop_silences_immediately() {
  buzzerAlert();
  record(100);
  buzzerStop();
  TEST_ASSERT_EQUAL(0, mock::toneFreq);
  TEST_ASSERT_FALSE(buzzerBusy());
  record(1000);
  TEST_ASSERT_EQUAL(0, mock::toneFreq);
}

void test_survives_millis_rollover() {
  mock::nowMs = 0xFFFFFF00;             // ~49.7 days of uptime, 256 ms before wrap
  buzzerGentle();
  auto e = record(2000);
  TEST_ASSERT_EQUAL(3, e.size());
  TEST_ASSERT_EQUAL(200, e[0].t);
  TEST_ASSERT_EQUAL(640, e[2].t);
}

void test_slow_loop_still_finishes_pattern() {
  // If something stalls the loop (a slow HTTP client, a long I2C write), the
  // pattern stretches but must still end silent.
  buzzerGentle();
  for (int i = 0; i < 10; i++) { mock::advance(250); buzzerTick(); }
  TEST_ASSERT_EQUAL(0, mock::toneFreq);
  TEST_ASSERT_FALSE(buzzerBusy());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_init_is_silent);
  RUN_TEST(test_volume_is_a_nudge_not_an_alarm);
  RUN_TEST(test_gentle_is_two_rising_notes);
  RUN_TEST(test_remind_is_three_notes_and_louder);
  RUN_TEST(test_alert_alternates_two_tones_without_gaps);
  RUN_TEST(test_idle_tick_does_not_touch_the_pin);
  RUN_TEST(test_restart_mid_pattern_ends_silent);
  RUN_TEST(test_stop_silences_immediately);
  RUN_TEST(test_survives_millis_rollover);
  RUN_TEST(test_slow_loop_still_finishes_pattern);
  return UNITY_END();
}
