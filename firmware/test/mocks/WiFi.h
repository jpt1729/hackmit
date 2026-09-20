#pragma once
// Fake async WiFi scanner. Tests fill mock::scanResults, then flip
// mock::scanState to the number of results (done) or WIFI_SCAN_FAILED.
#include "Arduino.h"

#define WIFI_SCAN_RUNNING (-1)
#define WIFI_SCAN_FAILED  (-2)

namespace mock {
struct ScanAP { std::string bssid; int rssi; };
inline std::vector<ScanAP> scanResults;
inline int  scanState = WIFI_SCAN_RUNNING;  // what scanComplete() returns
inline int  scansStarted = 0;
inline bool autoCompleteScans = true;       // finish scans instantly with scanResults
}  // namespace mock

class WiFiClass {
 public:
  int16_t scanNetworks(bool /*async*/) {
    mock::scansStarted++;
    mock::scanState = mock::autoCompleteScans ? (int)mock::scanResults.size() : WIFI_SCAN_RUNNING;
    return WIFI_SCAN_RUNNING;
  }
  int16_t scanComplete() { return (int16_t)mock::scanState; }
  void    scanDelete() { mock::scanState = WIFI_SCAN_RUNNING; }
  String  BSSIDstr(int i) { return String(mock::scanResults[i].bssid); }
  int32_t RSSI(int i) { return mock::scanResults[i].rssi; }
};
inline WiFiClass WiFi;
