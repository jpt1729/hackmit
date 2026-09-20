#include "schedule.h"
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <Preferences.h>
#endif

// A pushed routine is kept as the raw body we accepted. Re-parsing it at boot
// costs a millisecond and means there is exactly one piece of code that decides
// what a valid schedule is.
#define SCHEDULE_NVS_NS   "grannynanny"
#define SCHEDULE_NVS_KEY  "schedule"
#define SCHEDULE_JSON_MAX 3072

static Prompt  table[SCHEDULE_MAX];
static uint8_t tableLen = 0;
static bool    custom   = false;

// ---------------------------------------------------------------- tiny JSON reader
// Only the shape POST /schedule uses, hand-rolled for the same reason gps.cpp
// parses its own NMEA: one less library between the caregiver and the wrist.

static void skipWs(const char*& p) {
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
}

static bool expect(const char*& p, char c) {
  skipWs(p);
  if (*p != c) return false;
  p++;
  return true;
}

// Reads a JSON string literal into `out`. \u escapes are refused rather than
// half-decoded: a mangled label on a reminder is worse than a rejected push.
static bool readString(const char*& p, char* out, size_t outLen) {
  skipWs(p);
  if (*p != '"') return false;
  p++;
  size_t n = 0;
  while (*p && *p != '"') {
    char c = *p++;
    if (c == '\\') {
      switch (char e = *p++) {
        case '"': case '\\': case '/': c = e;    break;
        case 'n':                      c = '\n'; break;
        case 't':                      c = '\t'; break;
        case 'r':                      c = '\r'; break;
        case 'b':                      c = '\b'; break;
        case 'f':                      c = '\f'; break;
        default: return false;
      }
    }
    if (n + 1 >= outLen) return false;
    out[n++] = c;
  }
  if (*p != '"') return false;
  p++;
  out[n] = 0;
  return true;
}

static bool readInt(const char*& p, long& out) {
  skipWs(p);
  char* end = nullptr;
  out = strtol(p, &end, 10);
  if (end == p) return false;
  p = end;
  return true;
}

// Steps over any one JSON value without interpreting it. Lets the top level
// carry fields this firmware does not know about - including the "source" and
// "max" that GET /schedule reports - so the band accepts its own output back.
static bool skipValue(const char*& p, int depth = 0) {
  if (depth > 8) return false;
  skipWs(p);
  if (*p == '"') {
    char sink[PROMPT_LABEL_LEN];
    const char* probe = p;
    if (readString(probe, sink, sizeof sink)) { p = probe; return true; }
    // Too long to keep, but still a well-formed string we can step over.
    p++;
    while (*p && *p != '"') p += (*p == '\\' && p[1]) ? 2 : 1;
    if (*p != '"') return false;
    p++;
    return true;
  }
  if (*p == '{' || *p == '[') {
    char close = *p == '{' ? '}' : ']';
    p++;
    skipWs(p);
    if (*p == close) { p++; return true; }
    for (;;) {
      if (close == '}') {
        if (!skipValue(p, depth + 1) || !expect(p, ':')) return false;
      }
      if (!skipValue(p, depth + 1)) return false;
      skipWs(p);
      if (*p == ',') { p++; continue; }
      if (*p == close) { p++; return true; }
      return false;
    }
  }
  if (!strncmp(p, "true", 4))  { p += 4; return true; }
  if (!strncmp(p, "false", 5)) { p += 5; return true; }
  if (!strncmp(p, "null", 4))  { p += 4; return true; }
  long ignored;
  const char* before = p;
  if (readInt(p, ignored)) {
    while (*p == '.' || isdigit((unsigned char)*p) || *p == 'e' || *p == 'E' ||
           *p == '+' || *p == '-') p++;
    return p != before;
  }
  return false;
}

static bool validId(const char* id) {
  if (!*id) return false;
  for (const char* c = id; *c; c++)
    if (!isalnum((unsigned char)*c) && *c != '_' && *c != '-') return false;
  return true;
}

// ---------------------------------------------------------------- parsing

static bool fail(char* err, size_t errLen, const char* msg) {
  if (err && errLen) strlcpy(err, msg, errLen);
  return false;
}

