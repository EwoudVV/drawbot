# Drawbot Project — AI Handoff

> Comprehensive handoff for resuming this project in a fresh AI session.
> Read this first. Do not trust old KiCad notes or earlier diagnoses that have
> since been corrected (see "Lessons learned" below).

## Project location

```
/Users/ellievanvooren/Documents/codex/2026-06-19/i-have-a-drawing-machine-project/
```

The machine firmware is a single Arduino sketch under
`outputs/drawbot_r4/`. The machine is a CoreXY-style pen plotter built from
spare-parts steppers and 3D printer hardware, controlled by an Arduino Uno
R4 WiFi fitted with a Protoneer / GRBL 0.8-style CNC Shield V3.00 driving
two A4988 stepper drivers. G-code is sent over Bluetooth Low Energy (BLE)
using the Nordic UART service, with a chunked-println bridge that talks to
the Arduino USB serial.

Per-session directory naming is `<YYYY-MM-DD>/i-have-a-drawing-machine-project/`.
The `outputs/drawbot_r4/` tree is the working copy of the sketch and is
overwritten freely during work; the `work/` directory is a scratch area for
calibration artifacts and reference designs and should be treated as
read-only history.

## Anchored summary (the five facts the next agent must keep)

### Objective
- Get the CoreXY drawbot homing and drawing reliably over BLE.
- Stepping is now proven physically correct (motors lock, run, hit a wall,
  bounce off a limit switch). Remaining work: homing (`G28`), pen up/down
  calibration, end-to-end G-code streams from `examples/calibration_square.gcode`.

### Important details
- Power: **USB and 24 V cannot be applied simultaneously.** USB is used for
  sketch upload (24 V off); 24 V is applied for testing (USB off). Switching
  mid-test is fine but never overlap.
- **CNC Shield V3.00**, *not* the KiCad board that lived in the project
  earlier. The user explicitly told us to ignore the KiCad schematics as
  "very old" — the ENABLE-to-GND trace on the KiCad file is wrong for the
  current shield. Relevant shield fact: **D13 (MS3) is wired through R10
  into the A4988 RESET rail**, so any microstep mode with MS3 = LOW
  holds the drivers in reset. Only 1/16 stepping (MS1 = MS2 = MS3 = HIGH)
  keeps the drivers out of reset.
- **Uno R4 GPIO quirk**: `digitalWrite()` on a pin already in `OUTPUT`
  mode does not actually drive the pin. The reliable recipe is
  `digitalWrite(pin, val); pinMode(pin, OUTPUT);`. `digitalRead()` on the
  same pins returns the *logic level* correctly, so a `digitalRead`-based
  smoke test is misleading: it told us "the pins look right" while the
  motor wasn't actually getting current. The cure is always the
  `digitalWrite`-then-`pinMode` pair.
- The end-stop header is wired to `INPUT_PULLUP` and reads active **LOW**,
  so a pressed switch reads as `LOW`.
- `DRIVER_ENABLE_ACTIVE_LEVEL = HIGH` (D8 is inverted by the shield).
- Step timing is accurate on the R4. A requested 2 ms period measures
  ~2.094 ms, a 1 ms period measures ~1.050 ms, 500 µs measures ~528 µs,
  250 µs measures ~267 µs. The CPU is not the bottleneck.
- **Driver current**: VREF had been turned DOWN half a turn in an earlier
  session to "fix" a thermal shutdown that turned out to be a RESET-rail
  bug, not thermal. The lowered VREF left the A4988s unable to deliver
  enough current at higher step rates (NEMA 17 coil time constant is
  ~2 ms — at 4 kHz only ~12 % of the target current builds up in the
  coil). The user has since turned the trimpot back up by half a turn.
  **Target VREF is 0.9 – 1.1 V at the test point** (≈ 1.1 – 1.4 A with
  the 0.1 Ω sense resistors). Never push VREF past ~1.4 V.

### Work state
- **Done**: pin-driving recipe applied throughout `motion_controller.cpp`
  and both test commands; 1/16 microstep forced in `preferredMode()`,
  `home()`, and `homeAxis()` so MS3/D13 stays HIGH; `M100` rewritten as a
  raw step test with reported measured period; `M101` is the bare-pin
  smoke test (now also has limit-switch bounce); `M100` and `M101` both
  have the same limit-switch bounce protection (state-machine latch —
  see below); the sketch compiles clean (~100 kB / 38 % of flash).
