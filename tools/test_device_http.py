#!/usr/bin/env python3
"""Black-box tests against a running ESP32 over the LAN. Stdlib only.

    python3 tools/test_device_http.py 192.168.1.42            # read-only checks
    python3 tools/test_device_http.py 192.168.1.42 --fire     # + POST /demo/fire (device buzzes)
    python3 tools/test_device_http.py 192.168.1.42 --set-time # + POST /time with this laptop's clock
    python3 tools/test_device_http.py 192.168.1.42 --soak 30  # + poll like the dashboard for 30 min
    python3 tools/test_device_http.py 192.168.1.42 --schedule  # + push a routine and put the old one back

Run it on the same network the demo will use (phone hotspot), from the laptop
that will run the dashboard: it also proves client isolation isn't in the way.
Exit code is non-zero if anything failed.
"""
import argparse
import json
import statistics
import sys
import time
import urllib.error
import urllib.request

sys.path.insert(0, __import__("os").path.dirname(__file__))
from contract import (firmware_schedule_ids, validate_events,  # noqa: E402
                      validate_scan, validate_schedule, validate_state)

# The ids the band is actually running, which is whatever the dashboard last
# pushed. Falls back to the firmware defaults if /schedule cannot be read.
SCHEDULE_IDS = set(firmware_schedule_ids())
failures = 0


def report(name, errs):
    global failures
    print(("PASS " if not errs else "FAIL ") + name)
    for e in errs:
        print("     - " + e)
    failures += bool(errs)


def request(base, method, path, timeout=5.0, body=None):
    """Return (status, headers, body_text, seconds). Never raises on HTTP errors."""
    headers = {"Origin": "http://localhost:8000"}
    if body is not None:
        headers["Content-Type"] = "application/json"
    req = urllib.request.Request(base + path, method=method, headers=headers,
                                 data=body.encode() if body is not None else None)
    t0 = time.monotonic()
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, r.headers, r.read().decode(), time.monotonic() - t0
    except urllib.error.HTTPError as e:
        return e.code, e.headers, e.read().decode(), time.monotonic() - t0


def get_json(base, path):
    status, headers, body, secs = request(base, "GET", path)
    return status, headers, json.loads(body), secs


def cors_errs(headers):
    errs = []
    if headers.get("Access-Control-Allow-Origin") != "*":
        errs.append(f"Access-Control-Allow-Origin={headers.get('Access-Control-Allow-Origin')!r}, want '*'")
    return errs


# ---------------------------------------------------------------- checks

def check_state(base):
    status, headers, s, _ = get_json(base, "/state")
    errs = [] if status == 200 else [f"status {status}"]
    if "application/json" not in (headers.get("Content-Type") or ""):
        errs.append(f"Content-Type={headers.get('Content-Type')!r}")
    errs += cors_errs(headers) + validate_state(s, SCHEDULE_IDS)
    report("GET /state matches contract", errs)
    print(f"     {json.dumps(s)}")

    drift = s.get("ts", 0) - time.time()
    if s.get("ts", 0) < 1700000000:
        report("device clock is set", ["ts looks unset: NTP failed. Rerun with --set-time, "
                                       "or scheduled prompts will never fire"])
    else:
        # ts is UTC epoch; TZ only affects the device's localtime.
        report("device clock is set", [] if abs(drift) < 120 else [f"clock off by {drift:+.0f}s"])
    return s


