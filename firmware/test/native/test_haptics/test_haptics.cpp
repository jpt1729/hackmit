#include <unity.h>
#include "haptics.h"
#include "config.h"
#include "test_support.h"
#include <vector>

DeviceState g_state;

void setUp() {
  resetMocks();
  hapticsInit();
}
void tearDown() {}

// Tick every 1 ms for `ms` and record (elapsed, level) whenever the motor changes.
struct Edge { uint32_t t; int level; };
static std::vector<Edge> record(uint32_t ms) {
  std::vector<Edge> edges;
  int level = mock::pwm[PIN_VIBE];
  for (uint32_t t = 1; t <= ms; t++) {
    mock::advance(1);
    hapticsTick();
    if (mock::pwm[PIN_VIBE] != level) {
      level = mock::pwm[PIN_VIBE];
      edges.push_back({t, level});
    }
  }
  return edges;
}

void test_init_turns_motor_off() {
  TEST_ASSERT_EQUAL(0, mock::pwm[PIN_VIBE]);
}

void test_duty_is_a_nudge_not_full_power() {
  TEST_ASSERT_GREATER_THAN(0, VIBE_DUTY);
  TEST_ASSERT_LESS_THAN(255, VIBE_DUTY);
}

void test_gentle_pattern_timing() {
  hapticsGentle();
  TEST_ASSERT_EQUAL(VIBE_DUTY, mock::pwm[PIN_VIBE]);   // on immediately
  auto e = record(2000);
  TEST_ASSERT_EQUAL(3, e.size());
  TEST_ASSERT_EQUAL(400, e[0].t);  TEST_ASSERT_EQUAL(0, e[0].level);
  TEST_ASSERT_EQUAL(700, e[1].t);  TEST_ASSERT_EQUAL(VIBE_DUTY, e[1].level);
  TEST_ASSERT_EQUAL(1100, e[2].t); TEST_ASSERT_EQUAL(0, e[2].level);
}

void test_remind_pattern_timing() {
  hapticsRemind();
  auto e = record(2000);
  const uint32_t expectT[] = {200, 350, 550, 700, 900};
  const int      expectL[] = {0, VIBE_DUTY, 0, VIBE_DUTY, 0};
  TEST_ASSERT_EQUAL(5, e.size());
  for (int i = 0; i < 5; i++) {
    TEST_ASSERT_EQUAL(expectT[i], e[i].t);
    TEST_ASSERT_EQUAL(expectL[i], e[i].level);
  }
}

void test_idle_tick_does_not_touch_pin() {
  int before = mock::pwmWrites;
  record(500);
  TEST_ASSERT_EQUAL(before, mock::pwmWrites);
}

void test_restart_mid_pattern_ends_off() {
  hapticsGentle();
  record(200);
  hapticsRemind();                      // interrupt with the other pattern
  record(2000);
  TEST_ASSERT_EQUAL(0, mock::pwm[PIN_VIBE]);
}

void test_survives_millis_rollover() {
  mock::nowMs = 0xFFFFFF00;             // ~49.7 days of uptime, 256 ms before wrap
  hapticsGentle();
  auto e = record(2000);
  TEST_ASSERT_EQUAL(3, e.size());
  TEST_ASSERT_EQUAL(400, e[0].t);
  TEST_ASSERT_EQUAL(1100, e[2].t);
}

void test_slow_loop_still_finishes_pattern() {
  // If something stalls the loop (e.g. a slow HTTP client), the pattern
  // stretches but must still end with the motor off.
  hapticsGentle();
  for (int i = 0; i < 10; i++) { mock::advance(250); hapticsTick(); }
  TEST_ASSERT_EQUAL(0, mock::pwm[PIN_VIBE]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_init_turns_motor_off);
  RUN_TEST(test_duty_is_a_nudge_not_full_power);
  RUN_TEST(test_gentle_pattern_timing);
  RUN_TEST(test_remind_pattern_timing);
  RUN_TEST(test_idle_tick_does_not_touch_pin);
  RUN_TEST(test_restart_mid_pattern_ends_off);
  RUN_TEST(test_survives_millis_rollover);
  RUN_TEST(test_slow_loop_still_finishes_pattern);
  return UNITY_END();
}
