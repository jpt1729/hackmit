#include <unity.h>
#include "prompts.h"
#include "schedule.h"
#include "activity.h"
#include "buzzer.h"
#include "leds.h"
#include "config.h"
#include "test_support.h"

DeviceState g_state;

// Schedule indices, looked up so the tests survive reordering config.h.
static int MEDS, LUNCH;

void setUp() {
  resetMocks();
  mock::imuPresent = false;   // most tests drive g_state.activity by hand
  scheduleResetToDefaults();  // config.h defaults, not whatever a test pushed
  eventsInit();
  buzzerInit();
  ledsInit();
  ledsBootDone();
  activityInit();
  promptsInit();
  MEDS = promptsFindById("meds_9am");
  LUNCH = promptsFindById("lunch_checkin");
}
void tearDown() {}

// The subset of loop() the prompt engine interacts with.
static void loopTick() {
  activityTick();
  promptsTick();
  buzzerTick();
  ledsTick();
}
static void run(uint32_t ms) { runFor(ms, loopTick); }

static void withImu() {
  mock::imuPresent = true;
  activityInit();
}

static void shake() {
  for (int i = 0; i < SHAKE_COUNT; i++) {
    mock::accelX = 2.0f;
    run(100);
    mock::accelX = 0.0f;
    run(200);
  }
}

// ---------- lookup ----------

void test_find_by_id() {
  TEST_ASSERT_GREATER_OR_EQUAL(0, MEDS);
  TEST_ASSERT_GREATER_OR_EQUAL(0, LUNCH);
  TEST_ASSERT_EQUAL(-1, promptsFindById("nope"));
  TEST_ASSERT_EQUAL(-1, promptsFindById(""));
}

void test_schedule_ids_fit_event_detail() {
  for (uint8_t i = 0; i < scheduleCount(); i++)
    TEST_ASSERT_LESS_THAN_MESSAGE(PROMPT_ID_LEN, strlen(scheduleAt(i).id), scheduleAt(i).id);
}

// The OLED shows labels at text size 2: 10 chars per line, 3 lines above the footer.
void test_labels_fit_on_oled() {
  for (uint8_t i = 0; i < scheduleCount(); i++) {
    char buf[64];
    strlcpy(buf, scheduleAt(i).label, sizeof(buf));
    int lines = 1;
    size_t used = 0;
    for (char* w = strtok(buf, " "); w; w = strtok(nullptr, " ")) {
      size_t len = strlen(w);
      TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(10, len, scheduleAt(i).label);
      if (used && used + 1 + len > 10) { lines++; used = len; }
      else used += (used ? 1 : 0) + len;
    }
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(3, lines, scheduleAt(i).label);
  }
}

// ---------- firing ----------

void test_no_prompts_without_clock() {
  mock::epoch = 0;
  run(5000);
  TEST_ASSERT_EQUAL(0, countEvents("prompt_fired"));
  TEST_ASSERT_EQUAL(-1, g_state.pendingPrompt);
}

void test_not_before_slot() {
  mock::epoch = at(8, 59, 0);
  run(55000);
  TEST_ASSERT_EQUAL(0, countEvents("prompt_fired"));
}

void test_fires_at_slot_and_buzzes() {
  mock::epoch = at(8, 59, 58);
  run(3000);
  TEST_ASSERT_EQUAL(1, countEvents("prompt_fired", "meds_9am"));
  TEST_ASSERT_EQUAL(MEDS, g_state.pendingPrompt);
  TEST_ASSERT_GREATER_THAN(0, mock::toneWrites);
  TEST_ASSERT_EQUAL(LED_PROMPT, ledsMode());
}

void test_room_gate_holds_until_right_room() {
  g_state.room = BEDROOM;
  mock::epoch = at(9, 0);
  run(5 * 60000);
  TEST_ASSERT_EQUAL(0, countEvents("prompt_fired"));
  g_state.room = KITCHEN;
  run(2000);
  TEST_ASSERT_EQUAL(1, countEvents("prompt_fired", "meds_9am"));
}

void test_unknown_room_passes_gate() {
  g_state.room = ROOM_UNKNOWN;       // also what ENABLE_LOCATION 0 produces
  mock::epoch = at(9, 0);
  run(2000);
  TEST_ASSERT_EQUAL(1, countEvents("prompt_fired", "meds_9am"));
}

