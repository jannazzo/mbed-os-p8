# Temperature-Controlled Fan Controller

An embedded system that automatically adjusts a PWM fan's speed based on ambient temperature, with a configurable target temperature, on-board LED indicators, and a live status display. Built on an ST Microelectronics Nucleo-G474RE using the Arm Mbed OS framework.

## Overview

The controller continuously reads temperature from a thermistor, compares it against a user-adjustable target, and drives a 4-pin PWM fan to close the gap. Three buttons let the user raise or lower the target temperature and toggle the system on/off. A 20x4 I²C character LCD shows the current temperature, target, and fan PWM duty cycle in real time. Two sets of three LEDs mirror the target temperature level and the active fan level for quick visual feedback.

## Features

- Thermistor-based ambient temperature sensing with Steinhart-style conversion to °C
- Three-level target temperature selection (27 °C / 29 °C / 31 °C)
- Four-level fan response (off, low, medium, high) based on the temperature differential from target
- 25 kHz PWM output suitable for standard 4-pin computer fans
- Software debouncing on all three buttons (200 ms window)
- System on/off toggle that forces the fan off and clears the fan indicator LEDs
- Two three-LED strips for at-a-glance status (target level and active fan level)
- 20x4 I²C character LCD display driven by a custom PCF8574 / HD44780 driver
- USB serial debug output (current temperature, target level, fan level, duty cycle)

## Hardware

| Component | Details |
|---|---|
| MCU board | STM32 Nucleo-G474RE |
| Temperature sensor | NTC thermistor in a voltage divider with a 9.85 kΩ bias resistor |
| Display | 20x4 character LCD with PCF8574 I²C backpack (default address `0x27`) |
| Fan | 4-pin PWM fan, driven at 25 kHz, externally powered |
| Buttons | 3x momentary pushbuttons (Up, Down, Status) |
| Indicators | 6x LEDs arranged as two 3-LED strips (with current-limiting resistors) |

All logic runs at 3.3 V. The LCD can be run at 3.3 V as well.

## Pinout

### Inputs
| Signal | Pin | Arduino header |
|---|---|---|
| Thermistor (analog) | `PA_1` | A1 |
| Up button | `PB_5` | D4 |
| Down button | `PA_10` | D2 |
| Status button | `PA_8` | D7 |

### Outputs
| Signal | Pin | Arduino header |
|---|---|---|
| Fan PWM | `PB_3` | D3 |
| Status LED | `PC_4` | — |
| Target LED – red | `PC_8` | — |
| Target LED – yellow | `PC_6` | — |
| Target LED – green | `PC_5` | — |
| Fan LED – red | `PA_12` | — |
| Fan LED – yellow | `PA_11` | — |
| Fan LED – green | `PB_12` | — |

### I²C (LCD)
| Signal | Pin | Arduino header |
|---|---|---|
| SDA | `D14` | SDA |
| SCL | `D15` | SCL |

## Hardware Diagram

<img width="1788" height="620" alt="LTspice_AaAI5mMH4a" src="https://github.com/user-attachments/assets/f71a57ff-b89d-418e-89df-98668b37b8a1" />


## Software Architecture

```
├── main.cpp           // Application logic, button handling, main control loop
├── lcd_driver.h       // I2CLCD class declaration + display helper signatures
└── lcd_driver.cpp     // I2CLCD implementation + updateLCDvalues()
```

The `I2CLCD` class wraps an HD44780 in 4-bit mode over a PCF8574 I/O expander. It handles initialization, cursor positioning, and padded writes. Everything needed to update the display is exposed through a single `updateLCDvalues(lcd, currentTemp, targetTemp, dutyCycle)` call.

Button presses are routed through Mbed's `EventQueue` so the handlers run in a dedicated thread rather than true ISR context — this keeps `printf` and other non-ISR-safe calls usable inside button handlers. Each handler debounces itself with a 200 ms static-local timestamp check.

## Control Behavior

The fan level is determined by how far the measured temperature is above the target:

| Temperature above target | Fan level | PWM duty cycle |
|---|---|---|
| ≤ 1 °C | 0 (off) | 5% |
| ≤ 4 °C | 1 (low) | 35% |
| ≤ 7 °C | 2 (medium) | 65% |
| > 7 °C | 3 (high) | 95% |

The zero-RPM fan mode is represented by a duty cycle of 5%, because this is too little to allow the fan to spin, and a 0% signal will cause the fan to default to 100%. When the system is toggled off via the status button, the fan is forced to 5% regardless of temperature, and the fan indicator LEDs are extinguished.

## Building and Flashing

This project is built against Mbed OS 6 and targets the `NUCLEO_G474RE`.

Using the Mbed CLI or Keil Studio Cloud, compile with the `ARMC6` toolchain. The resulting `.bin` is flashed by dragging it onto the Nucleo's mass-storage drive.

## Usage

1. Power the Nucleo via USB (the fan and LCD can be powered from 3.3 V pins on the board or an external supply as appropriate).
2. On boot, the LCD displays a splash message, then the live readout.
3. Use the Up and Down buttons to set the target temperature level.
4. The fan ramps up automatically when the measured temperature rises above the target.
5. Press the Status button to toggle the whole system on or off.

The USB serial port (default 9600 baud) outputs the current temperature, target level, fan level, and duty cycle once per second for debugging.

## Known Limitations

- The thermistor linear-approximation constants are hard-coded for one specific NTC part. Replacing the thermistor will require recalibration.
- The three target temperature levels (27/29/31 °C) are hard-coded; continuous adjustment is not supported in this version.