- **Active**: the bounce latch in `M100` / `M101` was just updated from a
  naive per-step flip (which jittered at the lever) to a per-press latch
  that re-arms only after the switch reads open for 4 consecutive steps.
  The bounce test was interrupted before it could finish — the next agent
  should re-run the long BLE bounce test and confirm the gantry reverses
  cleanly without re-triggering.
- **Blocked / not yet verified**: `G28` homing, the full speed sweep
  (F500, F1000, F2000, F4000) without bumping a wall, the pen servo
  calibration (`M280`), and end-to-end runs of `examples/calibration_square.gcode`.

### Next move
1. Confirm the gantry is roughly centered (manually, with 24 V off).
2. Switch 24 V on, USB off, and run `M100 S16000 F2000` (≈ 20 s) over
   BLE. Expect: gantry moves until it hits a side, then reverses cleanly
   without rapid re-triggering, then hits the other side, then reverses
   again. Look for `limit bounce at step N` log lines.
3. Then `M100 S4000 F500` etc. to confirm the higher step rates work
   now that VREF is back up.
4. Once stepping is clean, run `G28` and confirm homing.
5. Then calibrate the pen with `M280`, and finally stream
   `examples/calibration_square.gcode`.

### Relevant files
- `outputs/drawbot_r4/drawbot_r4.ino` — main sketch, BLE init, BLE
  bridge, `BlePrint` chunked-flush wrapper.
- `outputs/drawbot_r4/config.h` — pin map, `DRIVER_ENABLE_ACTIVE_LEVEL`,
  `BASE_UNITS_PER_MM = 80.8`, feed-rate limits, switch active level,
  servo bounds.
- `outputs/drawbot_r4/command_processor.cpp` — M-code handlers,
  including `M100` and `M101` (raw step + bounce).
- `outputs/drawbot_r4/command_rules.h` — supported G/M codes whitelist.
- `outputs/drawbot_r4/motion_controller.cpp` — pin driving,
  microstep selection, homing.
- `outputs/drawbot_r4/gcode_parser.cpp` / `gcode_parser.h` — line parser.
- `outputs/drawbot_r4/microstep_math.h` — `MicrostepMode` and the
  `selectCompatibleMode` cascade.
- `outputs/drawbot_r4/tools/.venv` — Python virtualenv with `bleak` for
  BLE testing (Python 3.14).
- `outputs/drawbot_r4/tools/stream_gcode.py` and
  `outputs/drawbot_r4/tools/stream_gcode.sh` — file-streaming helpers.
- `outputs/drawbot_r4/examples/calibration_square.gcode` — the
  end-to-end test print.
- `outputs/drawbot_r4/docs/COMMISSIONING.md` and
  `outputs/drawbot_r4/docs/WIRING.md` — physical wiring reference.

## Hardware

### Controller
- **Arduino Uno R4 WiFi** (Renesas RA4M1). Has native BLE; the sketch
  uses the Arduino `BLEPeripheral` library that ships with the R4
  core. Build target: `arduino:renesas_uno:unor4wifi`.

### Stepper driver carrier
- **CNC Shield V3.00** (Protoneer / GRBL 0.8 layout). Sits on top of the
  Uno R4. Two A4988 stepper drivers in the X and Y sockets; the Z
  socket is unused. Z.STEP is rewired to a hobby servo on pin 4.
  Important wirings:
  - D8 → ENABLE, inverted on the shield (so `HIGH` enables).
  - D7 → MS1, D12 → MS2, D13 → MS3.
  - D13 feeds the A4988 RESET rail through R10 (100 Ω). This is the
    reason 1/16 microstepping is mandatory.
  - X socket = motor A (steps on D2, dir on D5).
  - Y socket = motor B (steps on D3, dir on D6).
  - End-stop headers: D9 = X, D10 = Y, D11 = Z/probe.

### Motors
- Two NEMA 17 steppers wired to A and B. Coil time constant ≈ 2 ms.
  1.8° / step (200 full steps / rev). At 1/16 microstepping the sketch
  uses 80.8 base units / mm (recovered from an old calibration).

