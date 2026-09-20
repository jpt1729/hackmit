#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "events.h"
#include "activity.h"
#include "location.h"
#include "wear.h"
#include "gps.h"
#include "buzzer.h"
#include "leds.h"
#include "prompts.h"
#include "safety.h"
#include "server.h"
#include "display.h"

DeviceState g_state;

#ifndef PIO_UNIT_TESTING

static void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("WiFi: connecting to %s ", WIFI_SSID);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
    ledsTick();                     // keep the boot animation turning
    Serial.print('.');
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi: connected, http://%s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nWiFi: failed, prompts wait for GPS time or POST /time");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Granny Nanny firmware placeholder");
  Serial.println("Build the DeviceState contract and modules here.");
  delay(200);
  Serial.println("\n[routine-anchor] boot");

  ledsInit();
  displayInit();
  buzzerInit();
  eventsInit();

  connectWiFi();
  configTime(TZ_OFFSET_SEC, 0, NTP_SERVER);

  activityInit();
  wearInit();
  locationInit();
  gpsInit();                        // also sets the clock if NTP is blocked
  promptsInit();
  safetyInit();
  serverInit();

  static char footer[24];
  if (WiFi.status() == WL_CONNECTED) {
    snprintf(footer, sizeof(footer), "%s", WiFi.localIP().toString().c_str());
    displayFooter(footer);
  }
  ledsBootDone();
  Serial.println("[routine-anchor] ready");
}

void loop() {
  activityTick();     // IMU: activity, shake-ack, wander flag
  wearTick();         // on-body estimate, from the IMU
  gpsTick();          // NMEA in, fix + geofence out
  locationTick();     // WiFi RSSI room estimate
  promptsTick();      // the product: schedule, gating, ack window
  safetyTick();       // away-from-home and night-wander chimes
  buzzerTick();       // non-blocking note sequencer
  ledsTick();         // ring animation
  displayTick();      // OLED, redrawn only when the content changes
  serverTick();       // dashboard polling
  delay(1);
}

#endif
