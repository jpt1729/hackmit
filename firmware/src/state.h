#pragma once
#include <Arduino.h>
#include <time.h>
#include <string.h>

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

// Inverse of roomName(): turns a contract room name from POST /schedule back
// into a Room. "any" means the reminder is not tied to a room at all.
inline bool roomFromName(const char* name, Room& out) {
  if (!strcmp(name, "any"))     { out = ANY_ROOM;     return true; }
  if (!strcmp(name, "unknown")) { out = ROOM_UNKNOWN; return true; }
  if (!strcmp(name, "kitchen")) { out = KITCHEN;      return true; }
  if (!strcmp(name, "bedroom")) { out = BEDROOM;      return true; }
  if (!strcmp(name, "living"))  { out = LIVING;       return true; }
  return false;
}

struct DeviceState {
  Activity activity       = RESTING;
  Room     room           = ROOM_UNKNOWN;
  uint8_t  roomConfidence = 0;
  bool     worn           = true;
  bool     wanderFlag     = false;
  int8_t   pendingPrompt  = -1;

  // GPS. `fix` false means every field below it is stale, not zero.
  bool     fix            = false;
  uint8_t  sats           = 0;
  double   lat            = 0.0;
  double   lon            = 0.0;
  float    distanceHomeM  = -1.0f;   // < 0 = unknown
  bool     awayFromHome   = false;   // outside the geofence, debounced
};

extern DeviceState g_state;

inline bool timeValid() { return time(nullptr) > 1700000000; }
