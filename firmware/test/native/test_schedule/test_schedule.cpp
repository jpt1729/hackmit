#include <unity.h>
#include "schedule.h"
#include "config.h"
#include "prompts.h"
#include "test_support.h"

DeviceState g_state;

void setUp() {
  resetMocks();
  scheduleResetToDefaults();
}
void tearDown() {}

static bool push(const char* json, char* err = nullptr, size_t errLen = 0) {
  char scratch[96] = "";
  return scheduleReplace(json, err ? err : scratch, err ? errLen : sizeof(scratch));
}

// ---------------------------------------------------------------- defaults

void test_defaults_come_from_config_h() {
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_SCHEDULE_LEN, scheduleCount());
  TEST_ASSERT_FALSE(scheduleIsCustom());
  TEST_ASSERT_NOT_EQUAL(-1, scheduleFindById("meds_9am"));
  TEST_ASSERT_EQUAL(-1, scheduleFindById("nothing_like_this"));
}

// ---------------------------------------------------------------- accepting a routine

void test_push_replaces_the_whole_table() {
  TEST_ASSERT_TRUE(push("{\"items\":[{\"id\":\"tea\",\"label\":\"Tea\",\"hour\":7,\"minute\":5}]}"));
  TEST_ASSERT_EQUAL_UINT8(1, scheduleCount());
  TEST_ASSERT_TRUE(scheduleIsCustom());
  TEST_ASSERT_EQUAL_STRING("tea", scheduleAt(0).id);
  TEST_ASSERT_EQUAL_STRING("Tea", scheduleAt(0).label);
  TEST_ASSERT_EQUAL_UINT8(7, scheduleAt(0).hour);
  TEST_ASSERT_EQUAL_UINT8(5, scheduleAt(0).minute);
  // Reminders the caregiver did not pin to a room fire anywhere in the house.
  TEST_ASSERT_EQUAL_UINT8(ANY_ROOM, scheduleAt(0).room);
  // Everything from config.h is gone, not merged.
  TEST_ASSERT_EQUAL(-1, scheduleFindById("meds_9am"));
}

void test_items_are_sorted_by_time_of_day() {
  TEST_ASSERT_TRUE(push("{\"items\":["
                        "{\"id\":\"c\",\"label\":\"Late\",\"hour\":21,\"minute\":0},"
                        "{\"id\":\"a\",\"label\":\"Early\",\"hour\":7,\"minute\":30},"
                        "{\"id\":\"b\",\"label\":\"Noon\",\"hour\":12,\"minute\":0}]}"));
  TEST_ASSERT_EQUAL_STRING("a", scheduleAt(0).id);
  TEST_ASSERT_EQUAL_STRING("b", scheduleAt(1).id);
  TEST_ASSERT_EQUAL_STRING("c", scheduleAt(2).id);
}

void test_room_names_map_back_to_rooms() {
  TEST_ASSERT_TRUE(push("{\"items\":[{\"id\":\"m\",\"label\":\"Meds\",\"hour\":9,"
                        "\"minute\":0,\"room\":\"kitchen\"}]}"));
  TEST_ASSERT_EQUAL_UINT8(KITCHEN, scheduleAt(0).room);
}

void test_an_empty_routine_is_allowed() {
  // A caregiver who removes every activity gets a band that stays quiet, not
  // one that silently falls back to somebody else's medication times.
  TEST_ASSERT_TRUE(push("{\"items\":[]}"));
  TEST_ASSERT_EQUAL_UINT8(0, scheduleCount());
}

void test_dashboard_custom_ids_survive_intact() {
  // The routine editor mints `custom_<uuid>`; a truncated id would never match
  // an event back to the activity that produced it.
  const char* id = "custom_f81d4fae-7dec-11d0-a765-00a0c91e6bf6";
  char body[160];
  snprintf(body, sizeof(body),
           "{\"items\":[{\"id\":\"%s\",\"label\":\"Call a friend\",\"hour\":16,\"minute\":0}]}", id);
  TEST_ASSERT_TRUE(push(body));
  TEST_ASSERT_EQUAL_STRING(id, scheduleAt(0).id);
  TEST_ASSERT_EQUAL(0, scheduleFindById(id));
}

void test_escaped_characters_in_a_label() {
  TEST_ASSERT_TRUE(push("{\"items\":[{\"id\":\"x\",\"label\":\"Mum\\\"s pills\",\"hour\":9,\"minute\":0}]}"));
  TEST_ASSERT_EQUAL_STRING("Mum\"s pills", scheduleAt(0).label);
}

// ---------------------------------------------------------------- rejecting a routine

void test_a_bad_item_leaves_the_running_routine_untouched() {
  TEST_ASSERT_TRUE(push("{\"items\":[{\"id\":\"keep\",\"label\":\"Keep\",\"hour\":8,\"minute\":0}]}"));
  // Second item is broken: the first must not be applied on its own.
  TEST_ASSERT_FALSE(push("{\"items\":[{\"id\":\"new\",\"label\":\"New\",\"hour\":8,\"minute\":0},"
                         "{\"id\":\"bad\",\"label\":\"Bad\",\"hour\":99,\"minute\":0}]}"));
  TEST_ASSERT_EQUAL_UINT8(1, scheduleCount());
  TEST_ASSERT_EQUAL_STRING("keep", scheduleAt(0).id);
}

