#include "buzzer.h"
#include "config.h"

// Patterns are note lists: {frequency Hz, duration ms}. freq 0 = a rest,
// ms 0 = end of pattern. Frequencies sit in the 900-2600 Hz band where a
// piezo disc is efficient and where age-related hearing loss is mildest.
struct Note { uint16_t freq; uint16_t ms; };

static const Note PAT_GENTLE[] = {{988, 200}, {0, 120}, {1319, 320}, {0, 0}};
static const Note PAT_REMIND[] = {{1319, 160}, {0, 120}, {1319, 160}, {0, 120}, {1319, 320}, {0, 0}};
static const Note PAT_ALERT[]  = {{1047, 220}, {1568, 220}, {1047, 220}, {1568, 220}, {0, 0}};
static const Note PAT_ACK[]    = {{1568, 90}, {0, 60}, {2093, 140}, {0, 0}};

static const Note* pat = nullptr;
static uint8_t     noteIdx = 0;
static uint16_t    duty = 0;
static uint32_t    stepEndMs = 0;

// ---------------------------------------------------------------- hardware
// The ESP32 Arduino core renamed the LEDC API in 3.x: channels became pins.
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
#define LEDC_TARGET PIN_BUZZER
static void ledcOpen() { ledcAttach(PIN_BUZZER, 2000, BUZZER_RES_BITS); }
#else
#define LEDC_TARGET BUZZER_LEDC_CHANNEL
static void ledcOpen() {
  ledcSetup(BUZZER_LEDC_CHANNEL, 2000, BUZZER_RES_BITS);
  ledcAttachPin(PIN_BUZZER, BUZZER_LEDC_CHANNEL);
}
#endif

// `level` is a raw LEDC duty (0 .. 2^BUZZER_RES_BITS - 1). Duty is volume on a
// piezo: 50% is the loudest a square wave gets, so these stay well under it.
static void sound(uint16_t freq, uint16_t level) {
#if BUZZER_PASSIVE
  if (freq == 0 || level == 0) {
    ledcWrite(LEDC_TARGET, 0);
    return;
  }
  ledcWriteTone(LEDC_TARGET, freq);
  ledcWrite(LEDC_TARGET, level);
#else
  // Active buzzer: it only knows on and off, so the pattern becomes rhythm.
  (void)freq;
  digitalWrite(PIN_BUZZER, level && freq ? HIGH : LOW);
#endif
}

// ---------------------------------------------------------------- patterns
static void start(const Note* p, uint16_t level) {
  pat = p;
  noteIdx = 0;
  duty = level;
  sound(pat[0].freq, duty);
  stepEndMs = millis() + pat[0].ms;
}

void buzzerInit() {
  pat = nullptr;
#if BUZZER_PASSIVE
  ledcOpen();
#else
  pinMode(PIN_BUZZER, OUTPUT);
#endif
  sound(0, 0);
}

void buzzerGentle() { start(PAT_GENTLE, BUZZER_DUTY_GENTLE); }
void buzzerRemind() { start(PAT_REMIND, BUZZER_DUTY_REMIND); }
void buzzerAlert()  { start(PAT_ALERT,  BUZZER_DUTY_ALERT); }
void buzzerAck()    { start(PAT_ACK,    BUZZER_DUTY_GENTLE); }

void buzzerStop() {
  pat = nullptr;
  sound(0, 0);
}

bool buzzerBusy() { return pat != nullptr; }

void buzzerTick() {
  if (!pat || (int32_t)(millis() - stepEndMs) < 0) return;
  noteIdx++;
  if (pat[noteIdx].ms == 0) {
    buzzerStop();
    return;
  }
  sound(pat[noteIdx].freq, duty);
  stepEndMs = millis() + pat[noteIdx].ms;
}
