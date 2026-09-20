// On-device bring-up tests. Runs on the real ESP32 with the real wiring:
//   pio test -e esp32dev
// Uses the real firmware modules (test_build_src = yes), so a pass here means
// the same code that ships talks to the same hardware correctly.
//
// Bench setup: board lying still, MPU-6050 + OLED on I2C, piezo on PIN_BUZZER,
// NeoPixel ring on PIN_LEDS, GT-U7 wired to PIN_GPS_RX, WIFI_SSID reachable.
// You should HEAR two chimes and SEE the ring animate. Put the GPS somewhere
// with a view of the sky (a window is usually enough) or its tests skip.

#include <Arduino.h>
#include <unity.h>
#include <WiFi.h>
#include <Wire.h>
#include "config.h"
#include "state.h"
#include "events.h"
#include "activity.h"
#include "buzzer.h"
#include "leds.h"
#include "gps.h"
#include "location.h"
#include "wear.h"
#include "prompts.h"
#include "safety.h"
#include "display.h"

// Unity's TEST_PRINTF can't do widths/floats portably; format ourselves.
static char sayBuf[160];
#define SAY(...) do { snprintf(sayBuf, sizeof(sayBuf), __VA_ARGS__); TEST_MESSAGE(sayBuf); } while (0)

void setUp() {}
void tearDown() {}

static uint8_t mpuRead(uint8_t reg, uint8_t* out, uint8_t n) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0;
  uint8_t got = Wire.requestFrom((int)MPU_ADDR, (int)n);
  for (uint8_t i = 0; i < got; i++) out[i] = Wire.read();
  return got;
}

// ---------------------------------------------------------------- memory

