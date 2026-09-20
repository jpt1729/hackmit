#!/usr/bin/env python3
"""Record a real session off the wristband into app/data/demo.json. Stdlib only.

    python3 tools/capture_demo.py 192.168.1.42                 # snapshot whatever is in the log
    python3 tools/capture_demo.py 192.168.1.42 --minutes 10     # follow the band for 10 minutes
    python3 tools/capture_demo.py 192.168.1.42 --fire meds_9am  # trigger a reminder, then record it
    python3 tools/capture_demo.py 192.168.1.42 --dry-run        # print, do not write

demo.json is what the website falls back to when there is no band on the
network - the venue WiFi, the battery, the five minutes before the judges
arrive. It is also the only thing the GitHub Pages copy can ever show, because
an HTTPS page cannot reach a device at http://192.168.x.x. Capturing it from
real hardware rather than writing it by hand is the difference between a demo
that shows the product and one that shows a mock-up.

The capture is validated against the same contract the live device is held to,
so a recording that would not have been accepted from the band is not written.
"""
import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from contract import ROOT, validate_events, validate_state  # noqa: E402

# Events worth keeping in a demo reel. A capture is mostly long stretches of
# nothing; these are the moments that show what the band is for.
INTERESTING = ("prompt_fired", "prompt_acked", "prompt_missed",
               "wear_on", "wear_off", "room_change", "wander",
               "geofence_exit", "geofence_return")


def request(base, method, path, timeout=5.0):
    req = urllib.request.Request(base + path, method=method)
    with urllib.request.urlopen(req, timeout=timeout) as response:
        return json.loads(response.read().decode())


def schedule_ids(base):
    """Ids the band is actually running, which may be a routine the website pushed."""
    try:
        return {item["id"] for item in request(base, "GET", "/schedule").get("items", [])}
    except (urllib.error.URLError, OSError, ValueError, KeyError):
        return set()


def follow(base, minutes, since):
    """Poll like the website does, collecting events as they happen."""
    collected, last = [], since
    deadline = time.time() + minutes * 60
    print(f"     recording for {minutes:g} min (Ctrl-C to stop early and keep what you have)")
    try:
        while time.time() < deadline:
            body = request(base, "GET", f"/events?since={last}")
            for event in body.get("events", []):
                last = event["id"]
                collected.append(event)
                stamp = time.strftime("%H:%M:%S", time.localtime(event["ts"]))
                print(f"     {stamp}  {event['type']:<16} {event['detail']}")
            time.sleep(3)
    except KeyboardInterrupt:
        print("\n     stopped early")
    return collected, last


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("host", help="device IP, as printed on serial at boot")
    parser.add_argument("--minutes", type=float, default=0,
                        help="follow the band for this long instead of taking a snapshot")
    parser.add_argument("--fire", metavar="PROMPT_ID",
                        help="trigger this reminder before recording, so the reel has one in it")
    parser.add_argument("--max-events", type=int, default=12,
                        help="keep at most this many events (default 12)")
    parser.add_argument("--out", default=str(ROOT / "app/data/demo.json"))
    parser.add_argument("--dry-run", action="store_true", help="print the capture, write nothing")
    args = parser.parse_args()

    base = "http://" + args.host.removeprefix("http://").rstrip("/")

    try:
        state = request(base, "GET", "/state")
    except (urllib.error.URLError, OSError, ValueError) as error:
        print(f"FAIL cannot reach {base}: {error}\n"
              "     Same network? Venue WiFi client isolation? Try a phone hotspot.")
        return 1

    ids = schedule_ids(base)
    problems = validate_state(state, ids or None)
    if problems:
        # A snapshot that breaks the contract would make the offline website
        # behave differently from the live one, which is the one thing it must not do.
        print("FAIL /state does not match the contract, refusing to record it:")
        for problem in problems:
            print("     - " + problem)
        return 1
    if state["ts"] < 1700000000:
        print("FAIL the device clock is unset, so every timestamp would be wrong.\n"
              "     Open the website against the band, or run "
              "tools/test_device_http.py <host> --set-time, then try again.")
        return 1

    since = 0
    existing = request(base, "GET", "/events").get("events", [])
    if args.fire:
        if ids and args.fire not in ids:
            print(f"FAIL {args.fire!r} is not on the band. It is running: {sorted(ids)}")
            return 1
        since = existing[-1]["id"] if existing else 0
        request(base, "POST", f"/demo/fire?id={args.fire}")
        print(f"     fired {args.fire} - shake the band to acknowledge it")
        if not args.minutes:
            args.minutes = 2

    if args.minutes:
        events, _ = follow(base, args.minutes, since)
        state = request(base, "GET", "/state")
    else:
        events = existing

    events = [e for e in events if e["type"] in INTERESTING][-args.max_events:]
    if not events:
        print("FAIL nothing worth replaying was recorded. Try --fire or a longer --minutes.")
        return 1

    problems = validate_events({"events": events}, events[0]["id"] - 1, ids or None)
    problems += validate_state(state, ids or None)
    if problems:
        print("FAIL the capture does not match the contract, refusing to write it:")
        for problem in problems:
            print("     - " + problem)
        return 1

    capture = {"state": state, "events": events}
    rendered = json.dumps(capture, indent=2) + "\n"
    if args.dry_run:
        print(rendered)
        return 0

    with open(args.out, "w", encoding="utf-8") as handle:
        handle.write(rendered)
    span = events[-1]["ts"] - events[0]["ts"]
    print(f"\nPASS wrote {args.out}: {len(events)} events over {span // 60}m{span % 60:02d}s")
    print("     The website replays this whenever no band answers.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
