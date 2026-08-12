# Uno R4 WiFi CoreXY Drawbot

This is a clean firmware rewrite for the 400 × 320 mm CoreXY drawing machine. It targets an Arduino Uno R4 WiFi with a Protoneer CNC Shield V3.00, two A4988 drivers, two normally-open home switches, and a scotch-yoke pen servo. It does not copy the old firmware.

Both A4988s have software-controlled MS1/MS2/MS3 inputs. Positions are always tracked in 1/16-step base units; each segment uses the coarsest compatible resolution from full through 1/16 without losing absolute position. Long straight drawing segments prefer coarse modes, while short curve/detail segments prefer fine modes.

Version 1 receives absolute, millimetre G-code over USB and BLE. The firmware advertises as "Drawbot" over Bluetooth LE using the standard Nordic UART Service (NUS). Pair from your Mac's Bluetooth settings or scan with `stream_gcode.py --ble`.

## Status: COMMISSIONED (2026-08-01)

- **The machine draws.** Verified end to end: 100 mm square, then a 35-minute
  endurance run (full-workspace hatching) with zero BLE drops or errors.
- **Pen is calibrated** (`PEN_CALIBRATED = true`): down = 500 µs (tip on
  paper), up = 1500 µs.
- **Scale verified**: `BASE_UNITS_PER_MM = 80.8` measured with a ruler.
- **Manual homing is the operating mode**: the bottom (Y−) limit switch is
  defective and never triggers, so `G28` will not complete. Park the carriage
  at the home corner, then send `M50 X0 Y0`.
- **Presentation workflow**: park at the corner, run
  `examples/stress_30min.gcode` (or any job) with
  `tools/stream_gcode.py --ble …`; the streamer auto-recovers from BLE drops
  with position reconciliation. Generate new hatch jobs with
  `tools/gen_stress_pattern.py --paper WxH`.

## Power and safety

Keep the 24→5 V buck converter. The Uno R4 must not power the servo, and 24 V
must not be connected to the Arduino 5 V rail. USB and 24 V must never be
applied at the same time: uploads are USB-only, motion is 24 V-only. Follow
[docs/WIRING.md](docs/WIRING.md) before applying power and
[docs/COMMISSIONING.md](docs/COMMISSIONING.md) before running `G28`.

## Build and upload

Install [Arduino CLI](https://arduino.github.io/arduino-cli/latest/installation/) or use Arduino IDE 2. Then install the official board core and required libraries:

```sh
arduino-cli core update-index
arduino-cli core install arduino:renesas_uno
arduino-cli lib install Servo
arduino-cli lib install ArduinoBLE
arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi .
arduino-cli upload --fqbn arduino:renesas_uno:unor4wifi --port /dev/cu.usbmodemXXXX .
```

Open a 115200-baud serial terminal. The controller emits `ready` after boot. Every accepted command ends with `ok`; faults use `error: ...` and do not receive a trailing `ok`.

## Supported commands

| Command | Behaviour |
|---|---|
| `G21` | Select millimetres (the only supported unit) |
| `G90` | Select absolute positioning (the only supported mode) |
| `G0 X… Y… F…` | Raise the pen, then travel |
| `G1 X… Y… F…` | Coordinated line; preserves the explicit `M3`/`M5` pen state |
| `G28` | Home left, then bottom (blocked until the Y− switch is fixed) |
| `M3` / `M5` | Pen down / pen up |
| `M17` / `M18` | Enable / disable drivers; disabling invalidates homing |
| `M50 X… Y…` | Mark the current physical position and homed state (manual homing) |
| `M114` | Report position, homed state, and active microstep mode |
| `M119` | Report both home switches and the probe input |
| `M280 S…` | Send a bounded raw microsecond pulse for servo calibration |
| `M2` / `M30` | Pen up and disable the drivers (no auto-homing) |

Inches, relative positioning, arcs, unsupported words, multiple commands on one line, malformed numbers, and out-of-bounds targets are rejected. Soft limits are 0–400 mm X and 0–320 mm Y; targets are never silently clamped.
Travel feed is limited to 2000 mm/min and drawing feed to 800 mm/min; excessive `F` values are rejected rather than clamped.

## Stream a file

Dependencies are pre-installed in a Python venv. Use the wrapper:

```sh
tools/stream_gcode.sh --port /dev/cu.usbmodemXXXX examples/calibration_square.gcode
```

The sender waits for `ready`, sends one line, waits for `ok`, and stops immediately on the first firmware error. To stream over BLE instead of USB:

```sh
tools/stream_gcode.sh --ble examples/calibration_square.gcode
```

This scans for the "Drawbot" BLE peripheral, connects, and uses the same protocol over the Nordic UART Service.

## Project layout

- `config.h` — pins, calibration, feeds, limits, and safety bounds
- `corexy_math.h`, `dda_math.h`, `microstep_math.h` — portable CoreXY, coordinated-step, and safe mode-selection primitives
- `motion_controller.*` — acceleration, soft limits, switch monitoring, and homing
- `pen_controller.*` — servo control
- `gcode_parser.*`, `command_processor.*`, `serial_line_reader.*` — transport-independent command engine and USB framing
- `tests/` — native tests for transforms, DDA counts, parsing, limits, and interpolation

Run the portable tests with:

```sh
tests/run_tests.sh
```

## References

- [Arduino Uno R4 WiFi documentation](https://docs.arduino.cc/hardware/uno-r4-wifi/)
- [Uno R4 WiFi datasheet](https://docs.arduino.cc/resources/datasheets/ABX00087-datasheet.pdf)
- [Pololu A4988 carrier guidance](https://www.pololu.com/product/1182)
- [Protoneer CNC Shield V3 guide (archived mirror)](https://hexmix.ru/wp-content/uploads/2019/03/Arduino-CNC-Shield-V3.XX_.pdf)
