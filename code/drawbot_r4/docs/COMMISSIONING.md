# Commissioning and verification

Work through this in order. Automated homing is intentionally late in the process.

## 1. Unpowered inspection

- Confirm the shield is seated without offset pins or bent headers.
- Verify A4988 orientation from pin labels, but leave both modules out initially.
- Verify all six microstep jumper headers are removed. Check continuity from D7 to both MS1 control pads, D12 to both MS2 pads, and D13 to both MS3 pads. Confirm all three nets are isolated from +5 V.
- Check 24 V polarity, fusing, emergency cutoff, capacitor polarity, common ground, and that buck +5 V is isolated from Arduino +5 V.
- Confirm each motor's two coil pairs with a continuity meter.
- Confirm each switch is normally open: infinite/open normally, near zero ohms when pressed.

## 2. Inputs, no drivers or motor power

Upload the firmware and connect USB only. At 115200 baud it should print `ready`. Run `M119` repeatedly while pressing one input at a time:

```text
M119
x_home:open y_home:open probe:open
ok
```

Each input must change only its own field to `TRIGGERED`. Correct wiring or `*_ACTIVE_LEVEL` before continuing.

## 3. Calibrate the servo safely

The R4 Servo library initially attaches at a standard 1500 µs pulse before accepting the first `M280`. Because the old mechanism's safe range is unknown, perform the first attach with the servo buck **off** (or with the horn/yoke mechanically disconnected):

1. Send `M280 S1000` while the servo has no power. The firmware now holds a 1000 µs signal.
2. Apply servo-buck power with a hand on its cutoff. If the mechanism strains, remove power immediately.
3. Move in small increments such as `M280 S1020`, never jumping blindly across the range. Find a pulse that clears the board everywhere and a center pulse that gives light, reliable contact.
4. Find conservative mechanical minimum and maximum pulse bounds that cannot jam the yoke.
5. Put those values into `PEN_UP_US`, `PEN_DOWN_CENTER_US`, `SERVO_SAFE_MIN_US`, and `SERVO_SAFE_MAX_US` in `config.h`. Set `PEN_CALIBRATED = true`, rebuild, and retest `M5`/`M3` at the center.
6. If the recovered surface correction acts in the wrong direction, change `HEIGHTMAP_SIGN` from `1` to `-1`. Adjust pressure only with `PEN_PRESSURE_OFFSET_US`.

The recovered 915–1045 µs measurements are never used as absolute commands. The firmware subtracts the recovered center value (945 µs) and adds only that relative offset to the newly calibrated center-down pulse.

## 4. Driver and motor bench test

Set and verify each VREF before connecting a motor. With belts disengaged so one motor cannot rack the gantry, use `tools/driver_bench_test/driver_bench_test.ino` to test one socket, driver, and motor at a time. Power everything off before installing/removing a driver or motor. The motor should rotate smoothly in both directions without buzzing, excessive heat, or shutdown.

Restore the main firmware after both channels pass. Reconnect the mechanics and turn both motors by hand with power removed; the gantry should move smoothly without binding.

## 5. Direction checks before homing

Keep a hand on the 24 V cutoff. Confirm both switches still report correctly with `M119`. For the first `G28`, begin with the carriage near the middle and be ready to cut motor power.

The machine must first move left, back off, touch left slowly, release, then repeat downward. If it moves away from a switch, cut 24 V immediately and correct the appropriate direction-inversion constant. If a switch is hit but motion continues, stop and fix the input before trying again.

After a successful home, `M114` should report approximately `X:0.000 Y:0.000 homed:yes microstep:1/16` and both switches should be open because the firmware finishes with clearance from the contacts.

## 6. Mechanical calibration

1. Run ten homing cycles and mark/measure the returned pen position. Investigate any visible spread or missed latch.
2. With the pen raised, command 100 mm X and Y moves and measure actual travel. Update `BASE_UNITS_PER_MM` using `new = old × commanded / measured`.
3. Draw `examples/calibration_square.gcode`, then diagonal lines and a host-generated segmented circle. Check size, squareness, belt tension, and skipped steps.
4. Tune current and acceleration conservatively. Do not increase current to hide mechanical binding.

## 7. Endurance run

Run a representative drawing continuously for 30 minutes. Stop if a driver enters thermal shutdown, the R4 resets, the servo supply sags, wiring or connectors heat up, or the machine skips steps. Only declare the machine commissioned after it completes without any of those symptoms.