### Power
- 24 V bench supply feeds the CNC shield VIN / motor rail. **Never apply
  USB and 24 V at the same time.** USB upload has 24 V off; 24 V testing
  has USB off. The Arduino is powered either by USB (upload) or by the
  shield's onboard 5 V regulator (BLE test), but never both rails at
  once.

### Trimpot
- Both A4988 modules have a small VREF trimpot. Target **0.9 – 1.1 V** at
  the test point; do **not** exceed ~1.4 V. The user turned it up half a
  turn from a too-low setting that had been chasing a misdiagnosed
  thermal shutdown.

### Limit switches
- Three mechanical switches on the end-stop header. Active LOW with
  `INPUT_PULLUP`. X switch on D9, Y switch on D10. The probe pin D11 is
  for the pen touch-probe, not for gantry limits.

## Firmware architecture

### Sketch structure
- `drawbot_r4.ino` — `setup()` initializes serial, BLE, the motion
  controller, and the pen. `loop()` reads serial lines, dispatches G/M
  commands through `CommandProcessor`, and writes responses back to
  serial. A `BlePrint` helper routes the same `Print` API the command
  handlers use out through BLE, chunked to fit MTU.
- `gcode_parser.{cpp,h}` — line-level parser, produces a `GCodeCommand`
  struct.
- `command_processor.{cpp,h}` — `handleGCode()` and `handleMCode()`
  dispatch tables. Uses a `requireNoArguments()` helper and a
  `setError()` helper to format error messages.
- `command_rules.h` — `isSupportedGCode()` / `isSupportedMCode()`.
- `motion_controller.{cpp,h}` — owns step generation. `begin()`,
  `enableDrivers()` / `disableDrivers()`, `setMicrostepMode()`,
  `setMotorDirections()`, `home()` (full G28), `homeAxis()` (single
  axis), `moveTo()` (queued via DDA-style ramp).
- `pen_controller.{cpp,h}` — servo control. `forceSafeUp()` raises the
  pen. `setAngle()` / `setPulse()` use the Arduino `Servo` library.
- `corexy_math.h`, `dda_math.h`, `microstep_math.h`,
  `workspace_math.h`, `machine_types.h` — math primitives.

### Supported G-codes
- `G0` / `G1` — linear move (rapid vs feed).
- `G21` — millimeters.
- `G28` — home all axes (implemented; not yet verified end-to-end).
- `G90` — absolute positioning.

### Supported M-codes
- `M2` / `M30` — pen up + home (used at end of program).
- `M3` / `M5` — pen down / pen up.
- `M17` / `M18` — enable / disable drivers.
- `M100` — raw step test. `S<steps>`, `F<period_us>` (default 2000),
  `X1` reverses both DIR pins, includes a limit-switch bounce latch.
- `M101` — bare-pin smoke test. `S<steps>` optional. Same bounce
  protection.
- `M114` — report current position.
- `M119` — report end-stop / probe state.
- `M280` — set servo pulse width (pen calibration).

### Test commands in detail

`M100 S<steps> [F<period_us>] [X1]`
- Enables drivers, drives both motors together (X-axis move).
- `F` is the step period in microseconds; clamped to ≥ 100 µs.
- Pulse HIGH for 50 µs, then LOW for `period_us − 50 µs`.
- `X1` starts in the reverse direction.
- On every step, polls `xHomeActive() || yHomeActive()`. If either is
  active and not already latched, flips both DIRs, latches, and prints
  `limit bounce at step N`. The latch stays engaged until the switch
  reads open for `kSwitchReleaseSteps = 4` consecutive steps, so the
  gantry always gets clear of the lever before re-arming. **Do not
  reimplement this as a "check + flip" pair** — that jittered at the
  switch because the gantry had not moved far enough to release it
  between the reverse and the next check.
- Reports `did N steps, requested P us, measured M us = X mm` using
  `micros()` for the wall-clock measurement.

`M101 [S<steps>]`
- Same as `M100` but no F / X / Y word support and the default step
  count is 400 at a fixed 2 ms period. Used as the "no-framework"
  smoke test when M100 misbehaves.

