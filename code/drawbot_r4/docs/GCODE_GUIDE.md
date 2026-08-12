# Drawbot — G-code guide for contributors

The drawbot is a CoreXY pen plotter with a 400 mm × 300 mm drawing area
(paper anchored at the home corner). It speaks a deliberately small subset
of G-code: absolute millimetre moves only, with explicit pen control.

If your file follows the rules below it will print. The fastest way to be
sure is to run it through the validator:

```sh
python3 code/drawbot_r4/tools/gcode_check.py my_drawing.gcode
# → "OK: safe to draw on 400x300 mm paper"
```

## The complete command set

| Command | Meaning |
|---|---|
| `G21` | Millimetres (only unit; accepted, not required) |
| `G90` | Absolute positioning (only mode; accepted, not required) |
| `G0 X… Y… F…` | Raise the pen, then travel (max 2000 mm/min) |
| `G1 X… Y… F…` | Draw a straight line at the pen's current state (max 800 mm/min) |
| `M3` | Pen down |
| `M5` | Pen up |
| `M2` / `M30` | End of job: pen up, motors off |
| `M17` / `M18` | Motors on / off (operator use) |
| `M114` | Report position (operator use) |
| `M119` | Report switches (operator use) |
| `M50 X… Y…` | Mark current position (operator use — this is how homing is done) |
| `M280 S…` | Servo calibration pulse (operator use) |

Anything else is rejected: no arcs (`G2`/`G3`), no inches (`G20`), no
relative mode (`G91`), no multiple commands on one line.

## The rules (violations are rejected, not ignored)

- **One command per line.** No `G1 X10 Y10 M3`.
- **Coordinates are absolute millimetres** from the home corner, in
  `0 ≤ X ≤ 400`, `0 ≤ Y ≤ 300`. Anything outside is rejected — the machine
  never silently clamps. Leave a margin; the pen holder has physical width.
- **Line length ≤ 96 characters.**
- **Feed rates:** `G0` up to `2000`, `G1` up to `800` mm/min. `F` is
  optional — `G0` defaults to 2000, `G1` defaults to 800. Feed is **not**
  remembered from line to line.
- **Comments:** anything after `;` is ignored (also `( ... )`).
- **`G28` will not work** (a faulty limit switch) — the operator homes
  manually. Don't put `G28` in your file.
- **Don't try to print faster** by raising feeds or disabling limits — the
  machine rejects out-of-range values on purpose.

## The one thing everyone gets wrong: the pen

`G1` does **not** lower the pen — it only moves it. You must say `M3`
(pen down) yourself, and `M5` (pen up) when done:

```
M3        ; pen down
G1 X100 Y10 F800
G1 X100 Y100
M5        ; pen up
```

`G0` **always** raises the pen (that's what travel means). So a typical
drawing alternates: `G0` somewhere, `M3`, a few `G1` lines, `M5`, `G0`
somewhere else, and so on. Files that never issue `M3` draw nothing.

## Minimal working file

```gcode
; 60x40 mm rectangle with a diagonal
G21
G90
M5
G0 X20 Y20 F2000
M3
G1 X80 Y20 F800
G1 X80 Y60
G1 X20 Y60
G1 X20 Y20
M5
M2
```

## Generating files with a tool

- **SVG → G-code:** the standard pipeline is [vpype](https://vpype.readthedocs.io/)
  with the `vpype-gcode` plugin. Feed it the machine's numbers:
  units `mm`, `PEN_DOWN_COMMAND = M3`, `PEN_UP_COMMAND = M5`,
  `G1` feed ≤ 800, `G0` feed ≤ 2000, and clip the artwork to a 400×300 mm
  box placed at the origin. Then run the result through `gcode_check.py`.
- **Any other converter or slicer:** same checklist — only `G0/G1/M3/M5`,
  absolute mm, your feeds within the limits, your coordinates inside the
  box. Strip whatever else the tool emits (tool changes, arcs, G28,
  relative moves, M-codes the machine doesn't know). `gcode_check.py` will
  list exactly what to fix.
- **By hand:** the table above is the whole language; the minimal file is
  your template.

## Handing a file to the machine

1. Send the `.gcode` file to the operator (AirDrop is fine) — or just print
   this guide's rules and let them use `gcode_check.py` first.
2. The operator: paper in place, pen inked, carriage parked at the home
   corner, then:
   ```sh
   python3 code/drawbot_r4/tools/stream_gcode.py --ble your_file.gcode
   ```
3. The machine prints it end to end; a dropped Bluetooth link is recovered
   automatically mid-job.

## Quick reference: feed vs speed

| Move | Feed (mm/min) | mm/s |
|---|---|---|
| `G0` travel | 2000 | 33 |
| `G1` drawing | 800 | 13 |

A 100 mm line takes about 7.5 s. The whole 400×300 mm cross-hatch
presentation pattern takes about 30 minutes.
