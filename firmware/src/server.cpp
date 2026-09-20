#include "server.h"
#include "config.h"
#include "events.h"
#include "schedule.h"
#include "location.h"
#include "prompts.h"
#include "safety.h"
#include "gps.h"
#include <WebServer.h>
#include <sys/time.h>

static WebServer http(80);

static void cors() {
  http.sendHeader("Access-Control-Allow-Origin", "*");
  http.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  http.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

static void reply(int code, const String& json) {
  cors();
  http.send(code, "application/json", json);
}

static void handleState() {
  char pending[PROMPT_ID_LEN + 4] = "null";
  if (g_state.pendingPrompt >= 0)
    snprintf(pending, sizeof(pending), "\"%s\"", scheduleAt(g_state.pendingPrompt).id);

  // Without a fix the position fields are null, not stale coordinates.
  char gps[160];
  if (gpsFixValid())
    snprintf(gps, sizeof(gps),
             "{\"fix\":true,\"sats\":%u,\"lat\":%.6f,\"lon\":%.6f,\"distanceHomeM\":%d,\"heading\":\"%s\"}",
             g_state.sats, g_state.lat, g_state.lon, (int)g_state.distanceHomeM, gpsHomeHeading());
  else
    snprintf(gps, sizeof(gps),
             "{\"fix\":false,\"sats\":%u,\"lat\":null,\"lon\":null,\"distanceHomeM\":null,\"heading\":\"\"}",
             g_state.sats);

  char out[640];
  snprintf(out, sizeof(out),
           "{\"ts\":%lu,\"activity\":\"%s\",\"room\":\"%s\",\"roomConfidence\":%u,"
           "\"worn\":%s,\"wanderFlag\":%s,\"awayFromHome\":%s,\"pendingPrompt\":%s,\"gps\":%s}",
           (unsigned long)time(nullptr), activityName(g_state.activity), roomName(g_state.room),
           g_state.roomConfidence, g_state.worn ? "true" : "false",
           g_state.wanderFlag ? "true" : "false", g_state.awayFromHome ? "true" : "false",
           pending, gps);
  reply(200, out);
}

// The caregiver can tick the prompt off from the dashboard when they are in
// the room and can see it was done.
static void handleAck() {
  if (!promptsAckPending()) return reply(409, "{\"error\":\"no pending prompt\"}");
  reply(200, "{\"ok\":true}");
}

// Stop the away-from-home chime; the alert stays up on the dashboard.
static void handleSilence() {
  safetySilence();
  reply(200, "{\"ok\":true}");
}

static void handleEvents() {
  uint32_t since = http.hasArg("since") ? http.arg("since").toInt() : 0;
  reply(200, eventsJsonSince(since));
}

static void handleDemoFire() {
  if (!http.hasArg("id")) return reply(400, "{\"error\":\"missing id\"}");
  int idx = promptsFindById(http.arg("id").c_str());
  if (idx < 0) return reply(404, "{\"error\":\"unknown id\"}");
  promptsDemoFire(idx);
  reply(200, "{\"ok\":true}");
}

// The routine the caregiver built on the dashboard, applied to the wrist.
// Without this the band could only ever remind about what was compiled into
// config.h, and the routine editor was a list that talked to nobody.
static void handleSchedulePush() {
  if (!http.hasArg("plain")) return reply(400, "{\"error\":\"missing body\"}");
  String body = http.arg("plain");

  char err[80] = "";
  if (!scheduleReplace(body.c_str(), err, sizeof(err))) {
    char out[160];
    snprintf(out, sizeof(out), "{\"error\":\"%s\"}", err);
    return reply(400, out);
  }

  // The routine is live from here; indices into the old table are stale.
  promptsScheduleChanged();
  schedulePersist(body.c_str());
  addEvent("schedule_set", "dashboard");

  char out[64];
  snprintf(out, sizeof(out), "{\"ok\":true,\"count\":%u}", scheduleCount());
  reply(200, out);
}

static void handleScheduleGet() { reply(200, scheduleJson()); }

// A raw WiFi scan, so tools/fingerprint_trainer.py can learn the rooms from
// the band's own radio rather than from a laptop standing somewhere else.
static void handleScan() { reply(200, locationScanJson()); }

static void handleTime() {
  if (!http.hasArg("epoch")) return reply(400, "{\"error\":\"missing epoch\"}");
  struct timeval tv = {(time_t)http.arg("epoch").toInt(), 0};
  settimeofday(&tv, nullptr);
  reply(200, "{\"ok\":true}");
}

void serverInit() {
  http.on("/state",     HTTP_GET,  handleState);
  http.on("/events",    HTTP_GET,  handleEvents);
  http.on("/schedule",  HTTP_GET,  handleScheduleGet);
  http.on("/schedule",  HTTP_POST, handleSchedulePush);
  http.on("/scan",      HTTP_GET,  handleScan);
  http.on("/demo/fire", HTTP_POST, handleDemoFire);
  http.on("/ack",       HTTP_POST, handleAck);
  http.on("/silence",   HTTP_POST, handleSilence);
  http.on("/time",      HTTP_POST, handleTime);
  for (const char* path : {"/state", "/events", "/schedule", "/scan",
                           "/demo/fire", "/ack", "/silence", "/time"}) {
    http.on(path, HTTP_OPTIONS, [] {
      cors();
      http.send(204);
    });
  }
  http.onNotFound([] { reply(404, "{\"error\":\"not found\"}"); });
  http.begin();
}

void serverTick() { http.handleClient(); }
