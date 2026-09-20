#include "prompts.h"
#include "config.h"
#include "schedule.h"
#include "events.h"
#include "activity.h"
#include "buzzer.h"
#include "leds.h"
#include "display.h"
#include <string.h>

static bool     doneToday[SCHEDULE_MAX];
static int      lastDay = -1;
static uint32_t firedAtMs = 0;
static bool     rebuzzed = false;
static uint32_t lastCheckMs = 0;

static bool gatesPass(const Prompt& p) {
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
  addEvent("prompt_fired", scheduleAt(idx).id);
}

static void resolve(int idx, const char* type) {
  addEvent(type, scheduleAt(idx).id);
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

int promptsFindById(const char* id) { return scheduleFindById(id); }

// A pushed routine renumbers everything, so anything holding an index into the
// old table - the pending prompt, today's done flags - has to let go.
void promptsScheduleChanged() {
  memset(doneToday, 0, sizeof(doneToday));
  g_state.pendingPrompt = -1;
  rebuzzed = false;
  lastDay = -1;
}

void promptsDemoFire(int idx) {
  if (idx < 0 || idx >= (int)scheduleCount()) return;
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
  }
  if (g_state.pendingPrompt >= 0) return;

  int nowMin = tmv.tm_hour * 60 + tmv.tm_min;
  for (uint8_t i = 0; i < scheduleCount(); i++) {
    const Prompt& prompt = scheduleAt(i);
    int lateMin = nowMin - (prompt.hour * 60 + prompt.minute);
    if (doneToday[i] || lateMin < 0) continue;
    if (lateMin > PROMPT_HOLD_MIN) {
      resolve(i, "prompt_missed");
    } else if (gatesPass(prompt)) {
      fire(i);
      break;
    }
  }
}
