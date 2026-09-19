// On-device bring-up tests. Runs on the real ESP32 with the real wiring:
//   pio test -e esp32dev
// Uses the real firmware modules (test_build_src = yes), so a pass here means
// the same code that ships talks to the same hardware correctly.
//
// Bench setup: board lying still, MPU-6050 + vibe motor wired per config.h,
// WIFI_SSID reachable. You should FEEL two buzzes during test_vibe_motor.

#include <Arduino.h>
#include <unity.h>
#include <WiFi.h>
#include <Wire.h>
#include "config.h"
#include "state.h"
#include "events.h"
#include "activity.h"
#include "haptics.h"
#include "location.h"
#include "wear.h"
#include "prompts.h"

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
  for (int i = 0; i < 250; i++) addEvent("prompt_missed", "wind_down");
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
  for (int i = 0; i < 3; i++) g[i] = (int16_t)((b[2 * i] << 8) | b[2 * i + 1]) / 16384.0f;
  float mag = sqrtf(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
  SAY("accel x=%.2f y=%.2f z=%.2f |a|=%.2f g", g[0], g[1], g[2], mag);
  TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.15f, 1.0f, mag,
      "|a| should be ~1g lying still. 0 = IMU still asleep; ~0.5/2 = wrong range/scale");
}

void test_activity_resting_on_bench() {
  activityInit();
  uint32_t start = millis();
  while (millis() - start < 3000) activityTick();
  TEST_ASSERT_EQUAL_STRING("resting", activityName(g_state.activity));
}

// ---------------------------------------------------------------- electrode

void test_electrode_adc_reads() {
  wearInit();
  int lo = 4095, hi = 0;
  for (int i = 0; i < 50; i++) {
    int v = analogRead(PIN_ELECTRODE);
    lo = min(lo, v); hi = max(hi, v);
    delay(5);
  }
  SAY("electrode ADC min=%d max=%d (band %d..%d = worn). Touch the pad and rerun to calibrate.",
              lo, hi, WEAR_ADC_MIN, WEAR_ADC_MAX);
  TEST_ASSERT_TRUE(hi <= 4095 && lo >= 0);
  // A floating pin swings wildly; a wired divider is steady.
  TEST_ASSERT_LESS_THAN_MESSAGE(800, hi - lo, "ADC is noisy: electrode divider may be disconnected");
}

// ---------------------------------------------------------------- motor

void test_vibe_motor() {
  hapticsInit();
  uint32_t t0 = millis();
  hapticsGentle();
  while (millis() - t0 < 1500) hapticsTick();
  hapticsRemind();
  t0 = millis();
  while (millis() - t0 < 1200) hapticsTick();
  TEST_MESSAGE("You should have felt a double pulse, then a triple pulse.");
  TEST_PASS();
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
      "NTP blocked? The dashboard must POST /time, or prompts never fire");
  time_t t = time(nullptr);
  struct tm tmv;
  localtime_r(&t, &tmv);
  SAY("local time %02d:%02d (check TZ_OFFSET_SEC if this is wrong)", tmv.tm_hour, tmv.tm_min);
}

void test_adc_still_works_with_wifi_on() {
  // ADC2 dies under WiFi; this catches someone moving PIN_ELECTRODE to an ADC2 pin.
  if (WiFi.status() != WL_CONNECTED) TEST_IGNORE_MESSAGE("no WiFi");
  int v = analogRead(PIN_ELECTRODE);
  SAY("electrode ADC with WiFi on: %d", v);
  TEST_ASSERT_TRUE(v >= 0 && v <= 4095);
}

// ---------------------------------------------------------------- whole loop

void test_main_loop_never_blocks() {
  // Everything except the HTTP server, for 30 s, including 2 location scans.
  eventsInit();
  hapticsInit();
  activityInit();
  wearInit();
  locationInit();
  promptsInit();
  uint32_t worst = 0, iterations = 0, t0 = millis();
  while (millis() - t0 < 30000) {
    uint32_t s = micros();
    activityTick();
    wearTick();
    locationTick();
    promptsTick();
    hapticsTick();
    uint32_t d = micros() - s;
    if (d > worst) worst = d;
    iterations++;
    delay(1);
  }
  SAY("%u loop iterations, worst tick %u us, free heap %u",
              iterations, worst, ESP.getFreeHeap());
  // Haptic steps are 150 ms; anything near that makes buzzes audibly uneven.
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
  RUN_TEST(test_electrode_adc_reads);
  RUN_TEST(test_vibe_motor);
  RUN_TEST(test_wifi_scan_sees_fingerprint_aps);
  RUN_TEST(test_async_scan_does_not_block_loop);
  RUN_TEST(test_wifi_connects);
  RUN_TEST(test_ntp_sets_clock);
  RUN_TEST(test_adc_still_works_with_wifi_on);
  RUN_TEST(test_main_loop_never_blocks);
  UNITY_END();
}

void loop() {}