void test_any_room_prompt_ignores_room() {
  g_state.room = BEDROOM;
  mock::epoch = at(12, 30);
  run(2000);
  TEST_ASSERT_EQUAL(1, countEvents("prompt_fired", "lunch_checkin"));
}

void test_not_worn_holds_then_fires_on_wear() {
  g_state.worn = false;
  mock::epoch = at(9, 0);
  run(10 * 60000);
  TEST_ASSERT_EQUAL(0, countEvents("prompt_fired"));
  g_state.worn = true;
  run(2000);
  TEST_ASSERT_EQUAL(1, countEvents("prompt_fired", "meds_9am"));
}

void test_sleeping_holds() {
  g_state.activity = SLEEPING;
  mock::epoch = at(9, 0);
  run(10 * 60000);
  TEST_ASSERT_EQUAL(0, countEvents("prompt_fired"));
  g_state.activity = RESTING;
  run(2000);
  TEST_ASSERT_EQUAL(1, countEvents("prompt_fired"));
}

void test_held_past_window_is_missed_without_firing() {
  g_state.worn = false;
  mock::epoch = at(9, 0);
  run((PROMPT_HOLD_MIN + 2) * 60000UL);
  TEST_ASSERT_EQUAL(0, countEvents("prompt_fired"));
  TEST_ASSERT_EQUAL(1, countEvents("prompt_missed", "meds_9am"));
  g_state.worn = true;
  run(5000);
  TEST_ASSERT_EQUAL(0, countEvents("prompt_fired"));   // too late now
}

void test_away_from_home_holds_the_routine() {
  // Out for a walk: the midday lunch prompt waits until they are back, as long
  // as they return inside PROMPT_HOLD_MIN.
  g_state.awayFromHome = true;
  mock::epoch = at(12, 0);
  run(10 * 60000);
  TEST_ASSERT_EQUAL(0, countEvents("prompt_fired"));
  g_state.awayFromHome = false;
  run(2000);
  TEST_ASSERT_EQUAL(1, countEvents("prompt_fired", "lunch_checkin"));
}

// ---------- ack / rebuzz / miss ----------

void test_dashboard_ack_resolves_the_prompt() {
  mock::epoch = at(9, 0);
  run(2000);
  TEST_ASSERT_EQUAL(MEDS, g_state.pendingPrompt);
  TEST_ASSERT_TRUE(promptsAckPending());
  TEST_ASSERT_EQUAL(1, countEvents("prompt_acked", "meds_9am"));
  TEST_ASSERT_EQUAL(-1, g_state.pendingPrompt);
  run(1000);
  TEST_ASSERT_EQUAL(LED_ACK, ledsMode());
}

void test_dashboard_ack_with_nothing_pending() {
  TEST_ASSERT_FALSE(promptsAckPending());
  TEST_ASSERT_EQUAL(0, countEvents("prompt_acked"));
}

void test_shake_acks_pending_prompt() {
  withImu();
  mock::epoch = at(9, 0);
  run(2000);
  TEST_ASSERT_EQUAL(MEDS, g_state.pendingPrompt);
  run(5000);
  shake();
  run(100);
  TEST_ASSERT_EQUAL(1, countEvents("prompt_acked", "meds_9am"));
  TEST_ASSERT_EQUAL(0, countEvents("prompt_missed"));
  TEST_ASSERT_EQUAL(-1, g_state.pendingPrompt);
  run(10 * 60000);                                   // no refire same day
  TEST_ASSERT_EQUAL(1, countEvents("prompt_fired", "meds_9am"));
}

void test_shake_before_prompt_does_not_ack_it() {
  withImu();
  mock::epoch = at(8, 59, 58);
  shake();                                           // lands right before 9:00
  run((ACK_WINDOW_MS) + 3000);
  TEST_ASSERT_EQUAL(1, countEvents("prompt_fired", "meds_9am"));
  TEST_ASSERT_EQUAL(0, countEvents("prompt_acked"));
  TEST_ASSERT_EQUAL(1, countEvents("prompt_missed", "meds_9am"));
}

void test_rebuzz_once_mid_window() {
  mock::epoch = at(9, 0);
  run(1000);
  TEST_ASSERT_EQUAL(MEDS, g_state.pendingPrompt);
  run(5000);
  TEST_ASSERT_EQUAL(0, mock::toneFreq);              // first chime finished
  int on = 0;
  for (int i = 0; i < 6000; i++) {                   // watch 25s..55s after fire
    run(10);
    if (i > 1900 && mock::toneFreq > 0) on++;
  }
  TEST_ASSERT_GREATER_THAN(0, on);
  TEST_ASSERT_EQUAL(0, mock::toneFreq);
}