void test_free_heap_at_boot() {
  SAY("free heap %u, largest block %u", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  TEST_ASSERT_GREATER_THAN(100000, ESP.getFreeHeap());
}

void test_full_event_log_fits_and_does_not_leak() {
  eventsInit();
  for (int i = 0; i < 250; i++) addEvent("prompt_missed", "quiet_time");
  uint32_t before = ESP.getFreeHeap();
  size_t len = 0;
  for (int i = 0; i < 20; i++) len = eventsJsonSince(0).length();   // 20 dashboard polls
  uint32_t after = ESP.getFreeHeap();
  SAY("full /events body %u bytes, heap delta %d", (unsigned)len, (int)(before - after));
  TEST_ASSERT_GREATER_THAN(1000, len);
  TEST_ASSERT_INT_WITHIN(512, before, after);
  eventsInit();
}

// ---------------------------------------------------------------- IMU

void test_mpu6050_whoami() {
  Wire.begin(PIN_SDA, PIN_SCL);
  uint8_t who = 0;
  TEST_ASSERT_EQUAL_MESSAGE(1, mpuRead(0x75, &who, 1),
      "No I2C reply at MPU_ADDR. Check SDA/SCL pins, 3V3/GND, and AD0 (0x68 low / 0x69 high)");
  SAY("WHO_AM_I = 0x%02X", who);
  // 0x68 genuine MPU-6050; 0x70/0x71/0x73/0x98 are common MPU-6500/9250 clones (same regs).
  bool known = who == 0x68 || who == 0x70 || who == 0x71 || who == 0x73 || who == 0x98;
  TEST_ASSERT_TRUE_MESSAGE(known, "Unexpected IMU. activity.cpp assumes MPU-6050 registers");
}

void test_accel_reads_1g_at_rest() {
  activityInit();                       // wakes the IMU
  delay(100);
  uint8_t b[6];
  TEST_ASSERT_EQUAL(6, mpuRead(0x3B, b, 6));
  float g[3];
  for (int i = 0; i < 3; i++) g[i] = (int16_t)((b[2 * i] << 8) | b[2 * i + 1]) / MPU_LSB_PER_G;
  float mag = sqrtf(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
  SAY("accel x=%.2f y=%.2f z=%.2f |a|=%.2f g", g[0], g[1], g[2], mag);
  TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.15f, 1.0f, mag,
      "|a| should be ~1g lying still. 0 = IMU still asleep; ~0.5/2 = MPU_ACCEL_FS_SEL "
      "and MPU_LSB_PER_G disagree");
}

void test_activity_resting_on_bench() {
  activityInit();
  uint32_t start = millis();
  while (millis() - start < 3000) activityTick();
  TEST_ASSERT_EQUAL_STRING("resting", activityName(g_state.activity));
}

// Wear is inferred from IMU noise, so the threshold has to sit above whatever
// this particular board reads while lying dead still on the bench.
void test_imu_noise_floor_is_below_the_wear_threshold() {
  activityInit();
  uint32_t start = millis();
  float peak = 0.0f;
  while (millis() - start < 5000) {
    activityTick();
    if (activityMotion() > peak) peak = activityMotion();
  }
  SAY("still-on-bench motion peaks at %.4f g (WEAR_MICRO_G = %.4f)", peak, WEAR_MICRO_G);
  // TEST_ASSERT_LESS_THAN is Unity's *integer* compare: it truncated both
  // WEAR_MICRO_G (0.012f) and peak to 0 and failed as "0 < 0" on any board.
  TEST_ASSERT_LESS_THAN_FLOAT_MESSAGE(WEAR_MICRO_G, peak,
      "This board's IMU is noisier than WEAR_MICRO_G: raise it or wear_off never fires");
}

void test_wear_detects_the_bench_as_not_worn() {
  activityInit();
  wearInit();
  TEST_ASSERT_TRUE(g_state.worn);                 // starts optimistic
  uint32_t start = millis();
  while (millis() - start < WEAR_OFF_STILL_MS + 3000) {
    activityTick();
    wearTick();
    delay(1);
  }
  SAY("after %u s of stillness worn=%d", (WEAR_OFF_STILL_MS + 3000) / 1000, g_state.worn);
  TEST_ASSERT_FALSE_MESSAGE(g_state.worn, "Still worn after a long still period: WEAR_MICRO_G too low?");
  TEST_MESSAGE("Now pick the board up and keep it moving for the next test.");
}

// ---------------------------------------------------------------- OLED

void test_oled_responds() {
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.beginTransmission(OLED_ADDR);
  TEST_ASSERT_EQUAL_MESSAGE(0, Wire.endTransmission(),
      "No I2C reply at OLED_ADDR. Check wiring; some modules use 0x3D");
}

void test_oled_shows_messages() {
  displayInit();
  displayFooter("192.168.0.0");
  delay(1000);
  displayFlash("TEST", 1500);
  uint32_t t0 = millis();
  while (millis() - t0 < 1500) displayTick();
  g_state.pendingPrompt = 0;
  t0 = millis();
  while (millis() - t0 < 2000) displayTick();
  g_state.pendingPrompt = -1;
  displayTick();
  TEST_MESSAGE("You should have seen: Starting, TEST, then the first prompt's label.");
  TEST_PASS();
}

// ---------------------------------------------------------------- piezo

void test_buzzer_plays_patterns() {
  buzzerInit();
  uint32_t t0 = millis();
  buzzerGentle();
  while (millis() - t0 < 1200) buzzerTick();
  buzzerRemind();
  t0 = millis();
  while (millis() - t0 < 1200) buzzerTick();
  buzzerAlert();
  t0 = millis();
  while (millis() - t0 < 1200) buzzerTick();
  TEST_MESSAGE("You should have heard: two rising notes, three notes, then a two-tone alert.");
  TEST_PASS();
}

// ---------------------------------------------------------------- LED ring

void test_led_ring_animates() {
  ledsInit();
  ledsBootDone();
  struct { LedMode mode; const char* what; } show[] = {
    {LED_PROMPT, "amber comet (prompt waiting)"},
    {LED_ALERT,  "red pulse (away from home)"},
    {LED_ACK,    "green wash (acknowledged)"},
    {LED_NIGHT,  "dim warm glow (asleep)"},
  };
  for (auto& s : show) {
    SAY("ring should show: %s", s.what);
    ledsFlash(s.mode, 2000);
    uint32_t t0 = millis();
    while (millis() - t0 < 2000) { ledsTick(); delay(1); }
  }
  ledsInit();
  TEST_MESSAGE("If the ring stayed dark: check DIN on PIN_LEDS, 5V, and the shared ground.");
  TEST_PASS();
}

void test_led_frame_is_cheap_enough_for_the_loop() {
  ledsInit();
  ledsBootDone();
  uint32_t worst = 0;
  for (int i = 0; i < 200; i++) {
    uint32_t s = micros();
    ledsTick();
    uint32_t d = micros() - s;
    if (d > worst) worst = d;
    delay(5);
  }
  SAY("worst ledsTick %u us", worst);
  // Bit-banged NeoPixel writes block with interrupts off: ~30 us per pixel.
  TEST_ASSERT_LESS_THAN(5000, worst);
}

// ---------------------------------------------------------------- GPS

void test_gps_module_is_talking() {
  gpsInit();
  uint32_t t0 = millis();
  int bytes = 0, dollars = 0;
  while (millis() - t0 < 5000) {
    while (Serial2.available()) {
      int c = Serial2.read();
      bytes++;
      if (c == '$') dollars++;
      gpsFeed((char)c);
    }
    delay(1);
  }
  SAY("%d bytes, %d NMEA sentences in 5 s", bytes, dollars);
  TEST_ASSERT_GREATER_THAN_MESSAGE(0, bytes,
      "Nothing on the GPS UART. GT-U7 TX -> PIN_GPS_RX (not TX), 9600 baud, 5V power");
  TEST_ASSERT_GREATER_THAN_MESSAGE(0, dollars, "Bytes but no '$': wrong baud rate?");
}

void test_gps_gets_a_fix() {
  // Cold start under a roof can take minutes; this is informational on purpose.
  gpsInit();
  uint32_t t0 = millis();
  while (!gpsFixValid() && millis() - t0 < 90000) {
    gpsTick();
    delay(10);
  }
  if (!gpsFixValid()) {
    SAY("no fix after %u s, %u sats seen", (millis() - t0) / 1000, g_state.sats);
    TEST_IGNORE_MESSAGE("No GPS fix indoors. Retry near a window before the demo");
  }
  SAY("fix in %u s: %.6f, %.6f  %u sats  %d m from HOME (%s)",
      (millis() - t0) / 1000, g_state.lat, g_state.lon, g_state.sats,
      (int)g_state.distanceHomeM, gpsHomeHeading());
  TEST_MESSAGE("Paste those coordinates into HOME_LAT / HOME_LON in config.h.");
  TEST_ASSERT_TRUE(g_state.lat != 0.0 && g_state.lon != 0.0);
}

void test_gps_sets_the_clock_without_ntp() {
  if (!gpsFixValid()) TEST_IGNORE_MESSAGE("no GPS fix");
  uint32_t t0 = millis();
  while (!timeValid() && millis() - t0 < 10000) {
    gpsTick();
    delay(10);
  }
  TEST_ASSERT_TRUE_MESSAGE(timeValid(), "Fix but no RMC time: the clock still needs NTP or POST /time");
  time_t t = time(nullptr);
  struct tm tmv;
  localtime_r(&t, &tmv);
  SAY("clock from GPS: %02d:%02d local (check TZ_OFFSET_SEC)", tmv.tm_hour, tmv.tm_min);
}

// ---------------------------------------------------------------- WiFi

void test_wifi_scan_sees_fingerprint_aps() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  int n = WiFi.scanNetworks();
  TEST_ASSERT_GREATER_THAN_MESSAGE(0, n, "WiFi scan found nothing: antenna or radio problem");
  for (int i = 0; i < n && i < 10; i++)
    SAY("  %s  %4d dBm  %s", WiFi.BSSIDstr(i).c_str(), WiFi.RSSI(i), WiFi.SSID(i).c_str());
#if ENABLE_LOCATION
  int matched = 0;
  for (size_t r = 0; r < NUM_ROOM_FPS; r++)
    for (uint8_t a = 0; a < ROOM_FPS[r].n; a++)
      for (int i = 0; i < n; i++)
        if (WiFi.BSSIDstr(i).equalsIgnoreCase(ROOM_FPS[r].aps[a].bssid)) { matched++; break; }
  WiFi.scanDelete();
  SAY("%d fingerprint APs visible", matched);
  TEST_ASSERT_GREATER_THAN_MESSAGE(0, matched,
      "None of the BSSIDs in ROOM_FPS are visible. Run tools/fingerprint_trainer.py at this venue");
#else
  WiFi.scanDelete();
#endif
}

