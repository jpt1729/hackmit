#include "wear.h"
#include "config.h"
#include "events.h"
#include "activity.h"

#if ENABLE_WEAR

// There is no electrode in this build. Wear is inferred from the IMU instead:
// a device strapped to a person is never perfectly still (breathing, pulse,
// small drifts), a device on a nightstand is. This is an estimate, not contact
// sensing - the dashboard says "not detected on body", never "removed".
static uint32_t lastSampleMs = 0;
static uint32_t lastMotionMs = 0;
static uint32_t motionStartMs = 0;

void wearInit() {
  lastSampleMs = 0;
  lastMotionMs = millis();
  motionStartMs = 0;
  g_state.worn = true;            // assume it is on until stillness says otherwise
}

void wearTick() {
  uint32_t now = millis();
  if (now - lastSampleMs < WEAR_SAMPLE_MS) return;
  lastSampleMs = now;

  // No IMU means no opinion: fail open so prompts still fire.
  if (!activityImuOk()) {
    lastMotionMs = now;
    g_state.worn = true;
    return;
  }

  if (activityMotion() > WEAR_MICRO_G) {
    if (!motionStartMs) motionStartMs = now;
    lastMotionMs = now;
  } else {
    motionStartMs = 0;
  }

  if (!g_state.worn && motionStartMs && now - motionStartMs >= WEAR_ON_DEBOUNCE_MS) {
    g_state.worn = true;
    addEvent("wear_on", "");
  } else if (g_state.worn && now - lastMotionMs >= WEAR_OFF_STILL_MS) {
    g_state.worn = false;
    addEvent("wear_off", "");
  }
}

#else

void wearInit() {}
void wearTick() { g_state.worn = true; }

#endif