void test_no_ack_is_missed_after_window() {
  mock::epoch = at(9, 0);
  run(ACK_WINDOW_MS - 2000);
  TEST_ASSERT_EQUAL(0, countEvents("prompt_missed"));
  run(4000);
  TEST_ASSERT_EQUAL(1, countEvents("prompt_missed", "meds_9am"));
  TEST_ASSERT_EQUAL(-1, g_state.pendingPrompt);
  run(10 * 60000);
  TEST_ASSERT_EQUAL(1, countEvents("prompt_fired"));
}

// ---------- demo + scheduling ----------

void test_demo_fire_bypasses_time_and_gates() {
  mock::epoch = at(15, 0);
  run(2000);                          // let startup bookkeeping settle
  g_state.worn = false;
  g_state.activity = SLEEPING;
  promptsDemoFire(LUNCH);
  TEST_ASSERT_EQUAL(LUNCH, g_state.pendingPrompt);
  TEST_ASSERT_EQUAL(1, countEvents("prompt_fired", "lunch_checkin"));
}

void test_demo_fire_rejects_bad_index() {
  promptsDemoFire(-1);
  promptsDemoFire((int)scheduleCount());
  TEST_ASSERT_EQUAL(-1, g_state.pendingPrompt);
  TEST_ASSERT_EQUAL(0, countEvents("prompt_fired"));
}

void test_demo_fire_does_not_refire_at_slot() {
  mock::epoch = at(8, 0);
  run(2000);
  promptsDemoFire(MEDS);
  run(ACK_WINDOW_MS + 1000);          // let it get missed
  mock::epoch = at(9, 0);
  run(5000);
  TEST_ASSERT_EQUAL(1, countEvents("prompt_fired", "meds_9am"));
}

void test_one_prompt_at_a_time() {
  mock::epoch = at(8, 59, 50);
  run(2000);
  promptsDemoFire(LUNCH);             // occupies the slot across 9:00
  run(20000);
  TEST_ASSERT_EQUAL(0, countEvents("prompt_fired", "meds_9am"));
  run(ACK_WINDOW_MS);                 // lunch resolves -> meds gets its turn
  TEST_ASSERT_EQUAL(1, countEvents("prompt_missed", "lunch_checkin"));
  TEST_ASSERT_EQUAL(1, countEvents("prompt_fired", "meds_9am"));
}

void test_new_day_rearms_schedule() {
  mock::epoch = at(9, 0);
  run(ACK_WINDOW_MS + 2000);
  TEST_ASSERT_EQUAL(1, countEvents("prompt_fired", "meds_9am"));
  mock::epoch = at(9, 0) + 24 * 3600 - 5;   // next day, 08:59:55
  run(10000);
  TEST_ASSERT_EQUAL(2, countEvents("prompt_fired", "meds_9am"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_find_by_id);
  RUN_TEST(test_schedule_ids_fit_event_detail);
  RUN_TEST(test_labels_fit_on_oled);
  RUN_TEST(test_no_prompts_without_clock);
  RUN_TEST(test_not_before_slot);
  RUN_TEST(test_fires_at_slot_and_buzzes);
  RUN_TEST(test_room_gate_holds_until_right_room);
  RUN_TEST(test_unknown_room_passes_gate);
  RUN_TEST(test_any_room_prompt_ignores_room);
  RUN_TEST(test_not_worn_holds_then_fires_on_wear);
  RUN_TEST(test_sleeping_holds);
  RUN_TEST(test_held_past_window_is_missed_without_firing);
  RUN_TEST(test_away_from_home_holds_the_routine);
  RUN_TEST(test_dashboard_ack_resolves_the_prompt);
  RUN_TEST(test_dashboard_ack_with_nothing_pending);
  RUN_TEST(test_shake_acks_pending_prompt);
  RUN_TEST(test_shake_before_prompt_does_not_ack_it);
  RUN_TEST(test_rebuzz_once_mid_window);
  RUN_TEST(test_no_ack_is_missed_after_window);
  RUN_TEST(test_demo_fire_bypasses_time_and_gates);
  RUN_TEST(test_demo_fire_rejects_bad_index);
  RUN_TEST(test_demo_fire_does_not_refire_at_slot);
  RUN_TEST(test_one_prompt_at_a_time);
  RUN_TEST(test_new_day_rearms_schedule);
  return UNITY_END();
}
