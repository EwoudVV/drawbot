# Drawbot — Handoff V2 (motor/direction debug)

## Handoff V4 addendum (same-day commissioning results)

> The machine was **commissioned to the point of drawing** on 2026-08-01,
> operating by manual homing. Read this before the V3 addendum.

### Verified working (end to end, on hardware)
- ENABLE polarity: `DRIVER_ENABLE_ACTIVE_LEVEL = LOW` (D8 direct, active-low).
- Direction reversal (dir HIGH=right, LOW=left), bounce logic on the X switch.
- CoreXY Y-sign convention corrected in `corexy_math.h` (A advances X−Y,
  B advances X+Y on THIS machine) + `stepHomingAxis`; host tests updated.
- Pen servo calibrated: down=500 us (tip touches paper), up=1500 us.
  `PEN_CALIBRATED = true`, `SERVO_SAFE_MIN_US = 500`. Servo floor was 700 —
  the pen could not reach the paper; the holder was also adjusted.
- Scale: `BASE_UNITS_PER_MM = 80.8` verified by a 100 mm dot-to-dot ruler
  measurement. Microstepping confirmed effectively 1/16 by correct distance.
- Manual homing path: `M50 X<mm> Y<mm>` sets position + marks homed (added
  this session; no motion). G0/G1/M3/M5 now all work after M50.
- `M2`/`M30` no longer auto-home (would ram the dead Y switch); they raise
  the pen and disable drivers.
- 100 mm square drawn over BLE via `stream_gcode.py` (`square_manual.gcode`)
  — all commands ok, square verified by eye. **The machine draws.**
- `stream_gcode.py` fixed (missing `bleak.` prefix; BLEDevice details;
  "ready" wait now tolerant of already-running boards). `servo_sweep.py`
  added (BLE pulse sweeps, floor 500).

### Still open
- **Bottom (Y−) limit switch never triggers on ANY header**, while the
  switch's own continuity is perfect (open=1, closed=0 at its plug) and a
  known-good switch triggers on the same headers. All other links proven
  fine; the fault is confined to the switch's plug/header contact pair —
  unresolved, operator chose manual homing for now. G28 will keep failing
  until it is fixed (reterminate the plug or solder it directly).
- `tools/servo_sweep.py --range` argument parsing is broken; pass explicit
  value lists instead.
- Endurance run (30 min) not yet done. Home switches still pressed into
  wrong walls not re-checked; probe button never verified.

### Standard startup procedure (manual homing)
1. 24 V on, BLE: `M18` (release), push gantry to corner, `M50 X0 Y0`.
2. Verify `M114` → homed:yes. Load paper, `M3`/`M5` spot-checks.
3. Stream gcode with `stream_gcode.py --ble` or `ble_test.py fire` for
   single commands. Finish with `M2` (pen up, motors off).

---

## Handoff V3 addendum (same day, after the shield-polarity research)

> Read this first. It resolves the two "suspect, must re-verify" claims in
> V2 below and documents the firmware fix + the new incremental probe tool.

### What was resolved (2026-08-01, schematic + grbl 0.8 + empirical evidence)

1. **ENABLE polarity: D8 is ACTIVE-LOW, direct, no inverter.** On the
   official Protoneer CNC Shield V3.00 design, D8 is the common A4988
   ENABLE net, connected directly to all driver sockets, with R1 (10K)
   pulling it to 5 V (so the net idles high = drivers off). grbl 0.8 —
   which the shield is built for — writes D8 **LOW to enable**
   (`st_wake_up`) and HIGH to disable (`st_go_idle`, `$15=0`). This
   matches the empirical evidence exactly: old firmware left D8 LOW and
   the machine moved; the pinMode-first fix wrote D8 HIGH and the motors
   went completely dead.
2. **The "D13 → R10 → A4988 RESET rail" claim is REFUTED.** The official
   V3.00 design contains no such connection and no R10 (the only
   resistor is R1). D12/D13 are the SpnEn/SpnDir headers. MS pins cannot
   disable the drivers — they only select stepping resolution. A4988
   RESET floats on the shield; the carrier's SLEEP internal pull-up
   keeps the drivers running.
