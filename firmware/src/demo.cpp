// Showcase build: the band with nothing plugged into it but power.
//
// No WiFi, no IMU, no satellites - just a timeline that pushes g_state through
// every situation the firmware knows about and lets the real ledsTick() and
// displayTick() draw it. Nothing here re-implements an animation or a screen,
// so what the ring and the OLED do in this demo is what they do in the field.
//
//   pio run -e esp32dev_demo -t upload && pio device monitor
//
// The monitor narrates each scene, so someone watching the wrist has a caption.

#ifdef DEMO_MODE

#include <Arduino.h>
#include <sys/time.h>

#include "config.h"
#include "events.h"
#include "gps.h"
#include "buzzer.h"
#include "leds.h"
#include "schedule.h"
#include "message.h"
#include "display.h"

DeviceState g_state;     // main.cpp is out of this build, so the demo owns it

#ifndef DEMO_BUZZER
#define DEMO_BUZZER 1    // -DDEMO_BUZZER=0 for a silent demo in a quiet room
#endif

// 2026-09-20 00:00 UTC. The clock runs in UTC with no offset, so an epoch set
// here reads straight back out of localtime_r() as the hour we asked for.
#define DEMO_DAY_EPOCH 1789862400L

// A point 140 m north-west of home: far enough outside GEOFENCE_RADIUS_M that
// the real geofence in gps.cpp flips, rather than the demo setting the flag.
#define DEMO_AWAY_LAT (HOME_LAT + 0.00089)
#define DEMO_AWAY_LON (HOME_LON - 0.00120)

// ---------------------------------------------------------------- fake inputs

static void setClock(int hour, int minute) {
  struct timeval tv = { DEMO_DAY_EPOCH + hour * 3600L + minute * 60L, 0 };
  settimeofday(&tv, nullptr);
}

// Hand the GPS parser one synthetic GGA sentence, checksum and all. Feeding
// the parser rather than writing g_state.lat means the distance, the compass
// heading and the geofence debounce are all computed by the shipping code.
static void feedFix(double lat, double lon, uint8_t sats) {
  char body[96];
  int  latDeg = (int)fabs(lat), lonDeg = (int)fabs(lon);
  double latMin = (fabs(lat) - latDeg) * 60.0, lonMin = (fabs(lon) - lonDeg) * 60.0;
  snprintf(body, sizeof(body),
           "GPGGA,120000.00,%02d%07.4f,%c,%03d%07.4f,%c,1,%02u,1.0,10.0,M,0.0,M,,",
           latDeg, latMin, lat >= 0 ? 'N' : 'S',
           lonDeg, lonMin, lon >= 0 ? 'E' : 'W', sats);

  uint8_t sum = 0;
  for (const char* p = body; *p; p++) sum ^= (uint8_t)*p;

  char line[110];
  snprintf(line, sizeof(line), "$%s*%02X\r\n", body, sum);
  for (const char* p = line; *p; p++) gpsFeed(*p);
}

// GEOFENCE_CONFIRM fixes in a row are what it takes to move the flag, so send
// a few. Repeating them also keeps the fix off GPS_STALE_MS.
static void feedFixes(double lat, double lon) {
  for (uint8_t i = 0; i < GEOFENCE_CONFIRM + 1; i++) feedFix(lat, lon, 7);
}

static void chime(void (*tone)()) {
#if DEMO_BUZZER
  tone();
#else
  (void)tone;
#endif
}

// ---------------------------------------------------------------- the scenes

struct Scene {
  const char* title;
  const char* ring;                  // caption for the monitor
  const char* screen;
  uint32_t    ms;
  void      (*enter)();
  void      (*tick)(uint32_t elapsedMs);
};

// Every scene starts from a worn, quiet, nothing-due band and adds one thing.
static void baseline() {
  g_state.activity       = RESTING;
  g_state.room           = ROOM_UNKNOWN;
  g_state.roomConfidence = 0;
  g_state.worn           = true;
  g_state.wanderFlag     = false;
  g_state.pendingPrompt  = -1;
  g_state.tasksDone      = 0;
  g_state.tasksTotal     = 0;
  g_state.fallStage      = FALL_NONE;
  messageInit();
  setClock(9, 58);
}

static void sceneIdle() {
  g_state.room = KITCHEN;
  g_state.roomConfidence = 82;
}

static void sceneProgress() {
  g_state.room = KITCHEN;
  g_state.tasksTotal = 3;
  g_state.tasksDone  = 1;
}

static void scenePrompt() {
  setClock(9, 0);
  g_state.room = KITCHEN;
  g_state.tasksTotal = 3;
  g_state.pendingPrompt = 0;          // "Meds are in the kitchen"
  chime(buzzerGentle);
}

// The second chime at REBUZZ_AT_MS, on the demo's shorter clock.
static void promptTick(uint32_t elapsed) {
  static bool rebuzzed = false;
  if (elapsed < 4500) { rebuzzed = false; return; }
  if (!rebuzzed) { rebuzzed = true; chime(buzzerRemind); }
}

static void sceneAck() {
  g_state.room = KITCHEN;
  g_state.tasksTotal = 3;
  g_state.tasksDone  = 2;
  chime(buzzerAck);
  ledsFlash(LED_ACK, 3500);
  displayFlash("Done", 3500);
}

