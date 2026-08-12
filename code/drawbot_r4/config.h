#pragma once

#include <Arduino.h>

namespace drawbot::config {

// CNC Shield V3.00 / GRBL 0.8 pin layout.
constexpr uint8_t PIN_MOTOR_A_STEP = 2;  // X socket STEP
constexpr uint8_t PIN_MOTOR_B_STEP = 3;  // Y socket STEP
constexpr uint8_t PIN_SERVO = 4;         // Unused Z.STEP breakout
constexpr uint8_t PIN_MOTOR_A_DIR = 5;   // X socket DIR
constexpr uint8_t PIN_MOTOR_B_DIR = 6;   // Y socket DIR
constexpr uint8_t PIN_MICROSTEP_MS1 = 7;   // Empty Z socket DIR
constexpr uint8_t PIN_DRIVER_ENABLE = 8;
constexpr uint8_t PIN_HOME_X = 9;   // X- end-stop header
constexpr uint8_t PIN_HOME_Y = 10;  // Y- end-stop header
constexpr uint8_t PIN_PROBE = 11;   // Z- end-stop header
constexpr uint8_t PIN_MICROSTEP_MS2 = 12;  // SpnEn
constexpr uint8_t PIN_MICROSTEP_MS3 = 13;  // SpnDir

constexpr bool MOTOR_A_DIRECTION_INVERTED = false;
constexpr bool MOTOR_B_DIRECTION_INVERTED = false;
// D8 is the shield's ENABLE net. On the CNC Shield V3.00 it connects DIRECTLY
// to every A4988 ENABLE pin (no inverter) and R1 (10K) pulls the net to 5 V,
// so the net idles high and the drivers are OFF unless D8 is pulled LOW
// (A4988 ENABLE is active-low). grbl 0.8, which this shield is built for,
// writes D8 LOW to enable (st_wake_up) and HIGH to disable (st_go_idle).
// Verified against the Protoneer V3.00 schematic and empirically on this
// machine: old firmware left D8 LOW and the motors moved; after this constant
// was set HIGH the motors went completely dead (no holding torque).
constexpr uint8_t DRIVER_ENABLE_ACTIVE_LEVEL = LOW;

// Existing switches and the temporary button probe close to ground.
constexpr uint8_t HOME_SWITCH_ACTIVE_LEVEL = LOW;
constexpr uint8_t PROBE_ACTIVE_LEVEL = LOW;

constexpr uint32_t SERIAL_BAUD = 115200;
constexpr size_t SERIAL_LINE_MAX = 96;

constexpr float X_MAX_MM = 400.0f;
constexpr float Y_MAX_MM = 320.0f;
// Canonical position units are 1/16 microsteps. The recovered calibration was
// 40.4 pulses/mm at 1/8, hence 80.8 base units/mm at 1/16.
constexpr float BASE_UNITS_PER_MM = 80.8f;

// Pen-up moves always request the coarsest electrically compatible mode.
// Pen-down lines use their length as a useful proxy: long G1 segments are
// straight features, while curves and detailed artwork arrive as short G1s.
constexpr float DRAW_FULL_STEP_MIN_MM = 30.0f;
constexpr float DRAW_HALF_STEP_MIN_MM = 15.0f;
constexpr float DRAW_QUARTER_STEP_MIN_MM = 7.0f;
constexpr float DRAW_EIGHTH_STEP_MIN_MM = 2.0f;

constexpr float TRAVEL_FEED_MM_MIN = 2000.0f;
constexpr float DRAW_FEED_MM_MIN = 800.0f;
constexpr float HOME_SEEK_FEED_MM_MIN = 600.0f;
constexpr float HOME_LATCH_FEED_MM_MIN = 150.0f;
constexpr float MAX_FEED_MM_MIN = TRAVEL_FEED_MM_MIN;
constexpr float ACCELERATION_MM_S2 = 200.0f;
constexpr float MIN_RAMP_SPEED_MM_S = 2.0f;

constexpr uint16_t STEP_PULSE_HIGH_US = 3;
constexpr float HOME_SEARCH_MARGIN_MM = 25.0f;
constexpr float HOME_BACKOFF_MM = 3.0f;
constexpr float HOME_FINAL_RELEASE_MM = 1.0f;
constexpr uint16_t SWITCH_DEBOUNCE_MS = 8;

// The old sketch and recovered map disagree about absolute servo direction.
// Keep this false until M280 has been used to find safe values, then edit the
// pulse constants and safety bounds below, then set PEN_CALIBRATED to true.
// Calibrated on-device: 500 us = tip touching paper (servo at its lowest),
// 1000 us = raised with good clearance. Verified visually during a full
// pulse sweep after adjusting the pen holder.
constexpr bool PEN_CALIBRATED = true;
constexpr int PEN_UP_US = 1500;
constexpr int PEN_DOWN_CENTER_US = 500;
constexpr int PEN_PRESSURE_OFFSET_US = 0;
constexpr int SERVO_SAFE_MIN_US = 500;
constexpr int SERVO_SAFE_MAX_US = 2200;
constexpr uint16_t SERVO_SETTLE_MS = 250;

}  // namespace drawbot::config
