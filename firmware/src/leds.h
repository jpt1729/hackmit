#pragma once
#include <stdint.h>

// What the ring is saying, in priority order (highest first inside ledsTick).
enum LedMode : uint8_t {
  LED_BOOT,     // white chase while WiFi/GPS come up
  LED_ALERT,    // red pulse: away from home, or night-time wandering
  LED_PROMPT,   // amber comet: a prompt is waiting to be acknowledged
  LED_ACK,      // green wash, fades out
  LED_OFFBODY,  // dim blue dot: device is not being worn
  LED_NIGHT,    // very dim warm glow: asleep, doubles as a night light
  LED_IDLE,     // slow teal breathe: worn, awake, nothing due
};

void    ledsInit();
void    ledsTick();
void    ledsFlash(LedMode mode, uint32_t ms);   // temporary override
void    ledsBootDone();
LedMode ledsMode();
const char* ledsModeName(LedMode mode);
