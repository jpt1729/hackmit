#include <unity.h>
#include "events.h"
#include "schedule.h"
#include "test_support.h"

DeviceState g_state;

void setUp() {
  resetMocks();
  eventsInit();
}
void tearDown() {}

void test_empty_log_is_valid_json() {
  TEST_ASSERT_EQUAL_STRING("{\"events\":[]}", eventsJson().c_str());
}

void test_ids_are_monotonic_from_one() {
  TEST_ASSERT_EQUAL_UINT32(1, addEvent("wear_on", ""));
  TEST_ASSERT_EQUAL_UINT32(2, addEvent("wear_off", ""));
}

void test_json_matches_contract_shape() {
  mock::epoch = 1758290400;
  addEvent("prompt_fired", "meds_9am");
  TEST_ASSERT_EQUAL_STRING(
      "{\"events\":[{\"id\":1,\"ts\":1758290400,\"type\":\"prompt_fired\",\"detail\":\"meds_9am\"}]}",
      eventsJson().c_str());
}

void test_since_filters_older_events() {
  addEvent("wear_on", "");
  addEvent("room_change", "kitchen");
  addEvent("wander", "");
  std::string j = eventsJson(2);
  TEST_ASSERT_EQUAL(std::string::npos, j.find("\"id\":1,"));
  TEST_ASSERT_EQUAL(std::string::npos, j.find("\"id\":2,"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, j.find("\"id\":3,"));
  TEST_ASSERT_EQUAL_STRING("{\"events\":[]}", eventsJson(3).c_str());
}

void test_null_detail_becomes_empty_string() {
  addEvent("wander", nullptr);
  TEST_ASSERT_NOT_EQUAL(std::string::npos, eventsJson().find("\"detail\":\"\""));
}

void test_long_strings_are_truncated_not_overflowed() {
  std::string detail(PROMPT_ID_LEN + 20, 'd');
  addEvent("a_type_that_is_way_too_long", detail.c_str());
  std::string j = eventsJson();
  TEST_ASSERT_NOT_EQUAL(std::string::npos, j.find("\"type\":\"a_type_that_is_\""));   // 15 chars
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        j.find("\"detail\":\"" + std::string(PROMPT_ID_LEN - 1, 'd') + "\""));
}

// A prompt id has to survive the round trip to the dashboard intact: an event
// carrying a cut-off id matches no activity on the routine.
void test_a_full_length_prompt_id_is_not_truncated() {
  const char* id = "custom_f81d4fae-7dec-11d0-a765-00a0c91e6bf6";
  addEvent("prompt_acked", id);
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        eventsJson().find(std::string("\"detail\":\"") + id + "\""));
}

void test_every_contract_event_type_fits_unchanged() {
  const char* types[] = {"prompt_fired", "prompt_acked", "prompt_missed", "wear_on",
                         "wear_off", "room_change", "wander"};
  for (const char* t : types) {
    addEvent(t, "");
    TEST_ASSERT_EQUAL_MESSAGE(1, countEvents(t), t);
  }
}

void test_ring_buffer_keeps_newest_200() {
  for (int i = 0; i < 250; i++) addEvent("wander", "");
  std::string j = eventsJson();
  TEST_ASSERT_EQUAL(200, countEvents("wander"));
  TEST_ASSERT_EQUAL(std::string::npos, j.find("\"id\":50,"));       // dropped
  TEST_ASSERT_NOT_EQUAL(std::string::npos, j.find("\"id\":51,"));   // oldest kept
  TEST_ASSERT_NOT_EQUAL(std::string::npos, j.find("\"id\":250,"));  // newest
  TEST_ASSERT_TRUE(j.find("\"id\":51,") < j.find("\"id\":250,"));   // oldest first
}

void test_since_works_across_wraparound() {
  for (int i = 0; i < 450; i++) addEvent("wander", "");
  std::string j = eventsJson(440);
  int n = 0;
  for (size_t p = j.find("\"id\":"); p != std::string::npos; p = j.find("\"id\":", p + 1)) n++;
  TEST_ASSERT_EQUAL(10, n);
}

void test_full_buffer_json_size_is_bounded() {
  // Worst case response the ESP32 has to build in RAM for GET /events?since=0.
  for (int i = 0; i < 200; i++) addEvent("prompt_missed", "wind_down_long_detail__");
  size_t len = eventsJson().size();
  printf("full /events payload: %zu bytes\n", len);
  TEST_ASSERT_LESS_THAN(24000, len);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_empty_log_is_valid_json);
  RUN_TEST(test_ids_are_monotonic_from_one);
  RUN_TEST(test_json_matches_contract_shape);
  RUN_TEST(test_since_filters_older_events);
  RUN_TEST(test_null_detail_becomes_empty_string);
  RUN_TEST(test_long_strings_are_truncated_not_overflowed);
  RUN_TEST(test_a_full_length_prompt_id_is_not_truncated);
  RUN_TEST(test_every_contract_event_type_fits_unchanged);
  RUN_TEST(test_ring_buffer_keeps_newest_200);
  RUN_TEST(test_since_works_across_wraparound);
  RUN_TEST(test_full_buffer_json_size_is_bounded);
  return UNITY_END();
}
