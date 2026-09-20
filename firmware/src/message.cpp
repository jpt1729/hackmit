#include "message.h"
#include "config.h"
#include "activity.h"
#include "events.h"
#include "buzzer.h"
#include "leds.h"
#include <string.h>

static char     text[MSG_MAX_LEN + 1] = "";
static uint32_t arrivedAtMs = 0;

void messageInit() {
  text[0] = 0;
  arrivedAtMs = 0;
  g_state.messageWaiting = false;
}

bool messagePending() { return g_state.messageWaiting; }

const char* messageText() { return text; }

void messageSet(const char* incoming) {
  if (!incoming || !*incoming) return;
  strncpy(text, incoming, MSG_MAX_LEN);
  text[MSG_MAX_LEN] = 0;
  arrivedAtMs = millis();
  g_state.messageWaiting = true;
  addEvent("message_received", "");
  buzzerGentle();
  ledsFlash(LED_ACK, 2000);
}

void messageDismiss() {
  if (!g_state.messageWaiting) return;
  g_state.messageWaiting = false;
  addEvent("message_read", "");
  buzzerAck();
}

void messageTick() {
  if (!g_state.messageWaiting) return;

  // A message must never sit on top of a fall or a live prompt, and it must
  // not hold the screen forever if nobody is there to dismiss it.
  if (millis() - arrivedAtMs >= MSG_TTL_MS) {
    g_state.messageWaiting = false;
    addEvent("message_expired", "");
    return;
  }
  if (g_state.fallStage != FALL_NONE || g_state.pendingPrompt >= 0) return;
  if (activityAckConsume()) messageDismiss();
}