void test_async_scan_does_not_block_loop() {
  uint32_t t0 = millis();
  WiFi.scanNetworks(true);
  uint32_t startCost = millis() - t0;
  int n;
  while ((n = WiFi.scanComplete()) == WIFI_SCAN_RUNNING && millis() - t0 < 10000) delay(10);
  SAY("async scan start took %u ms, finished after %u ms with %d APs",
              startCost, millis() - t0, n);
  TEST_ASSERT_LESS_THAN(50, startCost);
  TEST_ASSERT_GREATER_OR_EQUAL(0, n);
  WiFi.scanDelete();
}

void test_wifi_connects() {
  TEST_ASSERT_TRUE_MESSAGE(strcmp(WIFI_SSID, "your-hotspot-ssid") != 0,
                           "Set WIFI_SSID / WIFI_PASS in config.h");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) delay(100);
  TEST_ASSERT_EQUAL_MESSAGE(WL_CONNECTED, WiFi.status(), "Could not join WIFI_SSID");
  SAY("connected in %u ms, IP %s, RSSI %d", millis() - t0,
              WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

void test_ntp_sets_clock() {
  if (WiFi.status() != WL_CONNECTED) TEST_IGNORE_MESSAGE("no WiFi");
  configTime(TZ_OFFSET_SEC, 0, NTP_SERVER);
  uint32_t t0 = millis();
  while (!timeValid() && millis() - t0 < 15000) delay(100);
  TEST_ASSERT_TRUE_MESSAGE(timeValid(),
      "NTP blocked? Then the clock must come from GPS, or the dashboard must POST /time");
  time_t t = time(nullptr);
  struct tm tmv;
  localtime_r(&t, &tmv);
  SAY("local time %02d:%02d (check TZ_OFFSET_SEC if this is wrong)", tmv.tm_hour, tmv.tm_min);
}

void test_gps_uart_survives_wifi() {
  // The radio and the UART share nothing, but a wiring mistake on GPIO 16/17
  // (or a brownout from powering the GPS off 3V3) shows up once WiFi is on.
  if (WiFi.status() != WL_CONNECTED) TEST_IGNORE_MESSAGE("no WiFi");
  uint32_t t0 = millis();
  int bytes = 0;
  while (millis() - t0 < 3000) {
    while (Serial2.available()) { Serial2.read(); bytes++; }
    delay(1);
  }
  SAY("%d GPS bytes in 3 s with WiFi up", bytes);
  TEST_ASSERT_GREATER_THAN_MESSAGE(0, bytes, "GPS went quiet once WiFi came up: power or wiring");
}

// ---------------------------------------------------------------- whole loop

void test_main_loop_never_blocks() {
  // Everything except the HTTP server, for 30 s, including 2 location scans.
  eventsInit();
  buzzerInit();
  ledsInit();
  activityInit();
  wearInit();
  gpsInit();
  locationInit();
  promptsInit();
  safetyInit();
  displayInit();
  ledsBootDone();
  uint32_t worst = 0, iterations = 0, t0 = millis();
  while (millis() - t0 < 30000) {
    uint32_t s = micros();
    activityTick();
    wearTick();
    gpsTick();
    locationTick();
    promptsTick();
    safetyTick();
    buzzerTick();
    ledsTick();
    displayTick();
    uint32_t d = micros() - s;
    if (d > worst) worst = d;
    iterations++;
    delay(1);
  }
  SAY("%u loop iterations, worst tick %u us, free heap %u",
              iterations, worst, ESP.getFreeHeap());
  // Buzzer notes are 90 ms at the shortest; anything near that makes the
  // chimes audibly uneven.
  TEST_ASSERT_LESS_THAN(50000, worst);
  TEST_ASSERT_GREATER_THAN(60000, ESP.getFreeHeap());
}

void setup() {
  delay(2000);   // let the serial monitor attach
  UNITY_BEGIN();
  RUN_TEST(test_free_heap_at_boot);
  RUN_TEST(test_full_event_log_fits_and_does_not_leak);
  RUN_TEST(test_mpu6050_whoami);
  RUN_TEST(test_accel_reads_1g_at_rest);
  RUN_TEST(test_activity_resting_on_bench);
  RUN_TEST(test_imu_noise_floor_is_below_the_wear_threshold);
  RUN_TEST(test_oled_responds);
  RUN_TEST(test_oled_shows_messages);
  RUN_TEST(test_buzzer_plays_patterns);
  RUN_TEST(test_led_ring_animates);
  RUN_TEST(test_led_frame_is_cheap_enough_for_the_loop);
  RUN_TEST(test_gps_module_is_talking);
  RUN_TEST(test_gps_gets_a_fix);
  RUN_TEST(test_gps_sets_the_clock_without_ntp);
  RUN_TEST(test_wifi_scan_sees_fingerprint_aps);
  RUN_TEST(test_async_scan_does_not_block_loop);
  RUN_TEST(test_wifi_connects);
  RUN_TEST(test_ntp_sets_clock);
  RUN_TEST(test_gps_uart_survives_wifi);
  RUN_TEST(test_wear_detects_the_bench_as_not_worn);   // slow: 5 min of stillness
  RUN_TEST(test_main_loop_never_blocks);
  UNITY_END();
}

void loop() {}
