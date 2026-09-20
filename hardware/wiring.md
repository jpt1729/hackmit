# Hardware wiring

Everything is one ESP32 DevKit. No second board, no hub.

![Wiring diagram](../docs/img/wiring.svg)

*Source: [`docs/img/wiring.svg`](../docs/img/wiring.svg) — editable text, so pin changes are a one-line edit.*

| Part | Connection | Pin (`firmware/src/config.h`) |
|---|---|---|
| MPU-6050 (accel + gyro) | I2C, addr `0x68` (AD0 low) | SDA `GPIO 21`, SCL `GPIO 22`, VCC 3V3, GND |
| UCTRONICS 0.96" OLED (SSD1306 128x64) | same I2C bus, addr `0x3C` | SDA `GPIO 21`, SCL `GPIO 22`, VCC 3V3, GND |
| GT-U7 GPS receiver | UART2, 9600 baud | GPS **TX** → ESP32 `GPIO 16` (RX2), GPS RX → `GPIO 17` (optional), VCC 5V (VIN), GND |
| WS2812B ring (12 px, 5050) | one data line | `GPIO 27` → 330 Ω → **DI**, 5V (VIN), GND |
| Piezo buzzer | PWM tone (LEDC) | `GPIO 26` → piezo → 100 Ω → GND |

## Notes that will bite you

- **Common ground.** The ring and the GPS run off 5V/VIN while the ESP32 logic is 3V3. All grounds must be the same net. A ring with power but no shared ground has no reference for the data line and stays *completely dark* — it does not flicker, so a dead-still ring is the symptom to look for, not a flickering one. The GPS emits garbage. This applies double to a bench supply: its ground has to come back to the ESP32 too.
- **DI, not DO.** The ring's two data pads are silkscreened `DI` and `DO`, which in that
  small a font read as **D1** and **D0**. Data goes *into* `DI`; `DO` is the chain output for a
  second ring and drives nothing. On `DO` the ring is completely dark with power, ground and
  pin all correct — there is no partial failure to tip you off. The arrow between the pads
  points the way the data travels: feed the pad it points away from.
- **3.3 V data into a 5 V ring is marginal.** A WS2812B wants logic-1 above 0.7 × VDD — 3.5 V
  on a 5 V supply — and the ESP32 only drives 3.3 V. Many rings accept it, some never do, and
  raising the supply makes it worse (at 5.5 V the threshold is 3.85 V, and 5.5 V is over the
  part's 5.3 V maximum anyway). VDD range is 3.5–5.3 V, so dropping to 3V3 is not the way out
  either. If the ring stays dark on a known-good `DI`, in order: put one signal diode in series
  with the ring's 5 V feed (VDD ≈ 4.3 V, threshold ≈ 3.0 V), or drive `GPIO 27` open-drain with
  a 330 Ω–4.7 kΩ pull-up from `DI` to +5 V, or use a proper level shifter.
- **GPS TX, not RX.** The GT-U7's labels are from the module's point of view: its TX goes to the ESP32's RX. Swapping them gives a silent UART, which `test_gps_module_is_talking` reports as "nothing on the GPS UART".
- **GPS needs sky.** Indoors it may never fix; a cold start at a window takes 1-2 minutes. The firmware runs fine without a fix — no geofence, prompts still fire.
- **GPIO 16/17** are free on a plain ESP32 DevKit. On modules with PSRAM (ESP32-WROVER) they are not: move the UART pins if you are on a WROVER.
- **Ring brightness** is capped in firmware (`LED_BRIGHTNESS 40`). At full white, 12 pixels draw ~700 mA, more than a USB port will give you — and this is a device someone wears at night.
- **Piezo type.** A passive piezo (bare disc, or a 2-pin module with no oscillator) plays the tone patterns. An active buzzer only does on/off: set `BUZZER_PASSIVE 0` in `config.h` and the same patterns play as rhythm at its fixed pitch.
- **Strapping pins.** Avoid GPIO 0, 2, 12, 15 for anything new: the board may refuse to boot with something pulling on them.

## Power

USB is fine for the demo. On a battery the GPS is the biggest continuous draw (~30 mA)
and the ring is the spikiest. Nothing here sleeps: `loop()` runs continuously.

**Check VIN is actually live before blaming anything else.** On many ESP32 DevKits USB 5V is
back-fed to the VIN pin, so "5V (VIN)" in the table above just works. On some it is not: VIN
feeds the regulator but nothing feeds VIN, and it sits at 0 V with USB plugged in. The ring and
the GPS then have no supply at all, which looks exactly like a wiring mistake. Measure VIN to
GND once on a new board — it should read ~4.7 V on USB — and if it is dead, take 5 V from a
pin that is, or power the board from the VIN side.

## What is not in this build

No vibration motor and no electrode pad. The wearer's feedback is the piezo, the ring
and the OLED; "is it being worn" is inferred from IMU micro-motion instead of skin
contact (see `firmware/src/wear.cpp`).
