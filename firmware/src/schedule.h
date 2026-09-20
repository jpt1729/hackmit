#pragma once
#include <Arduino.h>
#include "state.h"

// The reminder table the band actually runs on.
//
// It used to be a `static const` array compiled into config.h, which meant the
// routine a caregiver built on the dashboard could never reach the wrist. This
// module makes the table live: config.h supplies the first-boot defaults, the
// dashboard replaces them with POST /schedule, and the accepted table is kept
// in NVS so a flat battery does not undo the setup.
//
// A replacement is all-or-nothing. A body that fails to parse leaves the
// running schedule exactly as it was - a half-applied routine is worse than an
// out-of-date one.

#define SCHEDULE_MAX     24
// Long enough for the dashboard's `custom_<uuid>` ids (43 characters).
#define PROMPT_ID_LEN    48
#define PROMPT_LABEL_LEN 48

struct Prompt {
  uint8_t hour, minute;
  Room    room;
  char    id[PROMPT_ID_LEN];
  char    label[PROMPT_LABEL_LEN];
};

void          scheduleInit();          // NVS if a routine was pushed before, else defaults
void          scheduleResetToDefaults();
uint8_t       scheduleCount();
const Prompt& scheduleAt(uint8_t i);
int           scheduleFindById(const char* id);   // -1 when absent
String        scheduleJson();                     // GET /schedule body

// Parse and apply a POST /schedule body. On failure returns false, writes a
// caregiver-readable reason into `err`, and leaves the running table alone.
bool          scheduleReplace(const char* json, char* err, size_t errLen);

// True once a schedule came from the dashboard rather than config.h.
bool          scheduleIsCustom();

// Write an accepted body to NVS. Kept separate from scheduleReplace() so the
// dashboard gets its answer as soon as the routine is live, not after flash.
void          schedulePersist(const char* json);
