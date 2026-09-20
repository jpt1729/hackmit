#!/usr/bin/env python3
"""Learn the room table for firmware/src/config.h by walking the band around. Stdlib only.

    python3 tools/fingerprint_trainer.py 192.168.1.42
    python3 tools/fingerprint_trainer.py 192.168.1.42 --rooms kitchen bedroom living
    python3 tools/fingerprint_trainer.py 192.168.1.42 --samples 12 --write

The band guesses which room it is in by comparing the WiFi it can hear against
a reference table. Shipping the placeholder table means every room reads as
"unknown", and a reminder that waits for the kitchen never fires.

The measurement has to come from the band's own antenna, through GET /scan:
a laptop standing in the same doorway hears a different world, and a table
trained on the laptop makes the band confidently wrong.

Carry the band into each room, keep it on the wrist, and hold still while it
samples - that is the posture it will be in when it has to guess.
"""
import argparse
import json
import os
import statistics
import sys
import urllib.error
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from contract import ROOMS, ROOT, validate_scan  # noqa: E402

# firmware/src/config.h: RoomFP.aps is fixed at 8 entries.
MAX_APS = 8
ROOM_ENUM = {"kitchen": "KITCHEN", "bedroom": "BEDROOM", "living": "LIVING"}


def scan(base, timeout=20.0):
    """One WiFi scan from the band. The device blocks for ~2 s while it listens."""
    request = urllib.request.Request(base + "/scan", method="GET")
    with urllib.request.urlopen(request, timeout=timeout) as response:
        body = json.loads(response.read().decode())
    problems = validate_scan(body)
    if problems:
        raise ValueError("; ".join(problems))
    return {ap["bssid"].upper(): ap["rssi"] for ap in body["aps"]}


def sample_room(base, room, samples):
    """Median RSSI per access point, plus how often each one was heard at all."""
    readings = {}
    for index in range(samples):
        try:
            seen = scan(base)
        except (urllib.error.URLError, OSError, ValueError) as error:
            print(f"     scan {index + 1}/{samples} failed: {error}")
            continue
        for bssid, rssi in seen.items():
            readings.setdefault(bssid, []).append(rssi)
        strongest = max(seen.values()) if seen else None
        print(f"     scan {index + 1}/{samples}: heard {len(seen)} APs"
              + (f", strongest {strongest} dBm" if strongest is not None else ""))
    if not readings:
        return None
    return {
        bssid: {"rssi": round(statistics.median(values)), "seen": len(values), "of": samples}
        for bssid, values in readings.items()
    }


def choose_reference_aps(rooms):
    """Pick the APs the table will be built from.

    An access point only tells the rooms apart if the band can hear it in all of
    them - one that appears in a single room looks identical to one that is
    simply out of range, because the firmware scores a missing AP as -100 dBm.
    So: everywhere-audible APs first, widest spread between rooms first within
    that, and strong-but-partial APs only to fill the remaining slots.
    """
    everywhere, partial = [], []
    all_bssids = {bssid for readings in rooms.values() for bssid in readings}
    for bssid in all_bssids:
        levels = [readings[bssid]["rssi"] for readings in rooms.values() if bssid in readings]
        reliable = all(
            bssid in readings and readings[bssid]["seen"] * 2 >= readings[bssid]["of"]
            for readings in rooms.values()
        )
        spread = max(levels) - min(levels)
        (everywhere if reliable else partial).append((bssid, spread, max(levels)))

    everywhere.sort(key=lambda row: (-row[1], -row[2]))
    partial.sort(key=lambda row: -row[2])
    chosen = [row[0] for row in everywhere] + [row[0] for row in partial]
    return chosen[:MAX_APS], len(everywhere)


