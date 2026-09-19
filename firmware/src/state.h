#pragma once

#include <stdint.h>
#include <time.h>

enum Activity { SLEEPING, RESTING, MOVING };
enum Room { UNKNOWN, KITCHEN, BEDROOM, LIVING };

struct DeviceState {
  Activity activity;
  Room room;
  bool worn;
  bool wanderFlag;
  int8_t rssiConfidence;
  uint32_t pendingPromptId;
};

struct Event {
  uint32_t id;
  time_t ts;
  const char* type;
  const char* detail;
};
