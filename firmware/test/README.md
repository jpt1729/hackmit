# Testing the firmware

There are four layers. Run them from the top down. Each one needs more hardware than the one before it.

| Layer | Command (from `firmware/`) | Needs | Catches |
|---|---|---|---|
| 1. Logic | `pio test -e native` | nothing (runs on laptop, ~5 s) | prompt gating, ack/rebuzz/miss, shake detection, wander, RSSI matching, wear debounce, event log + JSON |
| 2. Builds | `pio run` (all envs) | nothing | ESP32 compile errors in the full build and in the cut builds (`ENABLE_WEAR=0`, `ENABLE_LOCATION=0`), plus RAM/flash use |
| 3. Hardware | `pio test -e esp32dev` | ESP32 on USB, wired | IMU wiring + WHO_AM_I, OLED responds + shows messages (you watch it), 1 g at rest, electrode ADC, motor (you feel it), WiFi scan sees the fingerprint APs, WiFi join, NTP, loop never blocks >50 ms, heap headroom, no leak building `/events` |
| 4. Network | `python3 ../tools/test_device_http.py <ip> [--fire] [--set-time] [--soak 30]` | flashed device on the demo network | JSON contract, CORS, error codes, latency, prompt→shake→ack end to end, reboots or dropouts over a long soak |

Plus one static check that needs no device:
`python3 ../tools/contract.py` checks that `config.h` schedule ids match `docs/js/checklist.js`, and that `docs/data/demo.json` follows the contract.

## How the native tests work

`test/mocks/` holds fake `Arduino.h`, `Wire.h` (with a fake MPU-6050), and `WiFi.h` (a fake async scanner). The real `src/*.cpp` files compile against these fakes. `main.cpp` and `server.cpp` are left out. Tests move time forward with `mock::advance(ms)`, which moves both `millis()` and `time()`. The wall clock is set with `mock::epoch = at(9, 0)`. Each module's `xxxInit()` resets that module's state, so every test starts clean.

## Pre-demo checklist (at the venue)

1. `pio test -e native` shows all green (after any threshold change in `config.h`).
2. Retrain the room fingerprints. Then `pio test -e esp32dev`: `test_wifi_scan_sees_fingerprint_aps` must pass.
3. Flash the firmware with `pio run -t upload`. Note the IP from the serial monitor.
4. Run `test_device_http.py <ip> --fire` from the dashboard laptop, on the hotspot. Shake when asked.
5. Run `test_device_http.py <ip> --soak 20` while you rehearse. It should report 0 reboots.

Note: `pio test -e esp32dev` flashes the test firmware. Run step 3 again afterwards to put the real firmware back.