def render_table(rooms, reference):
    """The ROOM_FPS block, ready to paste into config.h."""
    lines = ["// Trained with tools/fingerprint_trainer.py. Retrain after moving a router,",
             "// or in a new venue: these are measurements of one building, not settings.",
             "static const RoomFP ROOM_FPS[] = {"]
    for room, readings in rooms.items():
        entries = []
        for bssid in reference:
            # -100 is the firmware's own stand-in for "not heard", so a room
            # that cannot hear a reference AP still scores against it honestly.
            rssi = readings[bssid]["rssi"] if bssid in readings else -100
            entries.append(f'{{"{bssid}", {rssi}}}')
        lines.append(f"  {{ {ROOM_ENUM[room]}, {len(entries)}, {{ " + ", ".join(entries) + " } },")
    lines.append("};")
    lines.append("#define NUM_ROOM_FPS (sizeof(ROOM_FPS) / sizeof(ROOM_FPS[0]))")
    return "\n".join(lines)


def write_table(table):
    """Replace the ROOM_FPS block in config.h, leaving the rest of the file alone."""
    path = ROOT / "firmware/src/config.h"
    source = path.read_text()
    start = source.find("static const RoomFP ROOM_FPS[]")
    if start < 0:
        raise SystemExit(f"could not find the ROOM_FPS block in {path}")
    # Take the preceding comment block with it, so stale notes do not survive.
    while True:
        previous = source.rfind("\n", 0, start - 1)
        if previous < 0 or not source[previous + 1:start].lstrip().startswith("//"):
            break
        start = previous + 1
    marker = "#define NUM_ROOM_FPS"
    end = source.find("\n", source.find(marker, start)) + 1
    path.write_text(source[:start] + table + "\n" + source[end:])
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("host", help="device IP, as printed on serial at boot")
    parser.add_argument("--rooms", nargs="+", default=["kitchen", "bedroom"],
                        choices=sorted(ROOM_ENUM), help="rooms to train, in walking order")
    parser.add_argument("--samples", type=int, default=8,
                        help="scans per room (default 8; more is steadier, ~3 s each)")
    parser.add_argument("--write", action="store_true",
                        help="patch firmware/src/config.h instead of only printing the table")
    args = parser.parse_args()

    base = "http://" + args.host.removeprefix("http://").rstrip("/")
    assert set(args.rooms) <= ROOMS, "argparse should have caught an unknown room"

    rooms = {}
    for room in args.rooms:
        print(f"\n== {room} ==")
        try:
            input(f"   Put the band on and stand in the {room}, then press Enter. ")
        except EOFError:
            print("   (not a terminal: sampling immediately)")
        readings = sample_room(base, room, args.samples)
        if not readings:
            print(f"FAIL no scan succeeded in the {room}. Is the band still on the network?")
            return 1
        rooms[room] = readings
        print(f"     {len(readings)} access points heard in the {room}")

    reference, stable = choose_reference_aps(rooms)
    if not reference:
        print("\nFAIL no access points were heard anywhere. Room tracking needs some WiFi around.")
        return 1

    table = render_table(rooms, reference)
    print("\n" + table)

    print(f"\n     {len(reference)} reference APs, {min(stable, len(reference))} of them audible in every room.")
    if stable < 2:
        # Without shared APs the scores are driven by which router is missing,
        # which flips on a closed door rather than on a change of room.
        print("     WARNING: fewer than 2 access points are audible in every room, so the\n"
              "     room guess will be unreliable. A second router or hotspot left in the\n"
              "     far room fixes this better than any amount of extra sampling.")

    # Rooms that measure the same cannot be told apart, however good the scan.
    names = list(rooms)
    for i, first in enumerate(names):
        for second in names[i + 1:]:
            gaps = [abs(rooms[first].get(b, {"rssi": -100})["rssi"]
                        - rooms[second].get(b, {"rssi": -100})["rssi"]) for b in reference]
            if max(gaps) < 8:
                print(f"     WARNING: {first} and {second} look almost identical "
                      f"(largest difference {max(gaps)} dBm). The band will confuse them.")

    if args.write:
        path = write_table(table)
        print(f"\nPASS wrote the table into {path}. Reflash, then check GET /state shows the room.")
    else:
        print("\n     Paste the block above over ROOM_FPS in firmware/src/config.h, "
              "or rerun with --write.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