void test_rejections_explain_themselves() {
  char err[96];
  TEST_ASSERT_FALSE(push("{\"items\":[{\"id\":\"a\",\"label\":\"A\",\"hour\":24,\"minute\":0}]}", err, sizeof(err)));
  TEST_ASSERT_NOT_NULL(strstr(err, "hour"));

  TEST_ASSERT_FALSE(push("{\"items\":[{\"id\":\"a\",\"label\":\"A\",\"hour\":1,\"minute\":60}]}", err, sizeof(err)));
  TEST_ASSERT_NOT_NULL(strstr(err, "minute"));

  TEST_ASSERT_FALSE(push("{\"items\":[{\"id\":\"a\",\"label\":\"A\",\"hour\":1,\"minute\":0,"
                         "\"room\":\"garage\"}]}", err, sizeof(err)));
  TEST_ASSERT_NOT_NULL(strstr(err, "room"));
}

void test_duplicate_ids_are_refused() {
  TEST_ASSERT_FALSE(push("{\"items\":[{\"id\":\"a\",\"label\":\"A\",\"hour\":1,\"minute\":0},"
                         "{\"id\":\"a\",\"label\":\"B\",\"hour\":2,\"minute\":0}]}"));
}

void test_missing_fields_are_refused() {
  TEST_ASSERT_FALSE(push("{\"items\":[{\"id\":\"a\",\"hour\":1,\"minute\":0}]}"));       // no label
  TEST_ASSERT_FALSE(push("{\"items\":[{\"label\":\"A\",\"hour\":1,\"minute\":0}]}"));    // no id
  TEST_ASSERT_FALSE(push("{\"items\":[{\"id\":\"a\",\"label\":\"A\",\"hour\":1}]}"));    // no minute
  TEST_ASSERT_FALSE(push("{\"items\":[{\"id\":\"\",\"label\":\"A\",\"hour\":1,\"minute\":0}]}"));
  TEST_ASSERT_FALSE(push("{\"items\":[{\"id\":\"a\",\"label\":\"\",\"hour\":1,\"minute\":0}]}"));
}

void test_malformed_bodies_are_refused() {
  TEST_ASSERT_FALSE(push(""));
  TEST_ASSERT_FALSE(push("[]"));
  TEST_ASSERT_FALSE(push("{\"items\":[}"));
  TEST_ASSERT_FALSE(push("{\"items\":[{\"id\":\"a\",\"label\":\"A\",\"hour\":1,\"minute\":0}"));
  TEST_ASSERT_FALSE(push("{\"schedule\":[]}"));
  // Trailing junk means we did not understand the body, whatever parsed first.
  TEST_ASSERT_FALSE(push("{\"items\":[]} and then some"));
  // An id with punctuation would not survive a round trip through an event.
  TEST_ASSERT_FALSE(push("{\"items\":[{\"id\":\"a b\",\"label\":\"A\",\"hour\":1,\"minute\":0}]}"));
}

void test_more_than_the_band_holds_is_refused() {
  String body = "{\"items\":[";
  for (int i = 0; i <= SCHEDULE_MAX; i++) {
    char item[96];
    snprintf(item, sizeof(item), "%s{\"id\":\"i%d\",\"label\":\"L\",\"hour\":%d,\"minute\":%d}",
             i ? "," : "", i, i % 24, i % 60);
    body += item;
  }
  body += "]}";
  char err[96];
  TEST_ASSERT_FALSE(push(body.c_str(), err, sizeof(err)));
  TEST_ASSERT_NOT_NULL(strstr(err, "24"));
  TEST_ASSERT_EQUAL_UINT8(DEFAULT_SCHEDULE_LEN, scheduleCount());
}

// ---------------------------------------------------------------- serving it back

void test_schedule_json_round_trips() {
  const char* body = "{\"items\":[{\"id\":\"m\",\"label\":\"Meds\",\"hour\":9,\"minute\":0,"
                     "\"room\":\"kitchen\"}]}";
  TEST_ASSERT_TRUE(push(body));
  std::string json(scheduleJson().c_str());
  TEST_ASSERT_NOT_EQUAL(std::string::npos, json.find("\"source\":\"dashboard\""));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
      json.find("{\"id\":\"m\",\"label\":\"Meds\",\"hour\":9,\"minute\":0,\"room\":\"kitchen\"}"));

  // What GET /schedule serves must be something POST /schedule accepts back.
  TEST_ASSERT_TRUE(push(json.c_str()));
  TEST_ASSERT_EQUAL_UINT8(1, scheduleCount());
}

void test_defaults_report_themselves_as_defaults() {
  std::string json(scheduleJson().c_str());
  TEST_ASSERT_NOT_EQUAL(std::string::npos, json.find("\"source\":\"defaults\""));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_defaults_come_from_config_h);
  RUN_TEST(test_push_replaces_the_whole_table);
  RUN_TEST(test_items_are_sorted_by_time_of_day);
  RUN_TEST(test_room_names_map_back_to_rooms);
  RUN_TEST(test_an_empty_routine_is_allowed);
  RUN_TEST(test_dashboard_custom_ids_survive_intact);
  RUN_TEST(test_escaped_characters_in_a_label);
  RUN_TEST(test_a_bad_item_leaves_the_running_routine_untouched);
  RUN_TEST(test_rejections_explain_themselves);
  RUN_TEST(test_duplicate_ids_are_refused);
  RUN_TEST(test_missing_fields_are_refused);
  RUN_TEST(test_malformed_bodies_are_refused);
  RUN_TEST(test_more_than_the_band_holds_is_refused);
  RUN_TEST(test_schedule_json_round_trips);
  RUN_TEST(test_defaults_report_themselves_as_defaults);
  return UNITY_END();
}