static void sceneMessage() {
  g_state.room = LIVING;
  g_state.tasksTotal = 3;
  g_state.tasksDone  = 2;
  messageSet("Ellie is coming over at four. Love, Dad");
}

static void sceneAllDone() {
  setClock(19, 40);
  g_state.room = LIVING;
  g_state.tasksTotal = 3;
  g_state.tasksDone  = 3;
}

static void sceneAway() {
  setClock(15, 20);
  feedFixes(DEMO_AWAY_LAT, DEMO_AWAY_LON);
  chime(buzzerAlert);
}

// Keep the fix fresh: gpsFixValid() ages out at GPS_STALE_MS and the heading
// on the screen goes with it.
static void awayTick(uint32_t elapsed) {
  static uint32_t last = 0;
  if (elapsed - last < 2000) return;
  last = elapsed;
  feedFix(DEMO_AWAY_LAT, DEMO_AWAY_LON, 7);
}

static void sceneHome() {
  setClock(15, 34);
  feedFixes(HOME_LAT, HOME_LON);
  g_state.room = LIVING;
}

static void sceneWander() {
  setClock(3, 12);
  g_state.activity = MOVING;
  g_state.room = BEDROOM;
  g_state.wanderFlag = true;
  chime(buzzerAlert);
}

static void sceneOffBody() {
  setClock(22, 5);
  g_state.worn = false;
}

static void sceneNight() {
  setClock(2, 30);
  g_state.room = BEDROOM;
  g_state.activity = SLEEPING;
}

static void sceneFallConfirm() {
  setClock(16, 2);
  g_state.fallStage = FALL_CONFIRMING;
  chime(buzzerAlert);
}

static void sceneFallCaregiver() {
  setClock(16, 2);
  g_state.fallStage = FALL_CAREGIVER;
  chime(buzzerAlert);
}

static void sceneFallEms() {
  setClock(16, 3);
  g_state.fallStage = FALL_EMS;
  chime(buzzerAlert);
}

static const Scene SCENES[] = {
  { "Morning, at home",   "teal breathe",        "clock, mascot, kitchen",     6000,  sceneIdle,          nullptr    },
  { "One task done",      "4 of 12 px green",    "clock, mascot",              5000,  sceneProgress,      nullptr    },
  { "9am reminder",       "amber comet",         "Meds are in the kitchen",    9000,  scenePrompt,        promptTick },
  { "Shaken to confirm",  "green wash",          "Done",                       4000,  sceneAck,           nullptr    },
  { "Caregiver message",  "green flash, then 8", "Ellie is coming over...",    8000,  sceneMessage,       nullptr    },
  { "Routine finished",   "full ring, breathing","clock, mascot",              6000,  sceneAllDone,       nullptr    },
  { "Outside the fence",  "red pulse",           "home 140m SE",               9000,  sceneAway,          awayTick   },
  { "Back home",          "teal breathe again",  "clock, mascot, living",      5000,  sceneHome,          nullptr    },
  { "3am wandering",      "red pulse",           "03:12, bedroom",             6000,  sceneWander,        nullptr    },
  { "Taken off",          "one dim blue dot",    "not on wrist",               5000,  sceneOffBody,       nullptr    },
  { "Asleep, night light","dim warm glow",       "02:30, bedroom",             6000,  sceneNight,         nullptr    },
  { "Fall: are you OK?",  "red pulse",           "Are you OK?",                6000,  sceneFallConfirm,   nullptr    },
  { "Fall: no answer",    "red pulse",           "Calling",                    5000,  sceneFallCaregiver, nullptr    },
  { "Fall: escalated",    "red pulse",           "Help coming",                5000,  sceneFallEms,       nullptr    },
};
#define SCENE_COUNT (sizeof(SCENES) / sizeof(SCENES[0]))

// ---------------------------------------------------------------- the loop

static uint8_t  scene = 0;
static uint32_t sceneStartMs = 0;

static void startScene(uint8_t i) {
  scene = i;
  sceneStartMs = millis();
  baseline();
  if (SCENES[i].enter) SCENES[i].enter();
  Serial.printf("[demo] %2u/%u  %-20s ring: %-21s oled: %s\n",
                i + 1, (unsigned)SCENE_COUNT, SCENES[i].title,
                SCENES[i].ring, SCENES[i].screen);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[demo] Brain Buddy showcase - ring and screen, no radio");

  ledsInit();
  displayInit();
  buzzerInit();
  eventsInit();
  gpsInit();
  scheduleResetToDefaults();     // the labels the prompt scene puts on screen
  messageInit();

  displayFlash("Brain Buddy", 3500);
  displayFooter("demo mode");
  uint32_t until = millis() + 3500;
  while ((int32_t)(millis() - until) < 0) {      // hold the boot chase
    ledsTick();
    displayTick();
    delay(5);
  }
  ledsBootDone();
  startScene(0);
}

void loop() {
  uint32_t elapsed = millis() - sceneStartMs;
  if (elapsed >= SCENES[scene].ms) {
    startScene((scene + 1) % SCENE_COUNT);
    elapsed = 0;
  } else if (SCENES[scene].tick) {
    SCENES[scene].tick(elapsed);
  }

  buzzerTick();
  ledsTick();
  displayTick();
  delay(1);
}

#endif
