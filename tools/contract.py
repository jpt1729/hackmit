#!/usr/bin/env python3
"""Firmware <-> dashboard JSON contract (docs/spec.md §5), as executable checks.

Run with no args for the offline checks (no device needed):
    python3 tools/contract.py
  - app/data/demo.json matches the contract
  - the band's fallback routine is one the dashboard knows how to draw
  - what the dashboard would push to POST /schedule is something the band takes
  - room names in firmware/src/state.h match what the contract allows

tools/test_device_http.py imports the validators to check a live ESP32.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

ACTIVITIES = {"sleeping", "resting", "moving"}
ROOMS = {"unknown", "kitchen", "bedroom", "living"}
EVENT_TYPES = {"prompt_fired", "prompt_acked", "prompt_missed",
               "wear_on", "wear_off", "room_change", "wander",
               "geofence_exit", "geofence_return", "schedule_set",
               "fall_detected", "fall_cancelled", "fall_alert", "fall_ems",
               "message_received", "message_read", "message_expired"}
FALL_STAGES = {"none", "confirming", "caregiver", "ems"}
STATE_KEYS = {"ts", "activity", "room", "roomConfidence", "worn", "wanderFlag",
              "awayFromHome", "pendingPrompt", "tasksDone", "tasksTotal",
              "fallStage", "messageWaiting", "gps"}
GPS_KEYS = {"fix", "sats", "lat", "lon", "distanceHomeM", "heading"}
COMPASS = {"", "N", "NE", "E", "SE", "S", "SW", "W", "NW"}
# firmware/src/schedule.h
SCHEDULE_MAX = 24
PROMPT_ID_LEN = 48
PROMPT_LABEL_LEN = 48
SCHEDULE_ROOMS = ROOMS | {"any"}
ID_RE = re.compile(r"^[A-Za-z0-9_-]+$")


def _is_int(v):
    return isinstance(v, int) and not isinstance(v, bool)


def validate_state(s, schedule_ids=None):
    """Return a list of problems with a /state object (empty = OK)."""
    errs = []
    if not isinstance(s, dict):
        return [f"state is {type(s).__name__}, expected object"]
    missing, extra = STATE_KEYS - s.keys(), s.keys() - STATE_KEYS
    if missing:
        errs.append(f"state missing keys {sorted(missing)}")
    if extra:
        errs.append(f"state has unexpected keys {sorted(extra)}")
    if "ts" in s and not (_is_int(s["ts"]) and s["ts"] >= 0):
        errs.append(f"ts={s['ts']!r} not a non-negative int")
    if "activity" in s and s["activity"] not in ACTIVITIES:
        errs.append(f"activity={s['activity']!r} not in {sorted(ACTIVITIES)}")
    if "room" in s and s["room"] not in ROOMS:
        errs.append(f"room={s['room']!r} not in {sorted(ROOMS)}")
    if "roomConfidence" in s and not (_is_int(s["roomConfidence"]) and 0 <= s["roomConfidence"] <= 100):
        errs.append(f"roomConfidence={s['roomConfidence']!r} not an int 0-100")
    for k in ("worn", "wanderFlag", "awayFromHome", "messageWaiting"):
        if k in s and not isinstance(s[k], bool):
            errs.append(f"{k}={s[k]!r} not a bool")
    for k in ("tasksDone", "tasksTotal"):
        if k in s and not (_is_int(s[k]) and 0 <= s[k] <= 255):
            errs.append(f"{k}={s[k]!r} not an int 0-255")
    if _is_int(s.get("tasksDone")) and _is_int(s.get("tasksTotal"))             and s["tasksDone"] > s["tasksTotal"]:
        errs.append(f"tasksDone={s['tasksDone']} exceeds tasksTotal={s['tasksTotal']}")
    if "fallStage" in s and s["fallStage"] not in FALL_STAGES:
        errs.append(f"fallStage={s['fallStage']!r} not in {sorted(FALL_STAGES)}")
    errs += validate_gps(s.get("gps"))
    p = s.get("pendingPrompt")
    if p is not None and not isinstance(p, str):
        errs.append(f"pendingPrompt={p!r} not null or string")
    if isinstance(p, str) and schedule_ids and p not in schedule_ids:
        errs.append(f"pendingPrompt={p!r} not a schedule id {sorted(schedule_ids)}")
    return errs


def validate_schedule(body):
    """Return a list of problems with a /schedule response body.

    The dashboard pushes the caregiver's routine here and reads it back, so the
    two directions have to agree: whatever GET returns must be something POST
    would accept.
    """
    if not isinstance(body, dict):
        return [f"schedule is {type(body).__name__}, expected object"]
    errs = []
    extra = body.keys() - {"source", "max", "items"}
    if extra:
        errs.append(f"schedule has unexpected keys {sorted(extra)}")
    if body.get("source") not in ("dashboard", "defaults"):
        errs.append(f"schedule.source={body.get('source')!r} not 'dashboard' or 'defaults'")
    if body.get("max") != SCHEDULE_MAX:
        errs.append(f"schedule.max={body.get('max')!r}, expected {SCHEDULE_MAX}")
    items = body.get("items")
    if not isinstance(items, list):
        return errs + [f"schedule.items is {type(items).__name__}, expected list"]
    if len(items) > SCHEDULE_MAX:
        errs.append(f"schedule has {len(items)} items, more than the band holds ({SCHEDULE_MAX})")

    seen, previous = set(), -1
    for i, item in enumerate(items):
        where = f"schedule.items[{i}]"
        if not isinstance(item, dict) or set(item.keys()) != {"id", "label", "hour", "minute", "room"}:
            errs.append(f"{where} must have exactly id, label, hour, minute, room: {item!r}")
            continue
        if not isinstance(item["id"], str) or not ID_RE.match(item["id"] or "") \
                or len(item["id"]) >= PROMPT_ID_LEN:
            errs.append(f"{where} id={item['id']!r} is not a usable prompt id")
        elif item["id"] in seen:
            errs.append(f"{where} id={item['id']!r} appears twice")
        else:
            seen.add(item["id"])
        if not isinstance(item["label"], str) or not item["label"] or len(item["label"]) >= PROMPT_LABEL_LEN:
            errs.append(f"{where} label={item['label']!r} is empty or too long for the band")
        if not (_is_int(item["hour"]) and 0 <= item["hour"] <= 23):
            errs.append(f"{where} hour={item['hour']!r} not an int 0-23")
        if not (_is_int(item["minute"]) and 0 <= item["minute"] <= 59):
            errs.append(f"{where} minute={item['minute']!r} not an int 0-59")
        if item["room"] not in SCHEDULE_ROOMS:
            errs.append(f"{where} room={item['room']!r} not in {sorted(SCHEDULE_ROOMS)}")
        # The band relies on the order to find the oldest reminder that is due.
        if _is_int(item.get("hour")) and _is_int(item.get("minute")):
            at = item["hour"] * 60 + item["minute"]
            if at < previous:
                errs.append(f"{where} is out of order: {at // 60:02d}:{at % 60:02d} "
                            f"after {previous // 60:02d}:{previous % 60:02d}")
            previous = max(previous, at)
    return errs


def validate_scan(body):
    """Return a list of problems with a /scan response body."""
    if not isinstance(body, dict) or set(body.keys()) != {"aps"} or not isinstance(body["aps"], list):
        return ['scan body must be exactly {"aps": [...]}']
    errs = []
    for i, ap in enumerate(body["aps"]):
        where = f"aps[{i}]"
        if not isinstance(ap, dict) or set(ap.keys()) != {"bssid", "rssi", "ssid", "channel"}:
            errs.append(f"{where} must have exactly bssid, rssi, ssid, channel: {ap!r}")
            continue
        if not isinstance(ap["bssid"], str) or not re.fullmatch(r"(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}", ap["bssid"]):
            errs.append(f"{where} bssid={ap['bssid']!r} is not a MAC address")
        if not (_is_int(ap["rssi"]) and -120 <= ap["rssi"] <= 0):
            errs.append(f"{where} rssi={ap['rssi']!r} not an int -120..0 dBm")
        if not _is_int(ap["channel"]) or not 1 <= ap["channel"] <= 196:
            errs.append(f"{where} channel={ap['channel']!r} not a WiFi channel")
        if not isinstance(ap["ssid"], str):
            errs.append(f"{where} ssid={ap['ssid']!r} not a string")
    return errs


def validate_gps(g):
    """Return a list of problems with the gps sub-object of /state."""
    if g is None:
        return ["state has no gps object"]
    if not isinstance(g, dict):
        return [f"gps is {type(g).__name__}, expected object"]
    errs = []
    missing, extra = GPS_KEYS - g.keys(), g.keys() - GPS_KEYS
    if missing:
        errs.append(f"gps missing keys {sorted(missing)}")
    if extra:
        errs.append(f"gps has unexpected keys {sorted(extra)}")
    if "fix" in g and not isinstance(g["fix"], bool):
        errs.append(f"gps.fix={g['fix']!r} not a bool")
    if "sats" in g and not (_is_int(g["sats"]) and 0 <= g["sats"] <= 64):
        errs.append(f"gps.sats={g['sats']!r} not an int 0-64")
    if g.get("heading") not in COMPASS:
        errs.append(f"gps.heading={g.get('heading')!r} not a compass point")
    # Without a fix the position fields must be null, never a stale position.
    for k, lo, hi in (("lat", -90, 90), ("lon", -180, 180), ("distanceHomeM", 0, 40_000_000)):
        v = g.get(k)
        if not g.get("fix"):
            if v is not None:
                errs.append(f"gps.{k}={v!r} should be null without a fix")
        elif not isinstance(v, (int, float)) or isinstance(v, bool) or not lo <= v <= hi:
            errs.append(f"gps.{k}={v!r} not a number in [{lo}, {hi}]")
    return errs


def validate_events(body, since=0, schedule_ids=None):
    """Return a list of problems with an /events response body."""
    if not isinstance(body, dict) or set(body.keys()) != {"events"} or not isinstance(body["events"], list):
        return ['events body must be exactly {"events": [...]}']
    errs, prev = [], since
    for i, e in enumerate(body["events"]):
        where = f"events[{i}]"
        if not isinstance(e, dict) or set(e.keys()) != {"id", "ts", "type", "detail"}:
            errs.append(f"{where} must have exactly id, ts, type, detail: {e!r}")
            continue
        if not _is_int(e["id"]) or e["id"] <= prev:
            errs.append(f"{where} id={e['id']!r} not increasing (prev {prev}, since {since})")
        else:
            prev = e["id"]
        if not _is_int(e["ts"]):
            errs.append(f"{where} ts={e['ts']!r} not an int")
        if e["type"] not in EVENT_TYPES:
            errs.append(f"{where} type={e['type']!r} not in the frozen vocabulary")
        if not isinstance(e["detail"], str):
            errs.append(f"{where} detail={e['detail']!r} not a string")
            continue
        if e["type"] in ("prompt_fired", "prompt_acked", "prompt_missed") and schedule_ids \
                and e["detail"] not in schedule_ids:
            errs.append(f"{where} {e['type']} detail={e['detail']!r} is not a schedule id")
        if e["type"] == "room_change" and e["detail"] not in ROOMS:
            errs.append(f"{where} room_change detail={e['detail']!r} not a room")
    return errs


# ---------------------------------------------------------------- source parsing

def firmware_schedule_ids():
    """Ids of the band's built-in fallback routine (config.h DEFAULT_SCHEDULE)."""
    src = (ROOT / "firmware/src/config.h").read_text()
    block = re.search(r"DEFAULT_SCHEDULE\[\]\s*=\s*\{(.*?)\n\};", src, re.S)
    return re.findall(r'\{\s*\d+\s*,\s*\d+\s*,\s*"([^"]+)"', block.group(1)) if block else []


