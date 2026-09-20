# Brain Buddy

**A $30 wristband that helps someone with dementia keep their own routine, and tells one caregiver when it didn't work. It runs entirely on the home WiFi — no account, no cloud, no subscription, and nothing about the person leaves the house.**

The products sold to families for this cost $30–$50 every month, need their own phone plan, and stream a person's location to someone else's server. I wanted the opposite. Brain Buddy costs about one month of that, once. The website talks to the band over the home network and to nothing else.

And the device is built to ask the person to do something, not to report on them. That distinction runs all the way down into the firmware. The buzzer duty cycle is capped so it chimes instead of alarming. The band nudges someone wandering at 3am exactly once, because waking them fully is worse than the wander. When it decides the person has left the safe area, it shows them the direction home before it tells anyone else. I would rather ship a device someone is willing to keep wearing than one that is technically more informative.

## Why it matters

Forgetting medication is not the thing that puts people in care. Losing the ability to run your own day is. The gap Brain Buddy sits in is small and specific:

- **A reminder that arrives where it can be acted on.** A 9am alarm for pills that are in the kitchen is useless in the bedroom. Brain Buddy holds that reminder until the band's room estimate says kitchen, or until 30 minutes have passed. It also holds the whole routine while the person is out of the house, because a reminder to eat lunch during a walk just teaches someone to ignore the buzzing.
- **Proof it happened, not a guess.** A reminder only counts as done when the person shakes their wrist. No shake inside the response window means the caregiver sees "not acknowledged" — an honest answer rather than a notification that was technically delivered.
- **The caregiver sets the routine, and the wrist follows.** Activities added on the website are pushed to the band and kept there through a flat battery. One person's day, edited in one place.
- **One caregiver, one page, no monitoring room.** Today's routine, the notices that need attention, and where the band is. That is the whole surface area.
- **Voice, both ways.** Whoever is caring can leave a spoken message that plays on the website, and the person can answer in their own voice. Reading gets hard before speaking does.

## What it does

| | |
|---|---|
| **Reminders** | Times a caregiver sets on the website. The band chimes, shows the words on its screen, and waits 60 seconds for a shake. It re-chimes once, then records a miss. |
| **Acknowledgement** | A shake of the wrist. A caregiver standing in the room can also tick it off from the website. |
| **Room estimate** | Which room the band is in, from WiFi signal strength. Used to hold a reminder until it is useful. It is an estimate, and the site shows its confidence rather than pretending otherwise. |
| **On the wrist or not** | Inferred from the IMU: a worn device always shows micro-motion, a device on a table is dead still. There is no skin electrode in the parts list. |
| **Away from home** | A GPS geofence. The band chimes and shows the distance and compass direction home; the caregiver gets a notice they can act on. |
| **Night wandering** | Movement between midnight and 5am. One quiet nudge on the wrist, one notice on the website. |
| **Voice messages** | Recorded in the browser, stored locally in SQLite, with optional speech-to-text the sender can correct before sending. |

## Run it

```sh
python3 tools/message_server.py
```

Open [Brain Buddy](http://127.0.0.1:8787/). The app is a caregiver dashboard with **Messages / Routine / Wristband / Location / Updates** navigation. To connect a bracelet on your local network, open `http://127.0.0.1:8787/?device=ESP32_IP`.

With no band on the network the site replays `app/data/demo.json`, a real session recorded off the hardware with `tools/capture_demo.py`. That is also what the published site serves, because it is HTTPS and browsers will not let an HTTPS page talk to a device at `http://192.168.x.x`. The split is not a workaround I am apologising for — it is the "no cloud" claim being literally true.

The firmware:

```sh
cd firmware && pio run -t upload && pio device monitor   # the band prints its IP at boot
```

Two things are measurements rather than settings and have to be retaken in a new building:

```sh
python3 tools/fingerprint_trainer.py <band-ip> --rooms kitchen bedroom --write   # room table
python3 tools/test_device_http.py <band-ip> --set-time --fire                    # end-to-end check
```

Venue WiFi often has client isolation, which stops the laptop and the band from seeing each other on the same SSID. A phone hotspot fixes it. Budget twenty minutes for this.

## Checks

```sh
python3 tools/contract.py          # the JSON contract, no device needed
cd firmware && pio test -e native  # 132 logic tests on your laptop, ~6 s
pio run                            # all four build configurations compile
```

`contract.py` checks the recorded demo data, that the band's fallback routine is one the website knows how to draw, and that the routine the website would push is a body the firmware accepts. The reminder loop — chime, shake, website — has no build flag that removes it; the room estimate, wear estimate and GPS each do.

## Repository

| Folder | Contents |
| --- | --- |
| [app/](app/) | Website pages, styles, JavaScript, icons, demo data, and service worker |
| [docs/](docs/README.md) | Build specification, website guide, deployment instructions, and documentation images |
| [firmware/](firmware/) | ESP32 firmware and tests |
| [hardware/](hardware/) | Wiring and bill of materials |
| [tools/](tools/) | Local message server, contract checks, and device utilities |

- [Website guide](docs/app.md): dashboard, messaging, routines, and GPS integration status
- [Build specification](docs/spec.md): architecture, firmware modules, and the JSON contract
- [Deployment](docs/deployment.md): local server and GitHub Pages
- [Hardware wiring](hardware/wiring.md) and [bill of materials](hardware/bom.md)
- [Firmware tests](firmware/test/README.md)

## Hosting

The Pages workflow publishes `app/` on pushes to `main`. GitHub Pages must use **GitHub Actions** as its publishing source; until that setting is changed, the root [`index.html`](index.html) forwards the site's front door to `app/`. Pages hosts the static site only — voice messaging needs the Python server, and a bracelet on a home network is never reachable from it. See the [deployment guide](docs/deployment.md).

## What I know is missing

- **The event log does not survive a reboot.** 200 events in RAM. The routine does persist, in NVS.
- **The room estimate is a guess.** It needs retraining whenever a router moves, and two rooms that hear the same access points at the same strength cannot be told apart — the trainer says so out loud rather than producing a table that quietly does not work.
- **Nothing is authenticated.** Anyone on the home WiFi can read the band and push a routine to it, and anyone who can reach the message server can send as either person. That is a deliberate trade for a device with no account and no cloud, and it is the first thing I would change before this went into a real house.
- **Weekly progress lives in one browser.** It is per-device history, not a record, and a blank day means no data rather than a missed day.

Next, in order: sign the website-to-band connection, persist the event log to flash, and let a caregiver set a reminder's room from the website instead of only its time — the firmware already accepts the field, the editor just does not offer it yet.

<img width="589" height="596" alt="image" src="https://github.com/user-attachments/assets/0c3d0fb3-ee6e-46cd-a9c7-e55984930c26" />