def check_events(base):
    status, headers, body, _ = get_json(base, "/events")
    errs = ([] if status == 200 else [f"status {status}"]) + cors_errs(headers)
    errs += validate_events(body, 0, SCHEDULE_IDS)
    report(f"GET /events matches contract ({len(body.get('events', []))} events)", errs)
    events = body.get("events", [])

    last = events[-1]["id"] if events else 0
    _, _, body2, _ = get_json(base, f"/events?since={last}")
    newer = body2.get("events", [])
    report("GET /events?since=<last> returns only newer events",
           [f"got id {e['id']} <= {last}" for e in newer if e["id"] <= last])
    if len(events) >= 2:
        mid = events[len(events) // 2]["id"]
        _, _, body3, _ = get_json(base, f"/events?since={mid}")
        report("GET /events?since=<mid> filters correctly",
               validate_events(body3, mid, SCHEDULE_IDS))
    return last


def check_schedule(base, push=False):
    """The routine the dashboard pushes to the band, and reads back."""
    global SCHEDULE_IDS
    status, headers, body, _ = get_json(base, "/schedule")
    errs = ([] if status == 200 else [f"status {status}"]) + cors_errs(headers)
    errs += validate_schedule(body)
    report(f"GET /schedule matches contract ({len(body.get('items', []))} reminders, "
           f"source {body.get('source')!r})", errs)
    if not errs:
        SCHEDULE_IDS = {item["id"] for item in body["items"]}

    # A malformed routine must be refused whole: a band running half of an old
    # routine and half of a new one is worse than one that ignored the push.
    before = json.dumps(body.get("items"))
    status, _, reply, _ = request(base, "POST", "/schedule",
                                  body='{"items":[{"id":"bad","label":"Bad","hour":99,"minute":0}]}')
    _, _, after, _ = get_json(base, "/schedule")
    errs = [] if status == 400 else [f"a routine with hour=99 got status {status}, want 400"]
    try:
        if not json.loads(reply).get("error"):
            errs.append(f"the refusal gave no reason: {reply!r}")
    except ValueError:
        errs.append(f"the refusal is not JSON: {reply!r}")
    if json.dumps(after.get("items")) != before:
        errs.append("a refused routine still changed what the band is running")
    report("POST /schedule refuses a bad routine and keeps the old one", errs)

    if not push:
        return
    # Round trip a real routine, then put back whatever was there before.
    sent = [{"id": "contract_test", "label": "Contract test", "hour": 6, "minute": 5}]
    status, _, reply, _ = request(base, "POST", "/schedule", body=json.dumps({"items": sent}))
    _, _, after, _ = get_json(base, "/schedule")
    errs = [] if status == 200 else [f"status {status}: {reply}"]
    errs += validate_schedule(after)
    if [i["id"] for i in after.get("items", [])] != ["contract_test"]:
        errs.append(f"the band is running {[i.get('id') for i in after.get('items', [])]}, "
                    "not what was pushed")
    if after.get("source") != "dashboard":
        errs.append(f"source={after.get('source')!r} after a push, want 'dashboard'")
    report("POST /schedule replaces the routine on the band", errs)

    status, _, reply, _ = request(base, "POST", "/schedule", body=json.dumps(body))
    _, _, restored, _ = get_json(base, "/schedule")
    report("the previous routine was put back",
           ([] if status == 200 else [f"status {status}: {reply}"]) +
           ([] if json.dumps(restored.get("items")) == before else
            ["the band is NOT running what it was before this test - push it again "
             "from the dashboard"]))
    SCHEDULE_IDS = {item["id"] for item in restored.get("items", [])}


def check_scan(base):
    """GET /scan, the measurement tools/fingerprint_trainer.py is built on."""
    status, headers, body, secs = request(base, "GET", "/scan", timeout=25)
    errs = [] if status == 200 else [f"status {status}"]
    errs += cors_errs(headers)
    try:
        aps = json.loads(body)
    except ValueError:
        report("GET /scan matches contract", errs + [f"body is not JSON: {body[:80]!r}"])
        return
    errs += validate_scan(aps)
    heard = len(aps.get("aps", []))
    if not heard:
        errs.append("the band heard no access points, so no room table can be trained")
    report(f"GET /scan matches contract ({heard} access points in {secs:.1f} s)", errs)


def check_cors_preflight(base):
    errs = []
    for path in ("/state", "/events", "/schedule", "/scan", "/demo/fire", "/ack", "/silence", "/time"):
        status, headers, _, _ = request(base, "OPTIONS", path)
        if status not in (200, 204):
            errs.append(f"OPTIONS {path} -> {status}")
        errs += [f"OPTIONS {path}: {e}" for e in cors_errs(headers)]
        if "POST" not in (headers.get("Access-Control-Allow-Methods") or ""):
            errs.append(f"OPTIONS {path}: Allow-Methods lacks POST")
    report("CORS preflight on every endpoint", errs)


def check_errors(base):
    errs = []
    status, headers, body, _ = request(base, "GET", "/nope")
    if status != 404:
        errs.append(f"GET /nope -> {status}, want 404")
    errs += cors_errs(headers)
    for path, want in (("/demo/fire", 400), ("/demo/fire?id=not_a_prompt", 404),
                       ("/time", 400), ("/schedule", 400)):
        status, _, body, _ = request(base, "POST", path)
        if status != want:
            errs.append(f"POST {path} -> {status}, want {want}")
        try:
            json.loads(body)
        except ValueError:
            errs.append(f"POST {path} body is not JSON: {body!r}")
    status, _, _, _ = request(base, "GET", "/demo/fire?id=lunch_checkin")
    if status == 200:
        errs.append("GET /demo/fire fired a prompt; should be POST-only")
    # /ack with nothing pending is a conflict, not a success.
    status, _, _, _ = request(base, "POST", "/ack")
    if status not in (200, 409):
        errs.append(f"POST /ack -> {status}, want 200 or 409")
    report("error responses (404/400, JSON bodies, GET can't fire)", errs)


def check_latency(base, n=20):
    times = []
    for _ in range(n):
        times.append(request(base, "GET", "/state")[3] * 1000)
    p95 = sorted(times)[int(n * 0.95) - 1]
    print(f"     /state latency ms: median {statistics.median(times):.0f}, p95 {p95:.0f}, max {max(times):.0f}")
    report("/state p95 latency < 1000 ms (dashboard polls every 3 s)",
           [] if p95 < 1000 else [f"p95 {p95:.0f} ms: loop is blocking or WiFi is weak"])


def check_set_time(base):
    now = int(time.time())
    status, _, _, _ = request(base, "POST", f"/time?epoch={now}")
    _, _, s, _ = get_json(base, "/state")
    report("POST /time sets the device clock",
           ([] if status == 200 else [f"status {status}"]) +
           ([] if abs(s["ts"] - now) <= 3 else [f"device ts {s['ts']} vs sent {now}"]))


def check_fire(base, prompt_id, since):
    status, _, _, _ = request(base, "POST", f"/demo/fire?id={prompt_id}")
    _, _, s, _ = get_json(base, "/state")
    _, _, body, _ = get_json(base, f"/events?since={since}")
    fired = [e for e in body["events"] if e["type"] == "prompt_fired" and e["detail"] == prompt_id]
    errs = [] if status == 200 else [f"status {status}"]
    if s.get("pendingPrompt") != prompt_id:
        errs.append(f"pendingPrompt={s.get('pendingPrompt')!r}, want {prompt_id!r}")
    if not fired:
        errs.append("no prompt_fired event appeared")
    report(f"POST /demo/fire?id={prompt_id} -> pending + prompt_fired (did it buzz?)", errs)

    print("     Shake the device 3x now to ack (waiting up to 70 s)...")
    last, deadline, outcome = since, time.time() + 70, None
    while time.time() < deadline and not outcome:
        for e in body["events"]:
            last = e["id"]
            if e["detail"] == prompt_id and e["type"] in ("prompt_acked", "prompt_missed"):
                outcome = e["type"]
        if not outcome:
            time.sleep(1)
            _, _, body, _ = get_json(base, f"/events?since={last}")
    print(f"     outcome: {outcome}")
    report("prompt resolves within the ack window",
           [] if outcome else ["neither prompt_acked nor prompt_missed after 70 s"])
    return last


def soak(base, minutes):
    """Poll exactly like the dashboard does and watch for resets, stalls, and errors."""
    print(f"     soaking for {minutes} min (Ctrl-C to stop early)...")
    end = time.time() + minutes * 60
    last_id, last_ts, fails, worst, polls, errs = 0, 0, 0, 0.0, 0, []
    try:
        while time.time() < end:
            try:
                _, _, s, t1 = get_json(base, "/state")
                _, _, body, t2 = get_json(base, f"/events?since={last_id}")
                polls += 1
                worst = max(worst, t1, t2)
                errs += validate_state(s, SCHEDULE_IDS) + validate_events(body, last_id, SCHEDULE_IDS)
                if s["ts"] + 5 < last_ts:
                    errs.append(f"device clock went backwards {last_ts} -> {s['ts']} (reboot?)")
                last_ts = s["ts"]
                for e in body["events"]:
                    print(f"     event {e['id']:>4} {e['type']:<14} {e['detail']}")
                    last_id = e["id"]
                # A reboot resets event ids: an empty since= answer while ids
                # restarted below last_id is otherwise invisible. Every ~30 s.
                if polls % 10:
                    time.sleep(3)
                    continue
                _, _, full, _ = get_json(base, "/events")
                if full["events"] and full["events"][-1]["id"] < last_id:
                    errs.append(f"event ids reset ({last_id} -> {full['events'][-1]['id']}): device rebooted")
                    last_id = full["events"][-1]["id"]
            except (urllib.error.URLError, TimeoutError, ConnectionError, ValueError) as e:
                fails += 1
                print(f"     poll failed: {e}")
            time.sleep(3)
    except KeyboardInterrupt:
        pass
    print(f"     {polls} polls, {fails} failures, worst response {worst * 1000:.0f} ms")
    # The dashboard falls back to replay mode after 3 consecutive failures;
    # a few scattered ones are tolerable, many are not.
    if polls and fails / (polls + fails) > 0.02:
        errs.append(f"{fails} failed polls (> 2%)")
    report(f"soak {minutes} min: no reboots, no contract drift, <2% failed polls", errs[:10])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("host", help="device IP (printed on serial at boot)")
    ap.add_argument("--fire", nargs="?", const="lunch_checkin", metavar="PROMPT_ID",
                    help="also force a prompt (default: lunch_checkin) and wait for ack/miss")
    ap.add_argument("--set-time", action="store_true", help="also POST /time with this machine's clock")
    ap.add_argument("--soak", type=float, metavar="MIN", help="also poll like the dashboard for MIN minutes")
    ap.add_argument("--schedule", action="store_true",
                    help="also push a routine to the band and restore the current one")
    a = ap.parse_args()
    base = "http://" + a.host.removeprefix("http://").rstrip("/")

    try:
        request(base, "GET", "/state", timeout=5)
    except Exception as e:  # noqa: BLE001
        print(f"FAIL cannot reach {base}: {e}\n"
              "     Same network? Venue WiFi client isolation? Try a phone hotspot.")
        return 1

    if a.set_time:
        check_set_time(base)
    check_schedule(base, push=a.schedule)
    check_state(base)
    last = check_events(base)
    check_scan(base)
    check_cors_preflight(base)
    check_errors(base)
    check_latency(base)
    if a.fire:
        last = check_fire(base, a.fire, last)
    if a.soak:
        soak(base, a.soak)

    print(f"\n{'ALL PASSED' if not failures else f'{failures} CHECK(S) FAILED'}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