def dashboard_schedule_ids():
    return [item["id"] for item in dashboard_routine()]


def dashboard_routine():
    """The dashboard's starting routine, parsed out of DEFAULT_ROUTINE_ITEMS."""
    src = (ROOT / "app/js/checklist.js").read_text()
    block = re.search(r"DEFAULT_ROUTINE_ITEMS\s*=\s*\[(.*?)\n\];", src, re.S)
    if not block:
        return []
    items = []
    for line in block.group(1).splitlines():
        found = re.search(r'id:\s*"([^"]+)".*?minutes:\s*(\d+)', line)
        label = re.search(r'label:\s*"([^"]+)"', line)
        if found and label:
            items.append({"id": found.group(1), "label": label.group(1),
                          "minutes": int(found.group(2))})
    return items


def dashboard_schedule_push():
    """The body app/js/device.js would POST to /schedule for that routine."""
    items = sorted(dashboard_routine(), key=lambda item: item["minutes"])[:SCHEDULE_MAX]
    return {
        "source": "dashboard",
        "max": SCHEDULE_MAX,
        "items": [{"id": item["id"], "label": item["label"][:PROMPT_LABEL_LEN - 1],
                   "hour": item["minutes"] // 60, "minute": item["minutes"] % 60,
                   "room": "any"} for item in items],
    }


