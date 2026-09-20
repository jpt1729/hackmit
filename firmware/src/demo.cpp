// Showcase build: the band with nothing plugged into it but power.
//
// No WiFi, no IMU, no satellites - just a timeline that pushes g_state through
// situations the firmware knows about and lets the real ledsTick() and
// displayTick() draw them. Nothing here re-implements an animation or a
// screen, so what the ring and the OLED do in this demo is what they do in the
// field. Where the firmware already exposes an entry point - promptsDemoFire(),
// promptsAckPending() - the demo calls it rather than setting state by hand.
//
//   pio run -e esp32dev_demo -t upload && pio device monitor
//
// Two loops:
//   TABLE (default)  ~50 s, five beats, for a judging table someone walks up to
//   TOUR             every scene, for a longer slot
//
// Driving it by hand, so you can hold a beat while you talk over it:
//   BOOT button (GPIO 0)   tap = next scene, hold = pause, hold 3 s = swap loop
//   serial                 any key = next, 'p' = pause, 't' = swap loop

#ifdef DEMO_MODE

#include <Arduino.h>
#include <sys/time.h>

#include "config.h"
#include "events.h"
#include "gps.h"
#include "buzzer.h"
#include "leds.h"
#include "schedule.h"
#include "prompts.h"
#include "message.h"
#include "display.h"

DeviceState g_state;     // main.cpp is out of this build, so the demo owns it

#ifndef DEMO_BUZZER
#define DEMO_BUZZER 1    // -DDEMO_BUZZER=0 for a silent demo in a quiet room
#endif
#ifndef DEMO_FULL_TOUR
#define DEMO_FULL_TOUR 0 // -DDEMO_FULL_TOUR=1 to boot into the long loop
#endif

#define BTN_PIN         0        // the DevKit's BOOT button, readable after boot
#define BTN_DEBOUNCE_MS 40
#define BTN_HOLD_MS     600      // held this long: pause on the current scene
#define BTN_SWAP_MS     3000     // held this long: swap between table and tour

// 2026-09-20 00:00 UTC. The clock runs in UTC with no offset, so an epoch set
// here reads straight back out of localtime_r() as the hour we asked for.
#define DEMO_DAY_EPOCH 1789862400L

// A point 140 m north-west of home: far enough outside GEOFENCE_RADIUS_M that
// the real geofence in gps.cpp flips, rather than the demo setting the flag.
#define DEMO_AWAY_LAT (HOME_LAT + 0.00089)
#define DEMO_AWAY_LON (HOME_LON - 0.00120)

// Schedule slots, from DEFAULT_SCHEDULE in config.h.
#define SLOT_MEDS  0             // "Meds are in the kitchen"
#define SLOT_LUNCH 1             // "Time for lunch"

// ---------------------------------------------------------------- fake inputs

static void setClock(int hour, int minute) {
  struct timeval tv = { DEMO_DAY_EPOCH + hour * 3600L + minute * 60L, 0 };
  settimeofday(&tv, nullptr);
}

// The buzzer is reached through the real prompt path, which chimes on its own,
// so a silent demo has to stop it rather than decline to start it.
static void muteCheck() {
#if !DEMO_BUZZER
  buzzerStop();
#endif
}

