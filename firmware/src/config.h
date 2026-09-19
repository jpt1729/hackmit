#pragma once
#include "state.h"

#define WIFI_SSID     "your-hotspot-ssid"
#define WIFI_PASS     "your-hotspot-password"
#define NTP_SERVER    "pool.ntp.org"
#define TZ_OFFSET_SEC (-4 * 3600)

#ifndef ENABLE_WEAR
#define ENABLE_WEAR 1
#endif
#ifndef ENABLE_LOCATION
#define ENABLE_LOCATION 1
#endif
#ifndef ENABLE_DISPLAY
#define ENABLE_DISPLAY 1
#endif

#define PIN_SDA       21
#define PIN_SCL       22
#define MPU_ADDR      0x68
#define OLED_ADDR     0x3C
#define PIN_VIBE      25
#define VIBE_DUTY     153
#define PIN_ELECTRODE 34   // must be ADC1 (GPIO 32-39): ADC2 doesn't work while WiFi is on

#define ACT_SAMPLE_HZ       10
#define ACT_EMA_ALPHA       0.1f
#define MOVE_THRESH_G       0.06f
#define SLEEP_STILL_MIN     20
#define SHAKE_G             1.8f
#define SHAKE_COUNT         3
#define SHAKE_WINDOW_MS     1500
#define WANDER_START_H      0
#define WANDER_END_H        5
#define WANDER_COOLDOWN_MIN 30

#define WEAR_ADC_MIN     300
#define WEAR_ADC_MAX     3000
#define WEAR_DEBOUNCE_MS 5000

#define LOC_SCAN_INTERVAL_MS 15000
#define LOC_DEBOUNCE_SCANS   2
#define LOC_MIN_CONFIDENCE   20

struct APRef  { const char* bssid; int8_t rssi; };
struct RoomFP { Room room; uint8_t n; APRef aps[8]; };

// Placeholders: replace with tools/fingerprint_trainer.py output at the venue.
static const RoomFP ROOM_FPS[] = {
  { KITCHEN, 3, { {"AA:BB:CC:DD:EE:01", -45}, {"AA:BB:CC:DD:EE:02", -60}, {"AA:BB:CC:DD:EE:03", -72} } },
  { BEDROOM, 3, { {"AA:BB:CC:DD:EE:01", -70}, {"AA:BB:CC:DD:EE:02", -48}, {"AA:BB:CC:DD:EE:03", -55} } },
};
#define NUM_ROOM_FPS (sizeof(ROOM_FPS) / sizeof(ROOM_FPS[0]))

struct PromptDef { uint8_t hour, minute; const char* id; const char* label; Room room; };

static const PromptDef SCHEDULE[] = {
  {  9,  0, "meds_9am",  "Meds are in the kitchen", KITCHEN  },
  { 12, 30, "lunch",     "Time for lunch",          ANY_ROOM },
  { 21,  0, "wind_down", "Getting ready for bed",   ANY_ROOM },
};
#define SCHEDULE_LEN (sizeof(SCHEDULE) / sizeof(SCHEDULE[0]))

#define ACK_WINDOW_MS   60000
#define REBUZZ_AT_MS    30000
#define PROMPT_HOLD_MIN 30