3. Therefore the V2 worry "one of those HIGHs kills the drivers" is
   answered: it was ENABLE (D8 HIGH), and nothing else. The regression
   was purely `DRIVER_ENABLE_ACTIVE_LEVEL = HIGH` in config.h plus
   hardcoded HIGH in M100/M101.

### Changes made this session (all compile clean, 100 864 B / 38 % flash)

- `config.h` — `DRIVER_ENABLE_ACTIVE_LEVEL = LOW`, with a comment citing
  the verified facts. This restores motion.
- `command_processor.cpp` — M100/M101 now write
  `config::DRIVER_ENABLE_ACTIVE_LEVEL` to D8 instead of hardcoded HIGH;
  the "inverter" comments are gone.
- `motion_controller.cpp` — stale RESET-rail/1/16-mandatory comments in
  `preferredMode()`, `homeAxis()`, `home()` replaced with the real
  rationale (1/16 base units, 80.8 units/mm calibration).
- `tools/driver_state_probe/driver_state_probe.ino` — NEW incremental
  probe (BLE + USB Serial, menu commands, see below). Compiles to
  86 080 B.

### What still needs physical verification (in order, hand on the 24 V cutoff)

1. Upload (USB only, 24 V off), then 24 V on.
2. **ENABLE lock check:** over BLE send `M17` → motors should LOCK
   (holding torque). `M18` → release. Alternative/fallback: flash the
   probe sketch, send `?` then `E0` (enabled) / `E1` (off).
   If no lock, measure D8 vs the driver ENABLE pins with a multimeter:
   expect D8 LOW ⇔ ENABLE LOW with no inversion.
3. **Direction reversal (the original bug):** gantry mid-travel, clear of
   switches: `M100 S200 F2000` (dir HIGH) — note travel direction; then
   `M100 S200 F2000 X1` (dir LOW) — must travel the opposite way. Then
   the bounce test near a wall: `M100 S4000 F2000` — the gantry must
   reverse off the wall (look for "limit bounce at step N"). Then the
   full `M100 S40000 F2000`.
   Probe alternative: `D1`, `S200`, `D0`, `S200` — the machine must
   reverse between the two bursts.
4. **Microstepping** (only affects distance, not motion): with a
   multimeter on the MS pads, `M111` in the probe should show 1/16 on
   the actual MS1/2/3 pins. If the D7/D12/D13 mod wires are missing,
   the machine runs full-step and moves 16× too far per unit — do NOT
   conclude the driver is broken; re-check BASE_UNITS_PER_MM after the
   mod wiring is confirmed.
5. Record the actual VREF at the trimpots (keep ≤ ~1.4 V). Then G28,
   M280 pen calibration, square.

### Probe sketch usage

```sh
arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi outputs/drawbot_r4/tools/driver_state_probe/
arduino-cli upload --fqbn arduino:renesas_uno:unor4wifi \
  --port /dev/cu.usbmodemF412FA75568C2 outputs/drawbot_r4/tools/driver_state_probe/
tools/.venv/bin/python tools/ble_test.py send "?"   # or E0 / M7 / D0 / S200 / R
```

Commands: `?` report | `E0/E1` enable (E0 = enabled, active-low) |
`M0..M7` MS bits (M7 = 1/16) | `D0/D1` both DIR | `A0/A1`, `B0/B1`
per-motor DIR | `S[n]`, `SA[n]`, `SB[n]` step bursts (default 400,
2 ms period) | `R` reset (drivers off) | `H` help. Boot state is safe
(ENABLE high, drivers off). Also accepts USB Serial input when 24 V is
off (multimeter level checks).

### Standing notes for the next AI

- The V2 sections below remain accurate except where this addendum
  disagrees (ENABLE polarity, R10/RESET rail, "1/16 mandatory").
- The pin write-order discovery in V2 is still the single most
  important Uno R4 fact: `pinMode(OUTPUT)` resets the output data
  register — always write pinMode FIRST.
- `HOME_SEARCH_MARGIN_MM`, `MOTOR_*_DIRECTION_INVERTED`, homing
  end-to-end, and the 80.8 units/mm calibration all remain unverified
  until the machine moves both ways.