static void chime(void (*tone)()) {
#if DEMO_BUZZER
  tone();
#else
  (void)tone;
#endif
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

// ---------------------------------------------------------------- the scenes

struct Scene {
  const char* title;
  const char* ring;                  // caption for the monitor
  const char* screen;
  const char* footer;                // caption on the wrist, idle screens only
  void      (*enter)();
  void      (*tick)(uint32_t elapsedMs);
};

// Every scene starts from a worn, quiet, nothing-due band and adds one thing.
// The table loop never moves the clock off morning: a dashboard watching this
// band should not see time jump backwards every few seconds.
static void baseline() {
  g_state.activity       = RESTING;
  g_state.room           = ROOM_UNKNOWN;
  g_state.roomConfidence = 0;
  g_state.worn           = true;
  g_state.wanderFlag     = false;
  g_state.tasksDone      = 0;
  g_state.tasksTotal     = 0;
  g_state.fallStage      = FALL_NONE;
  g_state.pendingPrompt  = -1;
  messageInit();
  buzzerStop();
  setClock(9, 0);
}

// -------- the table loop

static void sceneReminder() {
  g_state.room = KITCHEN;
  g_state.tasksTotal = 3;
  promptsDemoFire(SLOT_MEDS);        // real fire path: chime, event, pendingPrompt
  muteCheck();
}

static void sceneAck() {
  g_state.room = KITCHEN;
  g_state.tasksTotal = 3;
  promptsDemoFire(SLOT_MEDS);
  promptsAckPending();               // real ack path: green wash, "Done!", event
  g_state.tasksDone = 1;
  muteCheck();
}

static void sceneMissed() {
  setClock(12, 0);
  g_state.room = LIVING;
  g_state.tasksTotal = 3;
  g_state.tasksDone  = 1;
  promptsDemoFire(SLOT_LUNCH);
  muteCheck();
}

// The re-chime, then the miss. promptsTick() resolves this after ACK_WINDOW_MS
// (60 s), which is longer than anyone stands at a table, so the demo runs the
// same two steps on its own clock: the reminder gives up, and the caregiver is
// told the honest thing rather than nothing.
static void missedTick(uint32_t elapsed) {
  static bool rebuzzed = false, missed = false;
  if (elapsed < 3500) { rebuzzed = missed = false; return; }
  if (!rebuzzed) {
    rebuzzed = true;
    chime(buzzerRemind);
  }
  if (!missed && elapsed >= 7000) {
    missed = true;
    addEvent("prompt_missed", scheduleAt(SLOT_LUNCH).id);
    g_state.pendingPrompt = -1;
    buzzerStop();
    displayFlash("Not acknowledged", 4000);
  }
}

static void sceneAway() {
  setClock(15, 20);
  g_state.tasksTotal = 3;
  g_state.tasksDone  = 1;
  feedFixes(DEMO_AWAY_LAT, DEMO_AWAY_LON);
  chime(buzzerAlert);
}

// Keep the fix fresh: gpsFixValid() ages out at GPS_STALE_MS and the heading on
// the screen goes with it.
static void awayTick(uint32_t elapsed) {
  static uint32_t last = 0;
  if (elapsed < last) last = 0;
  if (elapsed - last < 2000) return;
  last = elapsed;
  feedFix(DEMO_AWAY_LAT, DEMO_AWAY_LON, 7);
}

static void sceneHome() {
  setClock(15, 34);
  g_state.room = LIVING;
  g_state.tasksTotal = 3;
  g_state.tasksDone  = 2;
  feedFixes(HOME_LAT, HOME_LON);
}

// -------- the rest of the tour

static void sceneIdle() {
  g_state.room = KITCHEN;
  g_state.roomConfidence = 82;
}

static void sceneProgress() {
  g_state.room = KITCHEN;
  g_state.tasksTotal = 3;
  g_state.tasksDone  = 1;
}

static void sceneMessage() {
  g_state.room = LIVING;
  g_state.tasksTotal = 3;
  g_state.tasksDone  = 2;
  messageSet("Ellie is coming over at four. Love, Dad");
  muteCheck();
}

static void sceneAllDone() {
  setClock(19, 40);
  g_state.room = LIVING;
  g_state.tasksTotal = 3;
  g_state.tasksDone  = 3;
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

// The catalogue. Both loops index into this, so a scene is described once.
enum SceneId : uint8_t {
  S_REMINDER, S_ACK, S_MISSED, S_AWAY, S_HOME,
  S_IDLE, S_PROGRESS, S_MESSAGE, S_ALLDONE,
  S_WANDER, S_OFFBODY, S_NIGHT,
  S_FALL1, S_FALL2, S_FALL3,
};

static const Scene SCENES[] = {
  [S_REMINDER] = { "9am reminder",      "amber comet",          "Meds are in the kitchen", "reminder",       sceneReminder,      nullptr    },
  [S_ACK]      = { "Shaken to confirm", "green wash",           "Done!",                   "acknowledged",   sceneAck,           nullptr    },
  [S_MISSED]   = { "No answer",         "amber, then progress", "Not acknowledged",        "not acked",      sceneMissed,        missedTick },
  [S_AWAY]     = { "Outside the fence", "red pulse",            "home 140m SE",            "away from home", sceneAway,          awayTick   },
  [S_HOME]     = { "Back home",         "2 of 3 done",          "clock, mascot, living",   "home again",     sceneHome,          nullptr    },
  [S_IDLE]     = { "Morning, at home",  "teal breathe",         "clock, mascot, kitchen",  "at home",        sceneIdle,          nullptr    },
  [S_PROGRESS] = { "One task done",     "4 of 12 px green",     "clock, mascot",           "1 of 3 done",    sceneProgress,      nullptr    },
  [S_MESSAGE]  = { "Caregiver message", "green flash, then 8",  "Ellie is coming...",      "message",        sceneMessage,       nullptr    },
  [S_ALLDONE]  = { "Routine finished",  "full ring, breathing", "clock, mascot",           "all done",       sceneAllDone,       nullptr    },
  [S_WANDER]   = { "3am wandering",     "red pulse",            "03:12, bedroom",          "night wander",   sceneWander,        nullptr    },
  [S_OFFBODY]  = { "Taken off",         "one dim blue dot",     "not on wrist",            "off wrist",      sceneOffBody,       nullptr    },
  [S_NIGHT]    = { "Asleep",            "dim warm glow",        "02:30, bedroom",          "night light",    sceneNight,         nullptr    },
  [S_FALL1]    = { "Fall: are you OK?", "red pulse",            "Are you OK?",             "",               sceneFallConfirm,   nullptr    },
  [S_FALL2]    = { "Fall: no answer",   "red pulse",            "Calling",                 "",               sceneFallCaregiver, nullptr    },
  [S_FALL3]    = { "Fall: escalated",   "red pulse",            "Help coming",             "",               sceneFallEms,       nullptr    },
};

struct Beat { SceneId scene; uint32_t ms; };

// The table loop, ~50 s. Five beats that carry "it asks the person to do
// something, it doesn't report on them": a reminder arriving, the person
// answering it, the reminder going unanswered and being reported honestly,
// the band pointing the way home before it tells anyone, and the day resuming.
static const Beat TABLE[] = {
  { S_REMINDER,  9000 },
  { S_ACK,       6000 },
  { S_MISSED,   12000 },
  { S_AWAY,     13000 },
  { S_HOME,      6000 },
};
#define TABLE_LEN (sizeof(TABLE) / sizeof(TABLE[0]))

static const Beat TOUR[] = {
  { S_IDLE,      6000 }, { S_PROGRESS,  5000 }, { S_REMINDER,  9000 },
  { S_ACK,       5000 }, { S_MISSED,   12000 }, { S_MESSAGE,   8000 },
  { S_ALLDONE,   6000 }, { S_AWAY,     13000 }, { S_HOME,      5000 },
  { S_WANDER,    6000 }, { S_OFFBODY,   5000 }, { S_NIGHT,     6000 },
  { S_FALL1,     6000 }, { S_FALL2,     5000 }, { S_FALL3,     5000 },
};
#define TOUR_LEN (sizeof(TOUR) / sizeof(TOUR[0]))

// ---------------------------------------------------------------- the loop

static bool     tourMode = DEMO_FULL_TOUR;
static bool     paused   = false;
static uint8_t  beat     = 0;
static uint32_t beatStartMs = 0;

static const Beat* order()   { return tourMode ? TOUR : TABLE; }
static uint8_t     orderLen() { return tourMode ? TOUR_LEN : TABLE_LEN; }

static void startBeat(uint8_t i) {
  beat = i;
  beatStartMs = millis();
  const Beat&  b = order()[i];
  const Scene& s = SCENES[b.scene];

  baseline();
  if (s.enter) s.enter();
  displayFooter(s.footer);

  Serial.printf("[demo] %s %2u/%u  %-19s ring: %-21s oled: %s\n",
                tourMode ? "tour " : "table", i + 1, orderLen(),
                s.title, s.ring, s.screen);
}

static void swapMode() {
  tourMode = !tourMode;
  Serial.printf("\n[demo] === %s loop ===\n", tourMode ? "TOUR" : "TABLE");
  displayFlash(tourMode ? "Full tour" : "Table loop", 1500);
  startBeat(0);
}

// BOOT button: tap advances, hold freezes the scene, a long hold swaps loops.
//
// GPIO 0 is also the auto-reset circuit's IO0 line, so an attached USB serial
// port can hold it LOW for as long as it is open - which reads exactly like a
// button held down, and would park the demo in a permanent pause. So the
// button arms only once it has been seen released: a line pinned low by the
// adapter never arms, and the demo runs normally with a monitor attached.
// Driving it by hand then goes through the serial keys instead.
static void buttonTick() {
  static bool     armed = false;
  static bool     down = false;
  static uint32_t downAtMs = 0, lastEdgeMs = 0;
  static bool     swapArmed = false;

  bool pressed = digitalRead(BTN_PIN) == LOW;
  uint32_t now = millis();
  if (!pressed) armed = true;
  if (!armed) return;
  if (now - lastEdgeMs < BTN_DEBOUNCE_MS) return;

  if (pressed && !down) {
    down = true;
    downAtMs = now;
    swapArmed = false;
    lastEdgeMs = now;
    return;
  }

  if (pressed && down) {
    uint32_t held = now - downAtMs;
    if (held >= BTN_HOLD_MS) paused = true;         // frozen while held
    if (held >= BTN_SWAP_MS && !swapArmed) {
      swapArmed = true;
      displayFlash("Release to swap", 1200);
    }
    return;
  }

  if (!pressed && down) {
    uint32_t held = now - downAtMs;
    down = false;
    lastEdgeMs = now;
    paused = false;
    if (held >= BTN_SWAP_MS)      swapMode();
    else if (held < BTN_HOLD_MS)  startBeat((beat + 1) % orderLen());
    else Serial.println("[demo] resumed");
  }
}

static void serialTick() {
  while (Serial.available()) {
    int c = Serial.read();
    if (c == 'p' || c == 'P') {
      paused = !paused;
      Serial.printf("[demo] %s\n", paused ? "paused" : "resumed");
    } else if (c == 't' || c == 'T') {
      swapMode();
    } else if (c != '\r' && c != '\n') {
      startBeat((beat + 1) % orderLen());
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[demo] Brain Buddy showcase - ring and screen, no radio");
  Serial.println("[demo] BOOT: tap = next, hold = pause, hold 3 s = swap loop");
  Serial.println("[demo] serial: any key = next, 'p' = pause, 't' = swap loop\n");

  pinMode(BTN_PIN, INPUT_PULLUP);

  ledsInit();
  displayInit();
  buzzerInit();
  eventsInit();
  gpsInit();
  scheduleResetToDefaults();     // the labels the reminder beats put on screen
  promptsInit();
  messageInit();

  displaySplash(3000);               // mascot + name, the same screen main.cpp boots to
  uint32_t until = millis() + 3000;
  while ((int32_t)(millis() - until) < 0) {      // hold the boot chase
    ledsTick();
    displayTick();
    delay(5);
  }
  ledsBootDone();
  startBeat(0);
}

void loop() {
  static uint32_t lastLoopMs = 0;
  uint32_t now = millis();

  buttonTick();
  serialTick();

  if (paused) {
    beatStartMs += now - lastLoopMs;    // freeze elapsed where it stands
  } else {
    uint32_t elapsed = now - beatStartMs;
    if (elapsed >= order()[beat].ms) {
      startBeat((beat + 1) % orderLen());
    } else {
      const Scene& s = SCENES[order()[beat].scene];
      if (s.tick) s.tick(elapsed);
    }
  }
  lastLoopMs = now;

  buzzerTick();
  ledsTick();
  displayTick();
  delay(1);
}

#endif
