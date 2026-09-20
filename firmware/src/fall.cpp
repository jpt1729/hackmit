#include "fall.h"
#include "config.h"
#include "activity.h"
#include "events.h"
#include "buzzer.h"
#include "leds.h"
#include "display.h"

static uint32_t stageSinceMs = 0;
static uint32_t lastChimeMs = 0;

// displayTick() renders the fall screen straight from g_state.fallStage, so
// there is nothing to flash here - only the noise.
static void enter(FallStage stage, const char* event) {
  g_state.fallStage = stage;
  stageSinceMs = millis();
  lastChimeMs = millis();
  addEvent(event, "");
  buzzerAlert();
}

void fallInit() {
  g_state.fallStage = FALL_NONE;
  stageSinceMs = 0;
  lastChimeMs = 0;
}

bool fallActive() { return g_state.fallStage != FALL_NONE; }

void fallCancel() {
  if (g_state.fallStage == FALL_NONE) return;
  addEvent("fall_cancelled", fallStageName(g_state.fallStage));
  g_state.fallStage = FALL_NONE;
  stageSinceMs = 0;
  buzzerStop();
  buzzerAck();
  ledsFlash(LED_ACK, LED_ACK_FLASH_MS);
  displayFlash("Glad you are OK", 6000);
}

void fallTick() {
  uint32_t now = millis();

  if (g_state.fallStage == FALL_NONE) {
    // Only look for new falls while the watch is actually on someone. A watch
    // knocked off a nightstand produces a textbook fall signature.
    if (activityFallConsume() && g_state.worn)
      enter(FALL_CONFIRMING, "fall_detected");
    return;
  }

  // A shake means "I am fine" and outranks everything below. promptsTick()
  // runs after this in the loop, so consuming it here is deliberate: while a
  // fall is open the shake belongs to the fall, not to a pending prompt.
  if (activityAckConsume()) {
    fallCancel();
    return;
  }

  uint32_t inStage = now - stageSinceMs;
  switch (g_state.fallStage) {
    case FALL_CONFIRMING:
      if (inStage >= FALL_CANCEL_MS)
        enter(FALL_CAREGIVER, "fall_alert");
      break;
    case FALL_CAREGIVER:
      if (inStage >= FALL_EMS_MS)
        enter(FALL_EMS, "fall_ems");
      break;
    default:
      break;   // FALL_EMS holds until someone clears it
  }

  // Keep chiming while it is unresolved: the wearer may have come round.
  if (now - lastChimeMs >= FALL_CHIME_MS) {
    lastChimeMs = now;
    buzzerAlert();
  }
}
