#include "safety.h"
#include "config.h"
#include "buzzer.h"
#include "leds.h"
#include "display.h"
#include "gps.h"
#include <stdio.h>

static bool     lastAway = false;
static bool     lastWander = false;
static uint32_t lastChimeMs = 0;
static uint32_t lastWanderChimeMs = 0;
static uint8_t  chimes = 0;
static bool     silenced = false;
static char     homeLine[24] = "";

static void chimeAway() {
  chimes++;
  lastChimeMs = millis();
  buzzerAlert();
  ledsFlash(LED_ALERT, 15000);
  const char* dir = gpsHomeHeading();
  if (g_state.distanceHomeM >= 0)
    snprintf(homeLine, sizeof(homeLine), "Home %dm %s", (int)g_state.distanceHomeM, dir);
  else
    snprintf(homeLine, sizeof(homeLine), "Lets head home");
  displayFlash(homeLine, 20000);
}

void safetyInit() {
  lastAway = false;
  lastWander = false;
  lastChimeMs = 0;
  lastWanderChimeMs = 0;
  chimes = 0;
  silenced = false;
  homeLine[0] = 0;
}

void safetySilence() {
  silenced = true;
  buzzerStop();
}

uint8_t safetyChimeCount() { return chimes; }

void safetyTick() {
  uint32_t now = millis();

  if (g_state.awayFromHome != lastAway) {
    lastAway = g_state.awayFromHome;
    chimes = 0;
    silenced = false;
    if (g_state.awayFromHome) {
      chimeAway();
    } else {
      buzzerAck();
      displayFlash("Back home", 8000);
    }
  } else if (g_state.awayFromHome && !silenced && chimes < SAFETY_MAX_CHIMES &&
             now - lastChimeMs >= SAFETY_REPEAT_MS) {
    chimeAway();
  }

  // Night wandering: one quiet nudge, then leave them alone. Waking someone
  // fully at 3am is worse than the wander itself - the dashboard has the alert.
  bool wander = g_state.wanderFlag && !g_state.awayFromHome;
  if (wander && (!lastWander || now - lastWanderChimeMs >= SAFETY_WANDER_MS)) {
    lastWanderChimeMs = now;
    buzzerGentle();
    ledsFlash(LED_ALERT, 10000);
    displayFlash("Bed is this way", 15000);
  }
  lastWander = wander;
}
