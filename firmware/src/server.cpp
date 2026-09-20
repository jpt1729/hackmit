#include "server.h"
#include "config.h"
#include "events.h"
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
  char pending[32] = "null";
  if (g_state.pendingPrompt >= 0) snprintf(pending, sizeof(pending), "\"%s\"", SCHEDULE[g_state.pendingPrompt].id);

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

  char out[512];
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

static void handleTime() {
  if (!http.hasArg("epoch")) return reply(400, "{\"error\":\"missing epoch\"}");
  struct timeval tv = {(time_t)http.arg("epoch").toInt(), 0};
  settimeofday(&tv, nullptr);
  reply(200, "{\"ok\":true}");
}

void serverInit() {
  http.on("/state",     HTTP_GET,  handleState);
  http.on("/events",    HTTP_GET,  handleEvents);
  http.on("/demo/fire", HTTP_POST, handleDemoFire);
  http.on("/ack",       HTTP_POST, handleAck);
  http.on("/silence",   HTTP_POST, handleSilence);
  http.on("/time",      HTTP_POST, handleTime);
  for (const char* path : {"/state", "/events", "/demo/fire", "/ack", "/silence", "/time"}) {
    http.on(path, HTTP_OPTIONS, [] {
      cors();
      http.send(204);
    });
  }
  http.onNotFound([] { reply(404, "{\"error\":\"not found\"}"); });
  http.begin();
}

void serverTick() { http.handleClient(); }