static bool parseItem(const char*& p, Prompt& out, char* err, size_t errLen) {
  if (!expect(p, '{')) return fail(err, errLen, "each item must be an object");

  bool haveId = false, haveLabel = false, haveHour = false, haveMinute = false;
  out.room = ANY_ROOM;
  out.hour = out.minute = 0;
  out.id[0] = out.label[0] = 0;

  skipWs(p);
  if (*p == '}') return fail(err, errLen, "an item has no id, label or time");

  for (;;) {
    char key[12];
    if (!readString(p, key, sizeof key)) return fail(err, errLen, "expected a field name");
    if (!expect(p, ':')) return fail(err, errLen, "expected ':' after a field name");

    if (!strcmp(key, "id")) {
      if (!readString(p, out.id, sizeof out.id)) return fail(err, errLen, "id must be a string of up to 47 characters");
      if (!validId(out.id)) return fail(err, errLen, "id may use only letters, digits, '_' and '-'");
      haveId = true;
    } else if (!strcmp(key, "label")) {
      if (!readString(p, out.label, sizeof out.label)) return fail(err, errLen, "label must be a string of up to 47 characters");
      if (!out.label[0]) return fail(err, errLen, "label cannot be empty");
      haveLabel = true;
    } else if (!strcmp(key, "hour")) {
      long v;
      if (!readInt(p, v) || v < 0 || v > 23) return fail(err, errLen, "hour must be a whole number 0-23");
      out.hour = (uint8_t)v;
      haveHour = true;
    } else if (!strcmp(key, "minute")) {
      long v;
      if (!readInt(p, v) || v < 0 || v > 59) return fail(err, errLen, "minute must be a whole number 0-59");
      out.minute = (uint8_t)v;
      haveMinute = true;
    } else if (!strcmp(key, "room")) {
      char room[12];
      if (!readString(p, room, sizeof room) || !roomFromName(room, out.room))
        return fail(err, errLen, "room must be any, kitchen, bedroom, living or unknown");
    } else {
      return fail(err, errLen, "unknown field in an item");
    }

    skipWs(p);
    if (*p == ',') { p++; continue; }
    if (*p == '}') { p++; break; }
    return fail(err, errLen, "expected ',' or '}' in an item");
  }

  if (!haveId)                     return fail(err, errLen, "an item is missing its id");
  if (!haveLabel)                  return fail(err, errLen, "an item is missing its label");
  if (!haveHour || !haveMinute)    return fail(err, errLen, "an item is missing its time");
  return true;
}

bool scheduleReplace(const char* json, char* err, size_t errLen) {
  if (!json) return fail(err, errLen, "the request had no body");
  if (strlen(json) > SCHEDULE_JSON_MAX) return fail(err, errLen, "the routine is too large to store");

  // Parse into a scratch table so a bad item halfway down cannot leave the
  // band running half of the old routine and half of the new one.
  Prompt  next[SCHEDULE_MAX];
  uint8_t n = 0;

  const char* p = json;
  if (!expect(p, '{')) return fail(err, errLen, "the body must be a JSON object");

  bool sawItems = false;
  skipWs(p);
  if (*p != '}') {
    for (;;) {
      char key[16];
      if (!readString(p, key, sizeof key)) return fail(err, errLen, "expected a field name");
      if (!expect(p, ':')) return fail(err, errLen, "expected ':' after a field name");

      if (strcmp(key, "items")) {
        // Metadata such as "source" or "max": read past it and move on.
        if (!skipValue(p)) return fail(err, errLen, "could not read a field in the body");
      } else {
        if (sawItems) return fail(err, errLen, "the body lists items twice");
        sawItems = true;
        if (!expect(p, '[')) return fail(err, errLen, "items must be an array");
        skipWs(p);
        if (*p != ']') {
          for (;;) {
            if (n >= SCHEDULE_MAX) return fail(err, errLen, "the band holds at most 24 reminders");
            if (!parseItem(p, next[n], err, errLen)) return false;
            for (uint8_t i = 0; i < n; i++)
              if (!strcmp(next[i].id, next[n].id)) return fail(err, errLen, "two items share an id");
            n++;
            skipWs(p);
            if (*p == ',') { p++; continue; }
            if (*p == ']') break;
            return fail(err, errLen, "expected ',' or ']' in items");
          }
        }
        if (!expect(p, ']')) return fail(err, errLen, "items is missing its closing ']'");
      }

      skipWs(p);
      if (*p == ',') { p++; continue; }
      if (*p == '}') break;
      return fail(err, errLen, "expected ',' or '}' in the body");
    }
  }
  if (!expect(p, '}')) return fail(err, errLen, "the body ends before the routine does");
  skipWs(p);
  if (*p) return fail(err, errLen, "unexpected text after the routine");
  if (!sawItems) return fail(err, errLen, "the body must be {\"items\": [...]}");

  // Earliest first, so promptsTick() finds the oldest thing that is due.
  for (uint8_t i = 1; i < n; i++) {
    Prompt held = next[i];
    int j = i - 1;
    while (j >= 0 && next[j].hour * 60 + next[j].minute > held.hour * 60 + held.minute) {
      next[j + 1] = next[j];
      j--;
    }
    next[j + 1] = held;
  }

  memcpy(table, next, sizeof(Prompt) * n);
  tableLen = n;
  custom = true;
  return true;
}

