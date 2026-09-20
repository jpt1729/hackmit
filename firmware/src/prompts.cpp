#include "prompts.h"
#include "config.h"
#include "events.h"
#include "activity.h"
#include "buzzer.h"
#include "leds.h"
#include "display.h"
#include <string.h>

static bool     doneToday[SCHEDULE_LEN];
static bool     ackedToday[SCHEDULE_LEN];
static int      lastDay = -1;
static uint32_t firedAtMs = 0;
static bool     rebuzzed = false;
static uint32_t lastCheckMs = 0;

// The ring counts what the wearer actually did, so a missed prompt closes the
// slot without lighting a pixel.
static void recount() {
  uint8_t done = 0;
  for (size_t i = 0; i < SCHEDULE_LEN; i++)
    if (ackedToday[i]) done++;
  g_state.tasksDone = done;
  g_state.tasksTotal = SCHEDULE_LEN;
}

static bool gatesPass(const PromptDef& p) {
  if (!g_state.worn || g_state.activity == SLEEPING) return false;
  if (g_state.awayFromHome) return false;   // out of the house: routine can wait
  return p.room == ANY_ROOM || g_state.room == ROOM_UNKNOWN || g_state.room == p.room;
}

static void fire(int idx) {
  activityAckConsume();
  g_state.pendingPrompt = idx;
  firedAtMs = millis();
  rebuzzed = false;
  buzzerGentle();
  addEvent("prompt_fired", SCHEDULE[idx].id);
}

static void resolve(int idx, const char* type) {
  addEvent(type, SCHEDULE[idx].id);
  doneToday[idx] = true;
  if (strcmp(type, "prompt_acked") == 0) ackedToday[idx] = true;
  g_state.pendingPrompt = -1;
  recount();
}

void promptsInit() {
  memset(doneToday, 0, sizeof(doneToday));
  memset(ackedToday, 0, sizeof(ackedToday));
  lastDay = -1;
  rebuzzed = false;
  lastCheckMs = 0;
  g_state.pendingPrompt = -1;
  recount();
}

int promptsFindById(const char* id) {
  for (size_t i = 0; i < SCHEDULE_LEN; i++)
    if (strcmp(SCHEDULE[i].id, id) == 0) return i;
  return -1;
}

void promptsDemoFire(int idx) {
  if (idx < 0 || idx >= (int)SCHEDULE_LEN) return;
  doneToday[idx] = true;
  fire(idx);
}

bool promptsAckPending() {
  int p = g_state.pendingPrompt;
  if (p < 0) return false;
  resolve(p, "prompt_acked");
  buzzerAck();
  ledsFlash(LED_ACK, LED_ACK_FLASH_MS);
  displayFlash("Done!", 5000);
  return true;
}

void promptsTick() {
  uint32_t now = millis();

  int p = g_state.pendingPrompt;
  if (p >= 0) {
    uint32_t elapsed = now - firedAtMs;
    if (activityAckConsume()) {
      promptsAckPending();
    } else if (elapsed >= ACK_WINDOW_MS) {
      resolve(p, "prompt_missed");
      buzzerStop();
    } else if (!rebuzzed && elapsed >= REBUZZ_AT_MS) {
      rebuzzed = true;
      buzzerRemind();
    }
  }

  if (now - lastCheckMs < 1000 || !timeValid()) return;
  lastCheckMs = now;

  time_t t = time(nullptr);
  struct tm tmv;
  localtime_r(&t, &tmv);
  if (tmv.tm_yday != lastDay) {
    lastDay = tmv.tm_yday;
    memset(doneToday, 0, sizeof(doneToday));
    memset(ackedToday, 0, sizeof(ackedToday));
    recount();
  }
  if (g_state.pendingPrompt >= 0) return;

  int nowMin = tmv.tm_hour * 60 + tmv.tm_min;
  for (size_t i = 0; i < SCHEDULE_LEN; i++) {
    int lateMin = nowMin - (SCHEDULE[i].hour * 60 + SCHEDULE[i].minute);
    if (doneToday[i] || lateMin < 0) continue;
    if (lateMin > PROMPT_HOLD_MIN) {
      resolve(i, "prompt_missed");
    } else if (gatesPass(SCHEDULE[i])) {
      fire(i);
      break;
    }
  }
}
