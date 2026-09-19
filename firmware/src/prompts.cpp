#include "prompts.h"
#include "config.h"
#include "events.h"
#include "activity.h"
#include "haptics.h"
#include "display.h"
#include <string.h>

static bool     doneToday[SCHEDULE_LEN];
static int      lastDay = -1;
static uint32_t firedAtMs = 0;
static bool     rebuzzed = false;
static uint32_t lastCheckMs = 0;

static bool gatesPass(const PromptDef& p) {
  if (!g_state.worn || g_state.activity == SLEEPING) return false;
  return p.room == ANY_ROOM || g_state.room == ROOM_UNKNOWN || g_state.room == p.room;
}

static void fire(int idx) {
  activityAckConsume();
  g_state.pendingPrompt = idx;
  firedAtMs = millis();
  rebuzzed = false;
  hapticsGentle();
  addEvent("prompt_fired", SCHEDULE[idx].id);
}

static void resolve(int idx, const char* type) {
  addEvent(type, SCHEDULE[idx].id);
  doneToday[idx] = true;
  g_state.pendingPrompt = -1;
}

void promptsInit() {
  memset(doneToday, 0, sizeof(doneToday));
  lastDay = -1;
  rebuzzed = false;
  lastCheckMs = 0;
  g_state.pendingPrompt = -1;
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

void promptsTick() {
  uint32_t now = millis();

  int p = g_state.pendingPrompt;
  if (p >= 0) {
    uint32_t elapsed = now - firedAtMs;
    if (activityAckConsume()) {
      resolve(p, "prompt_acked");
      displayFlash("Done!", 5000);
    } else if (elapsed >= ACK_WINDOW_MS) {
      resolve(p, "prompt_missed");
    } else if (!rebuzzed && elapsed >= REBUZZ_AT_MS) {
      rebuzzed = true;
      hapticsRemind();
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
