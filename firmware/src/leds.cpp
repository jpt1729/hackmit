#include "leds.h"
#include "config.h"
#include <math.h>

// Mode selection is plain logic and always compiles (the native tests exercise
// it); only the pixel pushing lives behind ENABLE_LEDS.
static LedMode  mode = LED_BOOT;
static LedMode  override_ = LED_BOOT;
static uint32_t overrideUntilMs = 0;
static bool     booting = true;
static uint32_t lastFrameMs = 0;

const char* ledsModeName(LedMode m) {
  switch (m) {
    case LED_BOOT:    return "boot";
    case LED_ALERT:   return "alert";
    case LED_PROMPT:  return "prompt";
    case LED_ACK:     return "ack";
    case LED_OFFBODY: return "offbody";
    case LED_NIGHT:   return "night";
    case LED_PROGRESS: return "progress";
    default:          return "idle";
  }
}

LedMode ledsMode() { return mode; }

// Round to nearest, but never round a part-done day down to an empty ring or
// an unfinished one up to a full one - those two both have to mean something.
uint8_t ledsProgressPixels(uint8_t done, uint8_t total) {
  if (!total) return 0;
  if (done >= total) return LED_COUNT;
  uint8_t px = (uint8_t)(((uint16_t)done * LED_COUNT + total / 2) / total);
  if (done > 0 && px == 0) px = 1;
  if (px >= LED_COUNT) px = LED_COUNT - 1;
  return px;
}

void ledsFlash(LedMode m, uint32_t ms) {
  override_ = m;
  overrideUntilMs = millis() + ms;
}

void ledsBootDone() { booting = false; }

static LedMode chooseMode() {
  if (overrideUntilMs && (int32_t)(millis() - overrideUntilMs) < 0) return override_;
  if (booting)                                      return LED_BOOT;
  if (g_state.fallStage != FALL_NONE)               return LED_ALERT;
  if (g_state.awayFromHome || g_state.wanderFlag)   return LED_ALERT;
  if (g_state.pendingPrompt >= 0)                   return LED_PROMPT;
  if (!g_state.worn)                                return LED_OFFBODY;
  if (g_state.activity == SLEEPING)                 return LED_NIGHT;
  if (g_state.tasksTotal > 0)                       return LED_PROGRESS;
  return LED_IDLE;
}

#if ENABLE_LEDS

#include <Adafruit_NeoPixel.h>

static Adafruit_NeoPixel ring(LED_COUNT, PIN_LEDS, NEO_GRB + NEO_KHZ800);

// 0..1 triangle wave of period `periodMs`, for breathing and pulsing.
static float wave(uint32_t periodMs) {
  float phase = (float)(millis() % periodMs) / (float)periodMs;
  return phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
}

static void fill(uint8_t r, uint8_t g, uint8_t b, float scale) {
  ring.fill(ring.Color((uint8_t)(r * scale), (uint8_t)(g * scale), (uint8_t)(b * scale)));
}

// One bright pixel with a fading tail, rotating once per periodMs.
static void comet(uint8_t r, uint8_t g, uint8_t b, uint32_t periodMs, uint8_t tail) {
  ring.clear();
  int head = (int)((millis() % periodMs) * LED_COUNT / periodMs);
  for (uint8_t i = 0; i <= tail; i++) {
    float f = 1.0f - (float)i / (float)(tail + 1);
    int   px = (head - i + LED_COUNT * 2) % LED_COUNT;
    ring.setPixelColor(px, ring.Color((uint8_t)(r * f), (uint8_t)(g * f), (uint8_t)(b * f)));
  }
}

static void render(LedMode m) {
  switch (m) {
    case LED_BOOT:
      comet(120, 120, 140, 1200, 3);
      break;
    case LED_ALERT:
      fill(255, 30, 0, 0.35f + 0.65f * wave(700));
      break;
    case LED_PROMPT:
      comet(255, 150, 0, 900, 4);
      break;
    case LED_ACK:
      fill(0, 255, 60, 1.0f);
      break;
    case LED_OFFBODY:
      ring.clear();
      ring.setPixelColor(0, ring.Color(0, 0, 60));
      break;
    case LED_NIGHT:
      fill(60, 25, 0, 0.25f);
      break;
    case LED_PROGRESS: {
      // One pixel per task. A finished day breathes green instead of sitting
      // flat, so "all done" is unmistakable from across the room.
      uint8_t lit = ledsProgressPixels(g_state.tasksDone, g_state.tasksTotal);
      if (lit >= LED_COUNT) {
        fill(0, 255, 60, 0.35f + 0.45f * wave(3000));
        break;
      }
      ring.clear();
      for (uint8_t i = 0; i < LED_COUNT; i++) {
        if (i < lit) ring.setPixelColor(i, ring.Color(0, 200, 50));
        else         ring.setPixelColor(i, ring.Color(LED_PROGRESS_MIN_V, LED_PROGRESS_MIN_V / 2, 0));
      }
      break;
    }
    default:
      fill(0, 120, 110, 0.15f + 0.35f * wave(4000));
      break;
  }
  ring.show();
}

void ledsInit() {
  mode = LED_BOOT;
  override_ = LED_BOOT;
  overrideUntilMs = 0;
  booting = true;
  lastFrameMs = 0;
  ring.begin();
  ring.setBrightness(LED_BRIGHTNESS);
  ring.clear();
  ring.show();
}

void ledsTick() {
  uint32_t now = millis();
  if (now - lastFrameMs < LED_FRAME_MS) return;
  lastFrameMs = now;
  mode = chooseMode();
  render(mode);
}

#else

void ledsInit() {
  mode = LED_BOOT;
  override_ = LED_BOOT;
  overrideUntilMs = 0;
  booting = true;
  lastFrameMs = 0;
}

void ledsTick() {
  uint32_t now = millis();
  if (now - lastFrameMs < LED_FRAME_MS) return;
  lastFrameMs = now;
  mode = chooseMode();
}

#endif