### Microstep mode handling
- `selectCompatibleMode()` cascades: if the requested mode is coarser
  than the driver can supply, it picks the next finer mode the driver
  actually supports. With `DRAW_*_STEP_MIN_MM` set so that even the
  finest motion wants 1/16, the cascade lands on 1/16 — which is
  *required* on this hardware to keep MS3 HIGH and RESET released.
- `home()` and `homeAxis()` explicitly call `setMicrostepMode(kSixteenth)`
  before seeking, so homing never accidentally drops MS3 LOW.

### Pin-driving convention (apply everywhere you touch a pin)
```cpp
digitalWrite(pin, value);
pinMode(pin, OUTPUT);
```
The order is mandatory on the R4. `pinMode(pin, OUTPUT)` first loses
the value on some peripheral mappings; `digitalWrite` alone on a pin
that is already OUTPUT does not re-drive it. The lambda `pi` inside
M100/M101 captures the pattern; in `motion_controller.cpp` it is
inlined into each method that touches a pin.

## BLE bridge

- The Arduino exposes a single Nordic UART service:
  - Service `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
  - TX characteristic `6E400002-...` (notify, Arduino → host)
  - RX characteristic `6E400003-...` (write, host → Arduino)
- Device name on macOS: `51974A51-AB5A-54FF-89A7-69167EA6986E`. The
  service UUID is what the host actually scans for; the device name is
  the one shown in the macOS Bluetooth menu.
- The Arduino `loop()` reads from `Serial` and writes to BLE; the
  `BlePrint` wrapper chunks the output into MTU-sized notifications
  with a small inter-chunk delay.
- Host-side test scripts live in `outputs/drawbot_r4/tools/.venv`
  (Python 3.14, `bleak`). Mac Bluetooth must be on or `bleak` will
  silently fail to enumerate. If the connection fails, check
  `System Settings → Bluetooth` first.
- The Python one-liner that has been used throughout the project:
  ```python
  import asyncio
  from bleak import BleakClient
  TXC='6E400002-B5A3-F393-E0A9-E50E24DCCA9E'
  RXC='6E400003-B5A3-F393-E0A9-E50E24DCCA9E'
  rec=b''
  def n(s,d):
      global rec; rec+=d
      while b'\n' in rec:
          l, rec = rec.split(b'\n',1)
          m = l.decode().strip()
          if m: print(f'< {m}')
  async def main():
      async with BleakClient('51974A51-AB5A-54FF-89A7-69167EA6986E', timeout=60) as c:
          await c.start_notify(TXC, n)
          await c.write_gatt_char(RXC, b'G28\n')
          await asyncio.sleep(30)
  asyncio.run(main())
  ```
- Long synchronous M-codes (anything that takes more than a few
  seconds) will exceed the macOS BLE supervision timeout and the
  connection will drop mid-run. **The command continues executing on
  the Arduino** — the drop only kills the notification stream. Reconnect
  afterward to query `M114` / `M119` for state. The Arduino does not
  need to be reset; the M-code is just running on the bare metal.

## Build, upload, and test commands

```sh
# Compile only
arduino-cli compile \
  --fqbn arduino:renesas_uno:unor4wifi \
  "/Users/ellievanvooren/Documents/codex/2026-06-19/i-have-a-drawing-machine-project/outputs/drawbot_r4/"

# Upload (24 V OFF, USB plugged in)
arduino-cli upload \
  --fqbn arduino:renesas_uno:unor4wifi \
  --port /dev/cu.usbmodemF412FA75568C2 \
  "/Users/ellievanvooren/Documents/codex/2026-06-19/i-have-a-drawing-machine-project/outputs/drawbot_r4/"

# BLE test (24 V ON, USB OFF)
/Users/ellievanvooren/Documents/codex/2026-06-19/i-have-a-drawing-machine-project/outputs/drawbot_r4/tools/.venv/bin/python \
  -c '... (one-liner above) ...'
