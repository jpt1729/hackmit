#pragma once

#include <stdint.h>

// Placeholder config for the Routine Anchor build spec.
#define WIFI_SSID "your_wifi"
#define WIFI_PASSWORD "your_password"

#define PROMPT_INTERVAL_MS 30000UL
#define ROOM_SCAN_INTERVAL_MS 15000UL

enum Activity {
  SLEEPING,
  RESTING,
  MOVING
};

enum Room {
  UNKNOWN,
  KITCHEN,
  BEDROOM,
  LIVING
};

struct PromptScheduleItem {
  uint8_t hour;
  uint8_t minute;
  const char* label;
  Room requiredRoom;
};

const PromptScheduleItem kPromptSchedule[] = {
  {9, 0, "meds_9am", Room::KITCHEN},
  {12, 0, "lunch_checkin", Room::ANY},
  {18, 30, "evening_prompt", Room::LIVING}
};

// NOTE: The project README uses a schedule table whose room can be ANY/UNKNOWN; this placeholder
// intentionally keeps the type simple while the firmware is being scaffolded.
