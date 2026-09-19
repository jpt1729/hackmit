#include "haptics.h"
#include "config.h"

// On/off durations in ms, starting with on, 0-terminated.
static const uint16_t PAT_GENTLE[] = {400, 300, 400, 0};
static const uint16_t PAT_REMIND[] = {200, 150, 200, 150, 200, 0};

static const uint16_t* pat = nullptr;
static uint8_t  patIdx = 0;
static uint32_t stepEndMs = 0;

static void motor(bool on) { analogWrite(PIN_VIBE, on ? VIBE_DUTY : 0); }

static void start(const uint16_t* p) {
  pat = p;
  patIdx = 0;
  motor(true);
  stepEndMs = millis() + pat[0];
}

void hapticsInit() {
  pat = nullptr;
  pinMode(PIN_VIBE, OUTPUT);
  motor(false);
}

void hapticsGentle() { start(PAT_GENTLE); }
void hapticsRemind() { start(PAT_REMIND); }

void hapticsTick() {
  if (!pat || (int32_t)(millis() - stepEndMs) < 0) return;
  patIdx++;
  if (pat[patIdx] == 0) {
    motor(false);
    pat = nullptr;
    return;
  }
  motor(patIdx % 2 == 0);
  stepEndMs = millis() + pat[patIdx];
}
