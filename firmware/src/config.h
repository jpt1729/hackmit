#pragma once
#include "state.h"

#define WIFI_SSID     "your-hotspot-ssid"
#define WIFI_PASS     "your-hotspot-password"
#define NTP_SERVER    "pool.ntp.org"
#define TZ_OFFSET_SEC (-4 * 3600)

// Placeholder config for the Granny Nanny build spec.
#define WIFI_SSID "your_wifi"
#define WIFI_PASSWORD "your_password"
#ifndef ENABLE_WEAR
#define ENABLE_WEAR 1
#endif
#ifndef ENABLE_LOCATION
#define ENABLE_LOCATION 1
#endif
#ifndef ENABLE_DISPLAY
#define ENABLE_DISPLAY 1
#endif
#ifndef ENABLE_LEDS
#define ENABLE_LEDS 1
#endif
#ifndef ENABLE_GPS
#define ENABLE_GPS 1
#endif

// ---------------------------------------------------------------- pins
#define PIN_SDA       21   // MPU-6050 + SSD1306 share this bus
#define PIN_SCL       22
#define MPU_ADDR      0x68
#define OLED_ADDR     0x3C
#define PIN_BUZZER    26   // piezo + 100 R to GND
#define PIN_LEDS      27   // NeoPixel ring DIN through 330 R
#define PIN_GPS_RX    16   // ESP32 RX2  <- GT-U7 TX
#define PIN_GPS_TX    17   // ESP32 TX2  -> GT-U7 RX (config only; unused in normal operation)

// ---------------------------------------------------------------- activity (MPU-6050)
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

// ---------------------------------------------------------------- fall detection (MPU-6050)
// A real fall is three phases, not one spike: the wrist goes light on the way
// down, slams, then stops moving. Requiring all three is what keeps setting a
// mug down hard or dropping into an armchair from calling an ambulance.
#define FALL_FREEFALL_G     0.55f    // below 1g: the arm is unloaded, falling
#define FALL_IMPACT_G       2.20f    // the landing spike
#define FALL_WINDOW_MS      900      // freefall must be followed by impact this fast
#define FALL_STILL_G        0.045f   // smoothed motion under this counts as "not moving"
#define FALL_SETTLE_MS      1200     // let the impact spike decay out of the average first
#define FALL_STILL_MS       2500     // stillness this long after impact confirms it
#define FALL_REARM_MS       10000    // ignore new candidates this long after one fires

// The escalation ladder. Nothing auto-dials anyone until both timers expire
// and every prompt to cancel has gone unanswered.
#define FALL_CANCEL_MS      30000    // "are you OK? shake to cancel" window
#define FALL_EMS_MS         60000    // caregiver alerted this long with no answer -> EMS
#define FALL_CHIME_MS       8000     // re-chime cadence while a fall is unresolved

// ---------------------------------------------------------------- caregiver messages (OLED)
#define MSG_MAX_LEN         96       // longer messages are truncated on the wrist
#define MSG_TTL_MS          600000   // unread this long: stop holding the screen

// ---------------------------------------------------------------- wear (inferred from the IMU)
// No electrode in the BOM: a worn device always shows micro-motion (breathing,
// pulse, tiny drift). A device on a table is dead still. That is the signal.
#define WEAR_MICRO_G       0.012f   // above this = a body is attached
#define WEAR_SAMPLE_MS     500
#define WEAR_ON_DEBOUNCE_MS  3000   // motion this long -> worn
#define WEAR_OFF_STILL_MS  300000   // 5 min of dead stillness -> taken off

// ---------------------------------------------------------------- buzzer (piezo)
// Passive piezo (the bare disc / 2-pin module): driven with LEDC tones.
// Set to 0 for an active buzzer module, which has its own oscillator and only
// takes on/off - the same patterns then play at whatever pitch it has.
#define BUZZER_PASSIVE      1
#define BUZZER_RES_BITS     10      // duty range 0..1023
#define BUZZER_DUTY_GENTLE  60      // ~6%: a chime across the room, not an alarm
#define BUZZER_DUTY_REMIND  110
#define BUZZER_DUTY_ALERT   240
#define BUZZER_LEDC_CHANNEL 0

// ---------------------------------------------------------------- LED ring (NeoPixel)
#define LED_COUNT          12
#define LED_BRIGHTNESS     40      // 0-255. Dim on purpose: it is a night-time device.
#define LED_FRAME_MS       40      // 25 fps animation tick
#define LED_ACK_FLASH_MS   3000
#define LED_PROGRESS_MIN_V 12      // unfinished pixels glow this dim, so 0/N still reads as a ring

// ---------------------------------------------------------------- location (WiFi RSSI)
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

// ---------------------------------------------------------------- GPS (GT-U7 / NEO-6M)
#define GPS_BAUD            9600
#define GPS_STALE_MS        15000   // no valid sentence for this long -> fix is stale
// Geofence: home is the centre, the radius is "still in the garden".
#define HOME_LAT            42.360100    // replace on site: read GET /state once outside
#define HOME_LON           -71.094200
#define GEOFENCE_RADIUS_M   80.0f
#define GEOFENCE_HYST_M     25.0f   // must come back this far inside before "returned"
#define GEOFENCE_CONFIRM    3       // consecutive fixes agreeing before an event
#define GPS_MIN_SATS        4

// ---------------------------------------------------------------- safety chimes
#define SAFETY_REPEAT_MS    120000   // while away from home, re-chime this often
#define SAFETY_MAX_CHIMES   5        // then stop nagging; the caregiver has the alert
#define SAFETY_WANDER_MS    300000   // night-wander chime cooldown

// ---------------------------------------------------------------- prompts
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
