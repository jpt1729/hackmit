#include <unity.h>
#include "message.h"
#include "activity.h"
#include "config.h"
#include "test_support.h"
#include <string.h>

DeviceState g_state;

static const uint32_t SAMPLE_MS = 1000 / ACT_SAMPLE_HZ;

void setUp() {
  resetMocks();
  eventsInit();
  activityInit();
  messageInit();
  g_state.worn = true;
}
void tearDown() {}

static void tickBoth() {
  activityTick();
  messageTick();
}

static void hold(float g, uint32_t ms) {
  mock::accelX = 0; mock::accelY = 0; mock::accelZ = g;
  runFor(ms, tickBoth, SAMPLE_MS);
}

static void shake() {
  for (int i = 0; i < SHAKE_COUNT + 1; i++) {
    hold(2.5f, SAMPLE_MS * 2);
    hold(1.0f, SAMPLE_MS * 2);
  }
}

void test_nothing_waiting_at_boot() {
  TEST_ASSERT_FALSE(messagePending());
  TEST_ASSERT_EQUAL_STRING("", messageText());
}

void test_a_message_is_held_until_it_is_read() {
  messageSet("Lunch is in the fridge");
  TEST_ASSERT_TRUE(messagePending());
  TEST_ASSERT_EQUAL_STRING("Lunch is in the fridge", messageText());
  TEST_ASSERT_EQUAL(1, countEvents("message_received"));
  hold(1.0f, 5000);
  TEST_ASSERT_TRUE(messagePending());
}

void test_shake_clears_it() {
  messageSet("Call me when you can");
  shake();
  TEST_ASSERT_FALSE(messagePending());
  TEST_ASSERT_EQUAL(1, countEvents("message_read"));
}

void test_empty_messages_are_rejected() {
  messageSet("");
  TEST_ASSERT_FALSE(messagePending());
  messageSet(nullptr);
  TEST_ASSERT_FALSE(messagePending());
  TEST_ASSERT_EQUAL(0, countEvents("message_received"));
}

void test_a_new_message_replaces_the_old_one() {
  messageSet("First");
  messageSet("Second");
  TEST_ASSERT_EQUAL_STRING("Second", messageText());
  TEST_ASSERT_EQUAL(2, countEvents("message_received"));
}

void test_long_messages_are_truncated_not_overflowed() {
  char longText[MSG_MAX_LEN * 2];
  memset(longText, 'a', sizeof(longText) - 1);
  longText[sizeof(longText) - 1] = 0;
  messageSet(longText);
  TEST_ASSERT_EQUAL(MSG_MAX_LEN, strlen(messageText()));
}

// The wearer may be asleep or out of the room; the note must not own the
// screen for the rest of the day.
void test_an_unread_message_expires() {
  messageSet("Back at six");
  runFor(MSG_TTL_MS + 1000, tickBoth, 1000);
  TEST_ASSERT_FALSE(messagePending());
  TEST_ASSERT_EQUAL(1, countEvents("message_expired"));
  TEST_ASSERT_EQUAL(0, countEvents("message_read"));
}

// A shake during a fall means "I am fine", not "I read your note".
void test_a_fall_keeps_the_shake_away_from_the_message() {
  messageSet("Hello");
  g_state.fallStage = FALL_CONFIRMING;
  shake();
  TEST_ASSERT_TRUE(messagePending());
  TEST_ASSERT_EQUAL(0, countEvents("message_read"));
}

// Same for a live prompt: the shake acknowledges the medication.
void test_a_pending_prompt_keeps_the_shake_away_from_the_message() {
  messageSet("Hello");
  g_state.pendingPrompt = 0;
  shake();
  TEST_ASSERT_TRUE(messagePending());
  TEST_ASSERT_EQUAL(0, countEvents("message_read"));
}

void test_dismiss_is_harmless_when_nothing_is_waiting() {
  messageDismiss();
  TEST_ASSERT_EQUAL(0, countEvents("message_read"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_nothing_waiting_at_boot);
  RUN_TEST(test_a_message_is_held_until_it_is_read);
  RUN_TEST(test_shake_clears_it);
  RUN_TEST(test_empty_messages_are_rejected);
  RUN_TEST(test_a_new_message_replaces_the_old_one);
  RUN_TEST(test_long_messages_are_truncated_not_overflowed);
  RUN_TEST(test_an_unread_message_expires);
  RUN_TEST(test_a_fall_keeps_the_shake_away_from_the_message);
  RUN_TEST(test_a_pending_prompt_keeps_the_shake_away_from_the_message);
  RUN_TEST(test_dismiss_is_harmless_when_nothing_is_waiting);
  return UNITY_END();
}
