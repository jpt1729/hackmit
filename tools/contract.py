#!/usr/bin/env python3
"""Firmware <-> dashboard JSON contract (README §5), as executable checks.

Run with no args for the offline checks (no device needed):
    python3 tools/contract.py
  - docs/data/demo.json matches the contract
  - schedule ids in firmware/src/config.h match docs/js/checklist.js
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
               "geofence_exit", "geofence_return"}
STATE_KEYS = {"ts", "activity", "room", "roomConfidence", "worn", "wanderFlag",
              "awayFromHome", "pendingPrompt", "gps"}
GPS_KEYS = {"fix", "sats", "lat", "lon", "distanceHomeM", "heading"}
COMPASS = {"", "N", "NE", "E", "SE", "S", "SW", "W", "NW"}


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
    for k in ("worn", "wanderFlag", "awayFromHome"):
        if k in s and not isinstance(s[k], bool):
            errs.append(f"{k}={s[k]!r} not a bool")
    errs += validate_gps(s.get("gps"))
    p = s.get("pendingPrompt")
    if p is not None and not isinstance(p, str):
        errs.append(f"pendingPrompt={p!r} not null or string")
    if isinstance(p, str) and schedule_ids and p not in schedule_ids:
        errs.append(f"pendingPrompt={p!r} not a schedule id {sorted(schedule_ids)}")
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
    src = (ROOT / "firmware/src/config.h").read_text()
    block = re.search(r"SCHEDULE\[\]\s*=\s*\{(.*?)\n\};", src, re.S)
    return re.findall(r'\{\s*\d+\s*,\s*\d+\s*,\s*"([^"]+)"', block.group(1)) if block else []


def dashboard_schedule_ids():
    path = ROOT / "docs/js/checklist.js"
    return re.findall(r'\bid:\s*"([^"]+)"', path.read_text()) if path.exists() else []


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
    check("config.h SCHEDULE parses", [] if fw_ids else ["could not find any SCHEDULE ids"])

    dash_ids = dashboard_schedule_ids()
    errs = []
    if set(fw_ids) - set(dash_ids):
        errs.append(f"in firmware but not dashboard checklist: {sorted(set(fw_ids) - set(dash_ids))}")
    if set(dash_ids) - set(fw_ids):
        errs.append(f"in dashboard but not firmware (can never be checked off): "
                    f"{sorted(set(dash_ids) - set(fw_ids))}")
    check("schedule ids: config.h == docs/js/checklist.js", errs)

    rooms = firmware_room_names()
    check("roomName() outputs are contract rooms",
          [f"firmware emits room {r!r} not in contract" for r in sorted(rooms - ROOMS)])

    demo = json.loads((ROOT / "docs/data/demo.json").read_text())
    check("docs/data/demo.json state", validate_state(demo.get("state"), set(fw_ids)))
    check("docs/data/demo.json events",
          validate_events({"events": demo.get("events")}, 0, set(fw_ids)))

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