def firmware_room_names():
    src = (ROOT / "firmware/src/state.h").read_text()
    fn = re.search(r"roomName\(Room r\)\s*\{(.*?)\n\}", src, re.S)
    return set(re.findall(r'return "([^"]+)"', fn.group(1))) if fn else set()


def main():
    failures = 0

    def check(name, errs):
        nonlocal failures
        print(("PASS " if not errs else "FAIL ") + name)
        for e in errs:
            print("     - " + e)
        failures += bool(errs)

    fw_ids = firmware_schedule_ids()
    check("config.h DEFAULT_SCHEDULE parses",
          [] if fw_ids else ["could not find any DEFAULT_SCHEDULE ids"])

    # The dashboard is the source of truth: it pushes its routine to the band
    # over POST /schedule. The two lists no longer have to be identical - but a
    # band running its built-in fallback still has to line up with what a fresh
    # dashboard draws, or those reminders arrive with nowhere to be shown.
    dash_ids = dashboard_schedule_ids()
    check("config.h fallback routine is drawable by app/js/checklist.js",
          [f"the band falls back to {i!r}, which the dashboard has no activity for"
           for i in sorted(set(fw_ids) - set(dash_ids))])

    # The push direction: what the routine editor would send has to be a body
    # firmware/src/schedule.cpp accepts, and it is the band that decides that.
    check("app/js/checklist.js routine is a valid POST /schedule body",
          validate_schedule(dashboard_schedule_push()))

    rooms = firmware_room_names()
    check("roomName() outputs are contract rooms",
          [f"firmware emits room {r!r} not in contract" for r in sorted(rooms - ROOMS)])

    demo = json.loads((ROOT / "app/data/demo.json").read_text())
    check("app/data/demo.json state", validate_state(demo.get("state"), set(fw_ids)))
    check("app/data/demo.json events",
          validate_events({"events": demo.get("events")}, 0, set(fw_ids)))

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
