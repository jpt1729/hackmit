# Brain Buddy

A wearable companion and accessible website for daily routines, caregiver voice messages, and bracelet status.

## Run locally

From the repository root, run:

```sh
python3 tools/message_server.py
```

Open [Brain Buddy](http://127.0.0.1:8787/). The app is a caregiver dashboard with **Message / Routine / Map / Notices** navigation. To connect a bracelet on your local network, open `http://127.0.0.1:8787/?device=ESP32_IP`.

Python 3 is the only server dependency. The website uses plain HTML, CSS, and JavaScript, with no frontend build step. Voice recordings and transcripts are stored in `var/voice_messages.sqlite3`; routine edits are saved in the browser.

## Repository layout

| Folder | Contents |
| --- | --- |
| [app/](app/) | Website pages, styles, JavaScript, icons, demo data, and service worker |
| [docs/](docs/README.md) | Build specification, website guide, deployment instructions, and documentation images |
| [firmware/](firmware/) | ESP32 firmware and tests |
| [hardware/](hardware/) | Wiring and bill of materials |
| [tools/](tools/) | Local message server, contract checks, and device utilities |

## Documentation

- [Website guide](docs/app.md): caregiver dashboard, messaging, routines, and GPS integration status
- [Build specification](docs/spec.md): architecture, firmware modules, and JSON contract
- [Deployment](docs/deployment.md): local server and GitHub Pages
- [Hardware wiring](hardware/wiring.md) and [bill of materials](hardware/bom.md)
- [Firmware tests](firmware/test/README.md)

## Checks

```sh
python3 tools/contract.py
```

This checks the recorded data contract and compares firmware schedule IDs with the website defaults. The schedules currently differ; that check reports the differences until the firmware and dashboard schedules are aligned.

## Hosting

The Pages workflow publishes `app/` on pushes to `main`. GitHub Pages must use **GitHub Actions** as its publishing source. Pages hosts the static website; voice messaging requires the Python server. See the [deployment guide](docs/deployment.md).