```

Upload port: `/dev/cu.usbmodemF412FA75568C2`. The Arduino IDE also
recognizes the board under the same name.

## Lessons learned (do not re-learn these the hard way)

1. **The "thermal shutdown" was a RESET bug.** The Uno R4 was holding
   the A4988 RESET pin LOW because D13 / MS3 was being driven LOW by a
   microstep mode other than 1/16. The earlier fix idea — turning VREF
   *down* to keep the drivers cool — was wasted effort, and lowered
   the headroom we now need. VREF must stay around 0.9 – 1.1 V.
2. **`digitalRead` is not a motor-current probe.** A `digitalRead` can
   show the right level on a pin that the R4 is not actually driving
   hard enough to overcome the driver's input threshold. The
   `digitalWrite`-then-`pinMode` pair is the only known-reliable way to
   set an output on this board.
3. **A naive per-step limit-switch check will jitter.** The gantry
   needs time to move away from the lever after a bounce. Per-press
   latch + 4-step open-debounce is the minimum that works. A single
   "flip and continue" with no latch is worse than no bounce at all
   because it oscillates at the switch.
4. **The KiCad design is stale.** The current shield is a CNC Shield
   V3.00, *not* whatever the KiCad project shows. Anything in the
   KiCad files that conflicts with the live shield should be ignored.
5. **macOS BLE will drop a long-running connection.** That's fine —
   the Arduino keeps executing. Plan tests accordingly: short queries
   over BLE, long motions fire-and-forget, then reconnect.

## Open questions / known unknowns

- **VREF exact value.** The user turned the trimpot up by "half a turn"
  from the earlier too-low setting. The target is 0.9 – 1.1 V at the
  test point; we have not measured the actual value with a multimeter.
  Measuring it and recording the number is a good first step on the
  next session.
- **Switch polarity and "open" position.** The end-stop switches
  read `LOW` when pressed (active LOW, `INPUT_PULLUP`). We do not know
  which physical end (X+ or X−) the X switch sits at, and similarly for
  Y. The M100 bounce test will reveal this empirically: if the bounce
  sends the gantry in the wrong direction, the home switch is at the
  other end and we should still end up centered.
- **`G28` homing direction and seek distance.** The homing code uses
  `HOME_SEARCH_MARGIN_MM = 25 mm` and a final-release of 1 mm. The
  physical axis travel is 400 mm × 320 mm. The first G28 run after the
  bounce protection lands should be watched carefully: if the seek
  direction is wrong, the gantry will run to the opposite end of the
  axis. The bounce latch will eventually catch it, but it is better
  to verify the seek direction in `homeAxis()` first.
- **Pen calibration.** `PEN_CALIBRATED = false`; the safe-up /
  safe-down bounds are guessed. M280 is the way to find the real
  numbers.
- **Acceleration and feed rates.** `ACCELERATION_MM_S2 = 200` and
  `TRAVEL_FEED_MM_MIN = 2000` are starting points. They have not been
  tuned against the now-correct VREF; the gantry may be able to take a
  higher travel feed now.
- **`selectCompatibleMode` cascade.** The cascade goes finer-than, not
  coarser-than, which is what we want for keeping MS3 HIGH. But it
  means a future change to the feed-rate logic that asks for full-step
  motion will silently upgrade to 1/16. If that ever matters, re-read
  `microstep_math.h` before assuming a mode change took effect.

## Suggested next session plan

1. Re-read this file and the anchored summary.
2. With 24 V off, manually center the gantry on both axes.
3. Switch 24 V on, run `M119` over BLE to confirm the switch state
   baseline.
4. Run `M100 S16000 F2000` and watch for `limit bounce at step N`
   lines; confirm the gantry oscillates between both ends without
   jittering.
5. Run the speed sweep `M100 S1600 F500 / F1000 / F2000 / F4000`,
   alternating directions so the gantry stays roughly centered.
6. Verify the measured step period matches the requested one
   (look at the `measured M us` line).
7. If everything looks good, run `G28`. Watch the seek direction on
   both axes; if either runs the wrong way, the limit switch is at the
   opposite end of travel from what `homeAxis()` assumed.
8. Once `G28` is happy, use `M280` to find the pen up / pen down
   pulses, then update `config.h` and set `PEN_CALIBRATED = true`.
9. Finally stream `examples/calibration_square.gcode` and watch the
   first plot.

If the gantry still cannot move at the higher step rates after the
VREF bump, measure VREF with a multimeter; if it is below 0.9 V,
turn the trimpot up another small amount and retry. If it is already
at or above 0.9 V, the issue is mechanical (belt tension, pulley
eccentricity, or motor current limit) rather than electrical.
