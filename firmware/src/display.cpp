#include "display.h"
#include "config.h"
#include "schedule.h"
#include "gps.h"

#if ENABLE_DISPLAY

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 oled(128, 64, &Wire, -1);
static bool        ok = false;
static const char* flashText = nullptr;
static uint32_t    flashUntilMs = 0;
static uint32_t    lastCheckMs = 0;
static char        footer[24] = "";
static char        shownKey[96] = "";

static void printWrapped(const char* text, size_t cols) {
  char buf[64];
  strlcpy(buf, text, sizeof(buf));
  size_t used = 0;
  for (char* word = strtok(buf, " "); word; word = strtok(nullptr, " ")) {
    size_t len = strlen(word);
    if (used && used + 1 + len > cols) {
      oled.println();
      used = 0;
    } else if (used) {
      oled.print(' ');
      used++;
    }
    oled.print(word);
    used += len;
  }
}

static void drawCentered(const char* text, uint8_t size, int16_t y) {
  oled.setTextSize(size);
  oled.setCursor((128 - (int16_t)strlen(text) * 6 * size) / 2, y);
  oled.print(text);
}

// The one-line summary under the clock: where they are, or why we are unsure.
static void statusLine(char* out, size_t n) {
  if (!g_state.worn) {
    strlcpy(out, "not on wrist", n);
  } else if (g_state.awayFromHome && g_state.distanceHomeM >= 0) {
    snprintf(out, n, "home %dm %s", (int)g_state.distanceHomeM, gpsHomeHeading());
  } else if (g_state.room != ROOM_UNKNOWN) {
    strlcpy(out, roomName(g_state.room), n);
  } else if (ENABLE_GPS && !gpsFixValid()) {
    snprintf(out, n, "gps %u sats", g_state.sats);
  } else {
    strlcpy(out, "at home", n);
  }
}

void displayInit() {
  flashText = nullptr;
  lastCheckMs = 0;
  shownKey[0] = 0;
  footer[0] = 0;

  Wire.begin(PIN_SDA, PIN_SCL);
  ok = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR, true, false);
  if (!ok) {
    Serial.println("[display] SSD1306 not found");
    return;
  }
  oled.setTextColor(SSD1306_WHITE);
  oled.clearDisplay();
  drawCentered("Starting", 2, 24);
  oled.display();
}

void displayFlash(const char* text, uint32_t ms) {
  flashText = text;
  flashUntilMs = millis() + ms;
}

void displayFooter(const char* text) { strlcpy(footer, text ? text : "", sizeof(footer)); }

void displayTick() {
  uint32_t now = millis();
  if (!ok || now - lastCheckMs < 250) return;
  lastCheckMs = now;

  int  p = g_state.pendingPrompt;
  bool flashing = flashText && (int32_t)(now - flashUntilMs) < 0;
  char clock[6] = "--:--";
  char weekday[12] = "";
  if (timeValid()) {
    time_t t = time(nullptr);
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(clock, sizeof(clock), "%H:%M", &tmv);
    strftime(weekday, sizeof(weekday), "%A", &tmv);
  }
  char status[24];
  statusLine(status, sizeof(status));

  // Full redraws cost ~25 ms of I2C, so only redraw when the content changes.
  char key[96];
  snprintf(key, sizeof(key), "%d|%s|%s|%s|%s", p, flashing ? flashText : "", clock, status, footer);
  if (strcmp(key, shownKey) == 0) return;
  strlcpy(shownKey, key, sizeof(shownKey));

  oled.clearDisplay();
  if (p >= 0) {
    oled.setTextSize(2);
    oled.setCursor(0, 0);
    printWrapped(scheduleAt(p).label, 10);
    drawCentered("Shake to confirm", 1, 56);
  } else if (flashing) {
    drawCentered(flashText, strlen(flashText) > 8 ? 2 : 3, 24);
  } else {
    drawCentered(clock, 3, 4);
    drawCentered(weekday, 1, 32);
    drawCentered(status, 1, 44);
    if (footer[0]) drawCentered(footer, 1, 56);
  }
  oled.display();
}

#else

void displayInit() {}
void displayTick() {}
void displayFlash(const char*, uint32_t) {}
void displayFooter(const char*) {}

#endif
