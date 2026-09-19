# Testing the firmware

There are four layers. Run them from the top down. Each one needs more hardware than the one before it.

| Layer | Command (from `firmware/`) | Needs | Catches |
|---|---|---|---|
| 1. Logic | `pio test -e native` | nothing (runs on laptop, ~6 s) | prompt gating, ack/rebuzz/miss, shake detection, wander, RSSI matching, wear inference, NMEA parsing + geofence + GPS clock, buzzer patterns, LED modes, safety chimes, event log + JSON |
| 2. Builds | `pio run` (all envs) | nothing | ESP32 compile errors in the full build and in the cut builds (`ENABLE_WEAR=0`, `ENABLE_LOCATION=0`, `ENABLE_GPS=0`), plus RAM/flash use |
| 3. Hardware | `pio test -e esp32dev` | ESP32 on USB, wired | IMU wiring + WHO_AM_I + noise floor, OLED responds + shows messages, 1 g at rest, piezo (you hear it), LED ring (you watch it), GPS UART + fix + clock, WiFi scan sees the fingerprint APs, WiFi join, NTP, loop never blocks >50 ms, heap headroom, no leak building `/events` |
| 4. Network | `python3 ../tools/test_device_http.py <ip> [--fire] [--set-time] [--soak 30]` | flashed device on the demo network | JSON contract, CORS, error codes, latency, prompt→shake→ack end to end, reboots or dropouts over a long soak |

Plus one static check that needs no device:
`python3 ../tools/contract.py` checks that `config.h` schedule ids match `docs/js/checklist.js`, and that `docs/data/demo.json` follows the contract.

## How the native tests work

`test/mocks/` holds fake `Arduino.h` (millis, the clock, ADC, LEDC tones, and `Serial2` as a fake GPS UART), `Wire.h` (with a fake MPU-6050), and `WiFi.h` (a fake async scanner). The real `src/*.cpp` files compile against these fakes. `main.cpp` and `server.cpp` are left out, and the OLED and NeoPixel ring are compiled out (`ENABLE_DISPLAY=0`, `ENABLE_LEDS=0`) because their libraries are hardware-only — what the tests cover is the logic that decides what they show. The GPS tests build their own NMEA sentences, checksums included, and push them through `Serial2` exactly as the module would receive them. Tests move time forward with `mock::advance(ms)`, which moves both `millis()` and `time()`. The wall clock is set with `mock::epoch = at(9, 0)`. Each module's `xxxInit()` resets that module's state, so every test starts clean.

## Pre-demo checklist (at the venue)

1. `pio test -e native` shows all green (after any threshold change in `config.h`).
2. Retrain the room fingerprints. Then `pio test -e esp32dev`: `test_wifi_scan_sees_fingerprint_aps` must pass.
2b. Set `HOME_LAT` / `HOME_LON` from a real fix at the venue (`test_gps_gets_a_fix` prints them, or read `GET /state`), then walk past the fence once and check `geofence_exit` lands in `/events`.
3. Flash the firmware with `pio run -t upload`. Note the IP from the serial monitor.
4. Run `test_device_http.py <ip> --fire` from the dashboard laptop, on the hotspot. Shake when asked.
5. Run `test_device_http.py <ip> --soak 20` while you rehearse. It should report 0 reboots.

Note: `pio test -e esp32dev` flashes the test firmware. Run step 3 again afterwards to put the real firmware back.
