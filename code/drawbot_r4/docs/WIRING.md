# Wiring and power guide

Disconnect USB, 24 V, and the servo buck before changing any wire, driver, motor, or microstep jumper. Never plug or unplug a stepper while its driver is powered.

## Signal assignment

| Function | CNC Shield location | Uno R4 pin | Firmware name |
|---|---|---:|---|
| CoreXY motor A | X driver socket | STEP D2, DIR D5 | `PIN_MOTOR_A_*` |
| CoreXY motor B | Y driver socket | STEP D3, DIR D6 | `PIN_MOTOR_B_*` |
| Shared A4988 MS1 | Empty Z socket DIR | D7 | `PIN_MICROSTEP_MS1` |
| Shared driver enable | Shield enable net | D8, active-low | `PIN_DRIVER_ENABLE` |
| Left home switch | X− end-stop signal | D9 | `PIN_HOME_X` |
| Bottom home switch | Y− end-stop signal | D10 | `PIN_HOME_Y` |
| Removable probe button | Z− end-stop signal | D11 | `PIN_PROBE` |
| Servo signal only | Unused Z.STEP breakout | D4 | `PIN_SERVO` |
| Shared A4988 MS2 | SpnEn | D12 | `PIN_MICROSTEP_MS2` |
| Shared A4988 MS3 | SpnDir | D13 | `PIN_MICROSTEP_MS3` |

The home switches and probe are configured as `INPUT_PULLUP`: connect each normally-open contact between its signal pin and GND. It should read `open` normally and `TRIGGERED` when pressed. Use the shield labels or a continuity meter to identify signal and GND; do not guess the position of a three-pin header and do not connect a switch to 5 V.

## Power domains

```text
Mains ── enclosed/fused 24 V supply ── emergency cutoff ── CNC Shield VMOT
                                                   └───── 24→5 V buck ── servo +5 V
USB-C ─────────────────────────────────────────────────── Uno R4 logic power

Servo GND ───────── buck GND ───────── shield GND ─────── Uno R4 GND
Servo signal ───────────────────────────────────────────── D4 / Z.STEP
```

- Connect 24 V only to the shield motor-power terminal, with polarity checked before energising it.
- Power the R4 from USB-C for version 1.
- Power the servo only from the 5 V buck. The buck must have enough surge current for the servo; 2 A or more is a sensible starting point for a typical hobby servo.
- Join all grounds. Do **not** join the buck's +5 V output to the R4/shield 5 V rail.
- Put at least 100 µF, preferably a 50 V-rated electrolytic, across VMOT and GND near the drivers. Put 470–1000 µF across the servo's buck output near the servo. Observe capacitor polarity.
- Fuse the 24 V branch, cover mains terminals, provide an accessible motor-power cutoff, and connect protective earth to exposed metal/enclosures where the power supply's instructions require it.

The R4 accepts 6–24 V on VIN, but 24 V is the stated upper limit and the motor rail is electrically noisy. Its datasheet also calls for external servo power. Keeping USB logic power and the buck is the safer arrangement.

## A4988 setup

1. Remove the three microstep jumper headers under both X and Y sockets. With the shield unpowered and drivers removed, identify the control-side pad of each pair by continuity to the corresponding A4988 socket MS pin. The opposite pad is +5 V and must remain isolated.
2. Wire D7/Z-DIR directly to the X and Y MS1 control pads; D12/SpnEn to both MS2 pads; and D13/SpnDir to both MS3 pads. The A4988 provides internal pull-downs, so no external resistors are required. Verify that none of these three nets has continuity to +5 V before reinstalling drivers.
3. Identify each driver's `VMOT`, `GND`, `VDD`, `1A`, and `1B` labels and match those to the socket labels. Clone board colours and potentiometer positions are not reliable orientation guides.
4. Identify each motor's two coil pairs with a continuity meter. One coil goes to 1A/1B and the other to 2A/2B. A motor that only buzzes usually has interleaved coil wires.
5. Set current with the motor disconnected and motor power applied, using an insulated screwdriver and meter. For the common A4988 relation, `I_limit = VREF / (8 × R_sense)`. Thus 0.8 A is about 0.64 V with `R100` (0.10 Ω) resistors or 0.32 V with `R050` (0.05 Ω) resistors. Confirm the actual resistor marking and carrier documentation before relying on those examples.
6. Start near 0.8 A/phase. Do not exceed roughly 1 A without suitable heatsinks and airflow even though the motors are rated higher.

The mode truth table is full=`000`, half=`100`, quarter=`010`, eighth=`110`, and sixteenth=`111` for MS1/MS2/MS3. Firmware changes mode only at an electrical position common to both the old and new resolutions.

If an axis moves opposite the expected direction, change `MOTOR_A_DIRECTION_INVERTED` or `MOTOR_B_DIRECTION_INVERTED` in `config.h` after determining which motor is reversed. Always remove power before swapping motor wires.

## Servo connection

The servo's signal lead goes to D4/Z.STEP; its power and ground go to the buck. Do not plug all three servo leads into an arbitrary shield header. The R4 Servo library can drive D4, while the Z driver socket remains empty.