---

## Handoff V2 body (original; see addendum above for the resolved claims)

> Written 2026-08-01 after a session where the root cause of the "never
> reverses" bug was found and *partially* fixed — the fix then exposed that
> more shield-polarity assumptions are wrong. **Read this entire file before
> touching anything.** It supersedes `HANDOFF.md` on every point where they
> disagree. The old file is still the reference for BLE/architecture/commands.

## Critical status (what a new AI must know first)

1. **The machine currently does not move at all.** With 24 V applied, the
   motors are not even locked (no holding torque). The previously-flashed
   firmware ran the motors but they **never changed direction** (gantry
   rammed into the wall, bounce logic fired, motors kept pushing).
2. **The handoff's "digitalWrite-then-pinMode" pin recipe is PROVEN WRONG**
   by an on-device probe (see below). The correct order on this Uno R4 core
   is `pinMode(pin, OUTPUT); digitalWrite(pin, val);`.
3. **Several shield-polarity claims in the old handoff are now suspect and
   must be re-verified with a multimeter, not assumed.**
4. The limit switches, BLE link, bounce logic, and switch detection all
   **work**. The problem is 100 % in the motor/direction/enable path.
5. Do **not** run any long motion test until the ENABLE/MS polarity is
   determined incrementally (test plan at the end).

---

## The one verified discovery: pin write order on the Uno R4

A probe sketch (`outputs/drawbot_r4/tools/pin_probe/pin_probe.ino`) writes
each pin with three sequences and reads it back. Run on the actual board,
actual core (`arduino:renesas_uno:unor4wifi`, core 1.6.0). Results, for
every pin tested (2, 3, 5, 6, 7, 12, 13) — identical on all of them:

| Sequence | Result |
|---|---|
| `digitalWrite(pin, HIGH); pinMode(pin, OUTPUT);` | pin ends **LOW** |
| `pinMode(pin, OUTPUT); digitalWrite(pin, HIGH);` | pin ends **HIGH** |
| `pinMode(pin, OUTPUT);` then plain `digitalWrite(pin, HIGH)` | pin ends **HIGH** |
| `digitalWrite(pin, HIGH); pinMode(OUTPUT);` then `digitalWrite(pin, LOW)` … | **LOW, and then LOW again — it cannot be re-raised via the recipe** |
| plain `digitalWrite` toggling on an already-OUTPUT pin | works correctly |

So `pinMode(OUTPUT)` **resets the output data register to 0**, clobbering
any value written before it. `R_IOPORT_PinCfg(OUTPUT)` in the core
(`cores/arduino/digital.cpp`) is what does it. The old handoff's lesson
"digitalWrite alone on an OUTPUT pin doesn't drive it — use
digitalWrite-then-pinMode" is **backwards and was itself a misdiagnosis**
(same family as the "thermal shutdown" red herring).

### Why the old firmware "moved but never reversed"

The old code everywhere used `digitalWrite(val); pinMode(OUTPUT);` → every
write collapsed to LOW:

- **DIR pins (D5/D6): always LOW** → direction could never change. The
  bounce latch ran (we saw `limit bounce at step N` prints) but the motors
  were told to go the same way every time → ramming.
- **MS1/MS2/MS3 (D7/D12/D13): always LOW** → machine was actually running
  in **full-step**, not 1/16, regardless of all the `kSixteenth` code.
- **ENABLE (D8): always LOW** → and the machine still moved → **D8 LOW
  evidently leaves the drivers enabled on this board** (see polarity
  question below — this contradicts the old handoff).
- **STEP pins (D2/D3): toggled in the loop with plain `digitalWrite`**
  (no `pinMode` re-call) → **worked fine** → motors stepped. This is why
  "stepping worked" while "direction never worked".

---

## Session timeline (what was observed, in order)

1. Over USB: switches verified — `M119` shows `x_home`/`y_home` flipping
   independently when pressed, probe stays `open`. Both switches and the
   bounce latch logic are fine. (tool: `tools/switch_test.py`)
