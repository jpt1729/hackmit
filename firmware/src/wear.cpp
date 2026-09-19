#include "wear.h"
#include "config.h"
#include "events.h"

#if ENABLE_WEAR

static bool     candidate = true;
static uint32_t candidateSinceMs = 0;
static uint32_t lastReadMs = 0;

void wearInit() {
  candidate = true;
  candidateSinceMs = millis();
  lastReadMs = 0;
  pinMode(PIN_ELECTRODE, INPUT);
}

void wearTick() {
  uint32_t now = millis();
  if (now - lastReadMs < 200) return;
  lastReadMs = now;

  int v = analogRead(PIN_ELECTRODE);
  bool contact = v > WEAR_ADC_MIN && v < WEAR_ADC_MAX;
  if (contact != candidate) {
    candidate = contact;
    candidateSinceMs = now;
  }
  if (candidate != g_state.worn && now - candidateSinceMs >= WEAR_DEBOUNCE_MS) {
    g_state.worn = candidate;
    addEvent(candidate ? "wear_on" : "wear_off", "");
  }
}

#else

void wearInit() {}
void wearTick() { g_state.worn = true; }

#endif
