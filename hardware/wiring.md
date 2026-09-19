# Hardware wiring notes

- ESP32 DevKit board
- MPU-6050 accelerometer on I2C (GPIO 21/22)
- Vibration motor driven through NPN transistor and flyback diode
- Electrode pad connected to ADC input with divider and bias network
- WiFi scans performed with the ESP32 onboard radio

## Example pin map
- GPIO 21 / 22: I2C bus for accelerometer
- GPIO 25: vibration motor driver
- GPIO 34: electrode ADC input
- 3V3/GND: power rails and divider network

This is a placeholder wiring guide for the initial repo scaffold.