2. Over BLE, 24 V on: `M100 S40000 F2000` (new telemetry build) showed the
   bounce latch firing repeatedly — `limit bounce at step 918`, then
   `x:TRIGGERED y:TRIGGERED` a second later — but the gantry stayed pressed
   into the wall/corner and kept re-triggering. Motors never reversed.
3. User manually pressed switches during a run: **motors did not change
   direction.** Confirmed: not a switch problem.
4. Pin probe (above) → discovered the write-order bug.
5. Fixed the order in `command_processor.cpp` (M100/M101 `pi`/`setDirs`) and
   `motion_controller.cpp` (`begin`, `enableDrivers`, `disableDrivers`,
   `setMicrostepMode`, `setMotorDirections`) → compiled → uploaded.
6. **Regression:** now with 24 V on, `M100 S40000 F2000` runs (progress
   lines stream) but the motors do not move at all — not even locked.

## Why the fix made it worse (analysis, still to be verified)

The fix changed five pin groups from LOW to their *intended* values:
MS1/2/3 HIGH, ENABLE HIGH, DIR HIGH. One of those HIGHs kills the drivers.
Two candidates, in order of likelihood:

1. **ENABLE (D8) polarity is active-LOW in practice.** Old state (D8 LOW)
   = drivers enabled → machine moved. New state (D8 HIGH) = drivers
   disabled → no current, no lock, no motion. This fits "not even locked"
   perfectly. The old handoff claimed "D8 → ENABLE, inverted on the shield
   (so HIGH enables)" — **this claim is now contradicted and must be
   re-verified on the actual board** (the shield may not invert D8, or D8
   may not even reach the driver ENABLE pins).
2. **The "D13 → R10 → A4988 RESET rail" claim may be wrong.** If that
   claim were true, the old code (MS3 clobbered LOW) would have held the
   drivers in RESET — yet the machine moved. So either the R10 rail is not
   RESET, is disconnected, or has inverse polarity. Driving MS3 HIGH may
   then be *asserting* something that disables the drivers.

Both are examples of why: **the shield's enable/reset/microstep polarity
must be measured, not assumed.**

---

## What is verified vs. suspect

### Verified (this session, on this hardware)
- Switch wiring/readback: active LOW via `INPUT_PULLUP`, `M119` correct.
- BLE Nordic UART bridge works; long runs drop the link (expected); board
  keeps executing; reconnect works.
- The pin write-order behavior in the table above (all tested pins).
- With old clobbered-to-LOW firmware: motors step, machine moves in one
  direction, never reverses, ramps against the wall.
- With new pinMode-first firmware: motors completely dead (no lock).
- M100/M101 bounce latch + telemetry prints + BLE polling during loops
  (progress every 512 steps, switch-state changes, `output.flush()` in
  loop) — all work.

### Suspect (old handoff claims now contradicted)
- "D8 → ENABLE, inverted so HIGH enables" — likely wrong (see above).
- "D13 → R10 → RESET rail; only 1/16 (MS3 HIGH) releases RESET" — at least
  incomplete; old code ran with MS3 LOW.
- "Only 1/16 stepping works" — old code ran fine in full-step.
- "VREF must be 0.9–1.1 V" — may still be fine, but it was tuned while
  chasing the same misdiagnosis family. Verify with multimeter, don't trust.

### Known-good from old handoff (still valid)
- BLE architecture, UUIDs, device address, chunked-println bridge.
- Command set (G0/G1/G21/G28/G90, M2/M3/M5/M17/M18/M100/M101/M114/M119/M280).
- Power rules (USB vs 24 V never simultaneously), VREF range as a *bound*
  (do not exceed ~1.4 V), motors NEMA 17, CoreXY kinematics, 80.8 base
  units/mm at 1/16.
- CNC Shield V3.00 basics: X socket D2/D5, Y socket D3/D6, end stops
  D9/D10/D11.

---

## Current firmware state (what the next AI inherits)

Files changed today (all compile clean, 100.8 kB / 38 % flash):

- `outputs/drawbot_r4/command_processor.cpp` — M100/M101: `pi`/`p`/`setDirs`
  lambdas now `pinMode` first, then `digitalWrite`. Plus new live telemetry:
  switch-state change prints, `limit bounce at step N`, progress every 512
  steps, `output.flush()` after prints, `motion_.pumpIdle()` every step.