// ---------------------------------------------------------------- storage

#if defined(ARDUINO_ARCH_ESP32)
static void scheduleSave(const char* json) {
  Preferences prefs;
  if (!prefs.begin(SCHEDULE_NVS_NS, false)) return;
  prefs.putString(SCHEDULE_NVS_KEY, json);
  prefs.end();
}

static bool scheduleLoad(char* out, size_t outLen) {
  Preferences prefs;
  if (!prefs.begin(SCHEDULE_NVS_NS, true)) return false;
  size_t n = prefs.getString(SCHEDULE_NVS_KEY, out, outLen);
  prefs.end();
  return n > 0;
}
#else
// The native test build has no NVS; the parsing above is what the tests cover.
static void scheduleSave(const char*) {}
static bool scheduleLoad(char*, size_t) { return false; }
#endif

void scheduleResetToDefaults() {
  tableLen = 0;
  for (size_t i = 0; i < DEFAULT_SCHEDULE_LEN && i < SCHEDULE_MAX; i++) {
    Prompt& slot = table[tableLen++];
    slot.hour = DEFAULT_SCHEDULE[i].hour;
    slot.minute = DEFAULT_SCHEDULE[i].minute;
    slot.room = DEFAULT_SCHEDULE[i].room;
    strlcpy(slot.id, DEFAULT_SCHEDULE[i].id, sizeof slot.id);
    strlcpy(slot.label, DEFAULT_SCHEDULE[i].label, sizeof slot.label);
  }
  custom = false;
}

void scheduleInit() {
  scheduleResetToDefaults();

  static char saved[SCHEDULE_JSON_MAX + 1];
  if (!scheduleLoad(saved, sizeof saved)) return;

  char err[64] = "";
  if (scheduleReplace(saved, err, sizeof err)) {
    Serial.printf("[schedule] restored %u reminders from NVS\n", tableLen);
  } else {
    // Firmware moved on and the stored body no longer parses: the defaults are
    // a working band, which beats a band with no reminders at all.
    Serial.printf("[schedule] stored routine rejected (%s), using defaults\n", err);
    scheduleResetToDefaults();
  }
}

// ---------------------------------------------------------------- accessors

uint8_t       scheduleCount()        { return tableLen; }
const Prompt& scheduleAt(uint8_t i)  { return table[i < tableLen ? i : 0]; }
bool          scheduleIsCustom()     { return custom; }

int scheduleFindById(const char* id) {
  for (uint8_t i = 0; i < tableLen; i++)
    if (!strcmp(table[i].id, id)) return i;
  return -1;
}

String scheduleJson() {
  String out;
  out.reserve(256 + tableLen * 96);
  char header[64];
  snprintf(header, sizeof(header), "{\"source\":\"%s\",\"max\":%d,\"items\":[",
           custom ? "dashboard" : "defaults", SCHEDULE_MAX);
  out = header;
  for (uint8_t i = 0; i < tableLen; i++) {
    char item[192];
    snprintf(item, sizeof(item),
             "%s{\"id\":\"%s\",\"label\":\"%s\",\"hour\":%u,\"minute\":%u,\"room\":\"%s\"}",
             i ? "," : "", table[i].id, table[i].label,
             table[i].hour, table[i].minute,
             table[i].room == ANY_ROOM ? "any" : roomName(table[i].room));
    out += item;
  }
  out += "]}";
  return out;
}

// Persisting is separate from applying so POST /schedule can answer the
// dashboard the moment the routine is running, even if the flash write is slow.
void schedulePersist(const char* json) { scheduleSave(json); }
