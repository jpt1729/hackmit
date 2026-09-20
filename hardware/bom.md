# Bill of materials

| Part | Qty | Approx. cost | What it does |
|---|---|---|---|
| ESP32 DevKit | 1 | $7 | everything: sensors, logic, WiFi, the dashboard's HTTP server |
| MPU-6050 6-axis IMU | 1 | $3 | activity, sleep, night wandering, shake-to-acknowledge, on-body estimate |
| GT-U7 GPS receiver (NEO-6M) | 1 | $8 | position, geofence around home, clock when NTP is blocked |
| UCTRONICS 0.96" OLED (SSD1306) | 1 | $4 | the prompt in words, clock, direction home, device IP |
| NeoPixel ring, mini (12 px) | 1 | $5 | ambient status: prompt waiting, alert, acknowledged, night glow |
| Piezo buzzer | 1 | $1 | the prompt chime and the away-from-home alert |
| Wires, resistors (330 Ω, 100 Ω), tape, enclosure | — | $2 | — |

Estimated total: **~$30**

The $15 figure in the pitch was for the vibration-motor build. This parts list
trades the motor and electrode for GPS, a screen and a light ring — still under
the price of one month of a commercial tracking service, which is the comparison
that matters on the slide.
