#pragma once
#include <Arduino.h>
#include <time.h>

enum Activity : uint8_t { SLEEPING, RESTING, MOVING };

// How far along the fall escalation ladder we are. Every step up is one the
// wearer failed to cancel; a shake at any stage drops straight back to NONE.
enum FallStage : uint8_t {
  FALL_NONE,        // nothing detected
  FALL_CONFIRMING,  // impact seen, asking "are you OK?" on the wrist
  FALL_CAREGIVER,   // no answer: the caregiver has been told
  FALL_EMS,         // still no answer: escalated to emergency services
};
enum Room : uint8_t { ROOM_UNKNOWN, KITCHEN, BEDROOM, LIVING, ANY_ROOM = 255 };

inline const char* activityName(Activity a) {
  switch (a) {
    case SLEEPING: return "sleeping";
    case MOVING:   return "moving";
    default:       return "resting";
  }
}

inline const char* fallStageName(FallStage s) {
  switch (s) {
    case FALL_CONFIRMING: return "confirming";
    case FALL_CAREGIVER:  return "caregiver";
    case FALL_EMS:        return "ems";
    default:              return "none";
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

  // GPS. `fix` false means every field below it is stale, not zero.
  bool     fix            = false;
  uint8_t  sats           = 0;
  double   lat            = 0.0;
  double   lon            = 0.0;
  float    distanceHomeM  = -1.0f;   // < 0 = unknown
  bool     awayFromHome   = false;   // outside the geofence, debounced

  // Routine progress for the day, mirrored by the LED ring.
  uint8_t  tasksDone      = 0;
  uint8_t  tasksTotal     = 0;

  // Fall detection and the escalation ladder.
  FallStage fallStage     = FALL_NONE;

  // A caregiver message is on the OLED, waiting to be dismissed.
  bool     messageWaiting = false;
};

extern DeviceState g_state;

inline bool timeValid() { return time(nullptr) > 1700000000; }
