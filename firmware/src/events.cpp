#include "events.h"
#include "schedule.h"
#include <time.h>
#include <string.h>

#define EVENT_BUF 200

struct Event {
  uint32_t id;
  time_t   ts;
  char     type[16];
  // Wide enough for a prompt id: the dashboard mints `custom_<uuid>` ids for
  // activities a caregiver adds, and a truncated one never matches back.
  char     detail[PROMPT_ID_LEN];
};

static Event    buf[EVENT_BUF];
static uint16_t count = 0;
static uint16_t head = 0;
static uint32_t nextId = 1;

void eventsInit() {
  count = 0;
  head = 0;
  nextId = 1;
}

uint32_t addEvent(const char* type, const char* detail) {
  Event& e = buf[head];
  e.id = nextId++;
  e.ts = time(nullptr);
  strlcpy(e.type, type, sizeof(e.type));
  strlcpy(e.detail, detail ? detail : "", sizeof(e.detail));

  head = (head + 1) % EVENT_BUF;
  if (count < EVENT_BUF) count++;

  Serial.printf("[event] #%lu %s %s\n", (unsigned long)e.id, e.type, e.detail);
  return e.id;
}

String eventsJsonSince(uint32_t sinceId) {
  String out;
  out.reserve(2048);
  out = "{\"events\":[";
  bool first = true;
  uint16_t oldest = (head + EVENT_BUF - count) % EVENT_BUF;
  for (uint16_t i = 0; i < count; i++) {
    const Event& e = buf[(oldest + i) % EVENT_BUF];
    if (e.id <= sinceId) continue;
    char item[224];
    snprintf(item, sizeof(item), "%s{\"id\":%lu,\"ts\":%lu,\"type\":\"%s\",\"detail\":\"%s\"}",
             first ? "" : ",", (unsigned long)e.id, (unsigned long)e.ts, e.type, e.detail);
    out += item;
    first = false;
  }
  out += "]}";
  return out;
}
