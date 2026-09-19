#include "location.h"
#include "config.h"
#include "events.h"
#include <WiFi.h>

#if ENABLE_LOCATION

static uint32_t lastScanStartMs = 0;
static bool     scanning = false;
static Room     lastGuess = ROOM_UNKNOWN;
static uint8_t  agreeCount = 0;

// Mean squared RSSI error; reference APs not seen count as -100 dBm.
static float roomDistance(const RoomFP& fp, int nFound) {
  float d = 0.0f;
  for (uint8_t i = 0; i < fp.n; i++) {
    int rssi = -100;
    for (int j = 0; j < nFound; j++) {
      if (WiFi.BSSIDstr(j).equalsIgnoreCase(fp.aps[i].bssid)) {
        rssi = WiFi.RSSI(j);
        break;
      }
    }
    float diff = rssi - fp.aps[i].rssi;
    d += diff * diff;
  }
  return fp.n ? d / fp.n : 1e9f;
}

static void processScan(int nFound) {
  float best = 1e9f, second = 1e9f;
  Room  bestRoom = ROOM_UNKNOWN;
  for (size_t r = 0; r < NUM_ROOM_FPS; r++) {
    float d = roomDistance(ROOM_FPS[r], nFound);
    if (d < best) {
      second = best;
      best = d;
      bestRoom = ROOM_FPS[r].room;
    } else if (d < second) {
      second = d;
    }
  }

  uint8_t conf = second > 0.0f ? constrain((1.0f - best / second) * 100.0f, 0.0f, 100.0f) : 0;

  if (bestRoom == lastGuess) {
    agreeCount++;
  } else {
    lastGuess = bestRoom;
    agreeCount = 1;
  }

  if (bestRoom == g_state.room) {
    g_state.roomConfidence = conf;
  } else if (agreeCount >= LOC_DEBOUNCE_SCANS && conf >= LOC_MIN_CONFIDENCE) {
    g_state.room = bestRoom;
    g_state.roomConfidence = conf;
    addEvent("room_change", roomName(bestRoom));
  }
}

void locationInit() {
  lastScanStartMs = 0;
  scanning = false;
  lastGuess = ROOM_UNKNOWN;
  agreeCount = 0;
}

void locationTick() {
  uint32_t now = millis();
  if (!scanning && now - lastScanStartMs > LOC_SCAN_INTERVAL_MS) {
    lastScanStartMs = now;
    WiFi.scanNetworks(true);
    scanning = true;
  }
  if (!scanning) return;

  int n = WiFi.scanComplete();
  if (n >= 0) {
    processScan(n);
    WiFi.scanDelete();
    scanning = false;
  } else if (n == WIFI_SCAN_FAILED) {
    scanning = false;
  }
}

#else

void locationInit() {}
void locationTick() {}

#endif
