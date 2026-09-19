#pragma once
#include <Arduino.h>
#include <time.h>

enum Activity : uint8_t { SLEEPING, RESTING, MOVING };
enum Room : uint8_t { ROOM_UNKNOWN, KITCHEN, BEDROOM, LIVING, ANY_ROOM = 255 };

inline const char* activityName(Activity a) {
  switch (a) {
    case SLEEPING: return "sleeping";
    case MOVING:   return "moving";
    default:       return "resting";
  }
}

inline const char* roomName(Room r) {
  switch (r) {
    case KITCHEN: return "kitchen";
    case BEDROOM: return "bedroom";
    case LIVING:  return "living";
    default:      return "unknown";
  }
}

struct DeviceState {
  Activity activity       = RESTING;
  Room     room           = ROOM_UNKNOWN;
  uint8_t  roomConfidence = 0;
  bool     worn           = true;
  bool     wanderFlag     = false;
  int8_t   pendingPrompt  = -1;
};

extern DeviceState g_state;

inline bool timeValid() { return time(nullptr) > 1700000000; }