- `outputs/drawbot_r4/motion_controller.cpp` — same pinMode-first order in
  `begin()`, `enableDrivers()`, `disableDrivers()`, `setMicrostepMode()`,
  `setMotorDirections()`.
- `outputs/drawbot_r4/motion_controller.h` — added `pumpIdle()`.
- `tools/pin_probe/pin_probe.ino` — the probe (compiles for
  `arduino:renesas_uno:unor4wifi`; prints results continuously in `loop()`).
- `tools/ble_test.py`, `tools/switch_test.py` — BLE/switch test harnesses.

**To restore "machine moves (but never reverses)":** revert the write order
to `digitalWrite` then `pinMode` in `setDirs`/`pi`/`p` (M100/M101) and in
`motion_controller.cpp`. **Do not do this as a fix** — it is only a
diagnostic baseline; it cannot reverse direction.

## The test plan for the next AI (do this before any long motion test)

Goal: determine, one pin group at a time, what actually drives the A4988s.

1. **Measure, don't guess.** With 24 V on and motors unplugged (or plug
   into the driver sockets on the shield with a voltmeter):
   - D8 level vs. driver ENABLE pin level — is there an inverter? Which
     D8 level gives ENABLE low (enabled)?
   - D13/D12/D7 vs. MS1/MS2/MS3 pins — which net is which?
   - D13 vs. the A4988 RESET/SLEEP pins — does R10 actually connect, and
     to what? (RESET is active-low: released = HIGH.)
   - Measure the actual VREF at the trimpot test point and record it.
2. **Extend the pin probe** (or write a new one) to hold each combination
   of ENABLE/MS and verify the motors lock (with 24 V on, small current):
   - Find the ENABLE polarity that locks the motors (try D8 HIGH vs LOW
     with MS pins fixed).
   - Find which MS level combination keeps them out of reset.
3. **Verify direction.** Drive DIR high vs low with a slow fixed step burst
   (e.g. 200 steps, 10 ms period) and confirm the gantry actually reverses
   when DIR changes. This is the bug that started everything.
4. Only after direction reversal works: re-run the M100 bounce test over
   BLE (`M100 S4000 F2000` from near a wall, or the full
   `M100 S40000 F2000`), then G28, then pen calibration, then the square.
5. Then decide the correct `config.h` values: `DRIVER_ENABLE_ACTIVE_LEVEL`,
   `MOTOR_A/B_DIRECTION_INVERTED`, and whether 1/16 microstepping is
   actually required or was another artifact of the same misdiagnosis.

## Safety rules (unchanged, still binding)

- USB and 24 V must never be applied simultaneously. Upload = USB only,
  24 V off. Motion tests = 24 V only, USB off.
- Hand near the 24 V cutoff during any motion test.
- VREF: keep at/below ~1.4 V at the test point. Prefer ~1 A/phase.
- Unplug a stepper only with driver power off.
- Do not run long ramming tests; the gantry CANNOT reverse until the
  direction issue is fixed.

## Open questions for the next AI

- Actual ENABLE polarity / whether D8 reaches ENABLE at all.
- Actual MS1/2/3 routing and whether 1/16 is needed (old code ran full-step
  fine — maybe the "1/16 mandatory" was wrong).
- Actual VREF measurement (record it).
- Which physical end each switch is at (X+/X−), and DIR polarity mapping
  for homing — only discoverable after direction reversal works.
- `HOME_SEARCH_MARGIN_MM`, `MOTOR_*_DIRECTION_INVERTED`, and the whole
  homing sequence still unverified end-to-end.

## Build/upload/test commands (same as before)

```sh
arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi outputs/drawbot_r4/
arduino-cli upload --fqbn arduino:renesas_uno:unor4wifi \
  --port /dev/cu.usbmodemF412FA75568C2 outputs/drawbot_r4/
tools/.venv/bin/python tools/ble_test.py send "M119"        # short query
tools/.venv/bin/python tools/ble_test.py fire "M100 S4000 F2000" --listen 30
tools/.venv/bin/python tools/switch_test.py --port /dev/cu.usbmodemF412FA75568C2
```
