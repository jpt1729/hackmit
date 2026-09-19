#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "events.h"
#include "activity.h"
#include "location.h"
#include "wear.h"
#include "haptics.h"
#include "prompts.h"
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
    Serial.print('.');
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi: connected, http://%s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nWiFi: failed, prompts wait for POST /time");
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[routine-anchor] boot");
  displayInit();

  connectWiFi();
  configTime(TZ_OFFSET_SEC, 0, NTP_SERVER);

  eventsInit();
  hapticsInit();
  activityInit();
  wearInit();
  locationInit();
  promptsInit();
  serverInit();

  Serial.println("[routine-anchor] ready");
}

void loop() {
  activityTick();
  wearTick();
  locationTick();
  promptsTick();
  hapticsTick();
  displayTick();
  serverTick();
  delay(1);
}

#endif
