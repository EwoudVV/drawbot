#include "command_processor.h"

#include <math.h>
#include <stdio.h>

#include "command_rules.h"
#include "config.h"
#include "motion_controller.h"
#include "pen_controller.h"

namespace drawbot {
namespace {

void setError(char* error, size_t error_size, const char* message) {
  if (error_size == 0) return;
  snprintf(error, error_size, "%s", message);
}

bool hasAnyArguments(const GcodeCommand& command) {
  return command.has_x || command.has_y || command.has_f || command.has_s;
}

}  // namespace

bool CommandProcessor::requireNoArguments(const GcodeCommand& command,
                                          char* error,
                                          size_t error_size) const {
  if (hasAnyArguments(command)) {
    setError(error, error_size, "this command takes no arguments");
    return false;
  }
  return true;
}

bool CommandProcessor::reportMotionResult(MotionStatus status, char* error,
                                          size_t error_size) const {
  if (status == MotionStatus::kOk) return true;
  setError(error, error_size, motionStatusMessage(status));
  return false;
}

bool CommandProcessor::execute(const GcodeCommand& command, Print& output,
                               char* error, size_t error_size) {
  if (error_size > 0) error[0] = '\0';
  if (command.has_g && command.has_m) {
    setError(error, error_size, "one G or M command is allowed per line");
    return false;
  }
  if (!command.has_g && !command.has_m) {
    setError(error, error_size, "line has arguments but no G or M command");
    return false;
  }
  return command.has_g ? executeG(command, output, error, error_size)
                       : executeM(command, output, error, error_size);
}

bool CommandProcessor::executeG(const GcodeCommand& command, Print& output,
                                char* error, size_t error_size) {
  (void)output;
  if (!isSupportedGCode(command.g)) {
    setError(error, error_size,
             "unsupported G-code (inches, relative moves, and arcs are rejected)");
    return false;
  }
  switch (command.g) {
    case 21:
    case 90:
      return requireNoArguments(command, error, error_size);

    case 0:
    case 1: {
      if (command.has_s) {
        setError(error, error_size, "S is not valid on G0 or G1");
        return false;
      }
      if (!command.has_x && !command.has_y) {
        setError(error, error_size, "G0/G1 requires X and/or Y");
        return false;
      }
      if (!motion_.isHomed()) {
        setError(error, error_size, "machine is not homed; run G28");
        return false;
      }
      if (!pen_.isCalibrated()) {
        setError(error, error_size,
                 "pen is not calibrated; use M280, then update config.h");
        return false;
      }

      const PositionMm current = motion_.positionMm();
      const float target_x = command.has_x ? command.x : current.x;
      const float target_y = command.has_y ? command.y : current.y;
      const float default_feed = command.g == 0 ? config::TRAVEL_FEED_MM_MIN
                                                : config::DRAW_FEED_MM_MIN;
      const float feed = command.has_f ? command.f : default_feed;
      if (!isfinite(feed) || feed <= 0.0f || feed > default_feed) {
        setError(error, error_size,
                 command.g == 0
                     ? "G0 feed must be >0 and <=2000 mm/min"
                     : "G1 feed must be >0 and <=800 mm/min");
        return false;
      }

      if (command.g == 0) pen_.raise();
      const MotionStatus status = motion_.moveTo(target_x, target_y, feed, pen_);
      if (status != MotionStatus::kOk) pen_.forceSafeUp();
      return reportMotionResult(status, error, error_size);
    }

    case 28: {
      if (!requireNoArguments(command, error, error_size)) return false;
      const MotionStatus status = motion_.home(pen_);
      return reportMotionResult(status, error, error_size);
    }

    default:
      setError(error, error_size,
               "unsupported G-code (inches, relative moves, and arcs are rejected)");
      return false;
  }
}

bool CommandProcessor::executeM(const GcodeCommand& command, Print& output,
                                char* error, size_t error_size) {
  if (!isSupportedMCode(command.m)) {
    setError(error, error_size, "unsupported M-code");
    return false;
  }
  switch (command.m) {
    case 3: {
      if (!requireNoArguments(command, error, error_size)) return false;
      if (!motion_.isHomed()) {
        setError(error, error_size, "machine is not homed; run G28");
        return false;
      }
      if (!pen_.lower()) {
        setError(error, error_size,
                 "pen is not calibrated; use M280, then update config.h");
        return false;
      }
      return true;
    }

    case 5:
      if (!requireNoArguments(command, error, error_size)) return false;
      if (!pen_.raise()) {
        setError(error, error_size,
                 "pen is not calibrated; use M280, then update config.h");
        return false;
      }
      return true;

    case 17:
      if (!requireNoArguments(command, error, error_size)) return false;
      motion_.enableDrivers();
      return true;

    case 18:
      if (!requireNoArguments(command, error, error_size)) return false;
      pen_.forceSafeUp();
      motion_.disableDrivers();
      return true;

    case 114: {
      if (!requireNoArguments(command, error, error_size)) return false;
      const PositionMm position = motion_.positionMm();
      output.print(F("X:"));
      output.print(position.x, 3);
      output.print(F(" Y:"));
      output.print(position.y, 3);
      output.print(F(" homed:"));
      output.print(motion_.isHomed() ? F("yes") : F("no"));
      output.print(F(" microstep:"));
      output.println(microstepModeName(motion_.microstepMode()));
      return true;
    }

    case 119:
      if (!requireNoArguments(command, error, error_size)) return false;
      output.print(F("x_home:"));
      output.print(motion_.xHomeActive() ? F("TRIGGERED") : F("open"));
      output.print(F(" y_home:"));
      output.print(motion_.yHomeActive() ? F("TRIGGERED") : F("open"));
      output.print(F(" probe:"));
      output.println(motion_.probeActive() ? F("TRIGGERED") : F("open"));
      return true;

    case 280: {
      if (command.has_x || command.has_y || command.has_f || !command.has_s) {
        setError(error, error_size, "M280 requires only S<pulse_us>");
        return false;
      }
      const int pulse_us = static_cast<int>(lroundf(command.s));
      if (fabsf(command.s - pulse_us) > 0.001f ||
          !pen_.writeCalibrationPulse(pulse_us)) {
        setError(error, error_size,
                 "servo pulse must be an integer inside the configured safety range");
        return false;
      }
      output.print(F("servo_us:"));
      output.println(pulse_us);
      return true;
    }

    case 2:
    case 30: {
      if (!requireNoArguments(command, error, error_size)) return false;
      pen_.forceSafeUp();
      // Program end must NOT auto-home: with a failed or absent limit
      // switch the homing seek would ram the wall. Homing is explicit
      // (G28, or M50 after manually parking the carriage).
      motion_.disableDrivers();
      return true;
    }

    case 50: {
      if (command.has_f || command.has_s) {
        setError(error, error_size, "M50 takes only optional X and Y (mm)");
        return false;
      }
      const float x = command.has_x ? command.x : 0.0f;
      const float y = command.has_y ? command.y : 0.0f;
      // Manually park the carriage at the corner, then tell the firmware
      // where it is. No motion is commanded; the machine is marked homed.
      motion_.setPosition(x, y);
      output.print(F("position set X:"));
      output.print(x, 3);
      output.print(F(" Y:"));
      output.print(y, 3);
      output.println(F(" homed:yes"));
      return true;
    }

    case 100: {
      if (!command.has_s) {
        setError(error, error_size, "M100 requires S<steps>");
        return false;
      }
      int steps = (int)command.s;
      uint32_t period_us = command.has_f ? (uint32_t)command.f : 2000U;
      if (period_us < 100U) period_us = 100U;
      // Match M101's raw pin init exactly
      auto pi = [](uint8_t pin, uint8_t val) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, val);
      };
      pi(config::PIN_MICROSTEP_MS1, HIGH);
      pi(config::PIN_MICROSTEP_MS2, HIGH);
      pi(config::PIN_MICROSTEP_MS3, HIGH);
      // D8 drives the A4988 ENABLE net DIRECTLY (active-low, R1 pull-up to
      // 5 V). Only the configured active level enables the drivers.
      pi(config::PIN_DRIVER_ENABLE, config::DRIVER_ENABLE_ACTIVE_LEVEL);
      bool dir_high = !command.has_x;  // default HIGH (left); X1 = LOW (right)
      auto setDirs = [&dir_high](bool high) {
        dir_high = high;
        pinMode(config::PIN_MOTOR_A_DIR, OUTPUT);
        digitalWrite(config::PIN_MOTOR_A_DIR, high ? HIGH : LOW);
        pinMode(config::PIN_MOTOR_B_DIR, OUTPUT);
        digitalWrite(config::PIN_MOTOR_B_DIR, high ? HIGH : LOW);
      };
      setDirs(dir_high);
      pi(config::PIN_MOTOR_A_STEP, LOW);
      pi(config::PIN_MOTOR_B_STEP, LOW);
      delay(10);
      // Limit-switch bounce: a latch flips direction once per physical press;
      // it stays latched until the switch reads open for a few consecutive
      // steps, so the gantry always gets clear of the lever before re-arming.
      constexpr int kSwitchReleaseSteps = 4;
      bool bouncing = false;
      int release_steps = 0;
      bool last_x = motion_.xHomeActive();
      bool last_y = motion_.yHomeActive();
      uint32_t t0 = micros();
      for (int i = 0; i < steps; i++) {
        const bool x = motion_.xHomeActive();
        const bool y = motion_.yHomeActive();
        if (x != last_x || y != last_y) {
          output.print(F("switch x:"));
          output.print(x ? F("TRIGGERED") : F("open"));
          output.print(F(" y:"));
          output.print(y ? F("TRIGGERED") : F("open"));
          output.print(F(" at step "));
          output.println(i);
          output.flush();
          last_x = x;
          last_y = y;
        }
        const bool pressed = x || y;
        if (bouncing) {
          if (pressed) {
            release_steps = 0;
          } else if (++release_steps >= kSwitchReleaseSteps) {
            bouncing = false;
            release_steps = 0;
          }
        } else if (pressed) {
          bouncing = true;
          setDirs(!dir_high);
          output.print(F("limit bounce at step "));
          output.println(i);
          output.flush();
        }
        digitalWrite(config::PIN_MOTOR_A_STEP, HIGH);
        digitalWrite(config::PIN_MOTOR_B_STEP, HIGH);
        delayMicroseconds(50);
        digitalWrite(config::PIN_MOTOR_A_STEP, LOW);
        digitalWrite(config::PIN_MOTOR_B_STEP, LOW);
        delayMicroseconds(period_us > 50U ? period_us - 50U : 50U);
        if ((i & 511) == 0) {
          output.print(F("step "));
          output.println(i);
          output.flush();
        }
        motion_.pumpIdle();
      }
      uint32_t elapsed = micros() - t0;
      float actual_us = elapsed / (float)steps;
      output.print(F("did "));
      output.print(steps);
      output.print(F(" steps, requested "));
      output.print(period_us);
      output.print(F("us, measured "));
      output.print(actual_us, 1);
      output.print(F("us = "));
      output.print(static_cast<float>(steps) / config::BASE_UNITS_PER_MM, 1);
      output.println(F(" mm"));
      output.flush();
      return true;
    }

    case 101: {
      // Bypass all framework — raw pin control from scratch
      auto p = [](uint8_t pin, uint8_t val) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, val);
      };
      p(config::PIN_MICROSTEP_MS1, HIGH);
      p(config::PIN_MICROSTEP_MS2, HIGH);
      p(config::PIN_MICROSTEP_MS3, HIGH);
      // D8 -> A4988 ENABLE net, direct, active-low (no inverter on the
      // shield; see config.h). Only the active level enables the drivers.
      p(config::PIN_DRIVER_ENABLE, config::DRIVER_ENABLE_ACTIVE_LEVEL);
      bool dir_high = true;
      auto setDirs = [&dir_high](bool high) {
        dir_high = high;
        pinMode(config::PIN_MOTOR_A_DIR, OUTPUT);
        digitalWrite(config::PIN_MOTOR_A_DIR, high ? HIGH : LOW);
        pinMode(config::PIN_MOTOR_B_DIR, OUTPUT);
        digitalWrite(config::PIN_MOTOR_B_DIR, high ? HIGH : LOW);
      };
      setDirs(dir_high);
      p(config::PIN_MOTOR_A_STEP, LOW);
      p(config::PIN_MOTOR_B_STEP, LOW);
      delay(10);
      int n = command.has_s ? (int)command.s : 400;
      constexpr int kSwitchReleaseSteps = 4;
      bool bouncing = false;
      int release_steps = 0;
      bool last_x = motion_.xHomeActive();
      bool last_y = motion_.yHomeActive();
      output.print(F("raw step "));
      output.print(n);
      output.println(F(" starting"));
      output.flush();
      for (int i = 0; i < n; i++) {
        const bool x = motion_.xHomeActive();
        const bool y = motion_.yHomeActive();
        if (x != last_x || y != last_y) {
          output.print(F("switch x:"));
          output.print(x ? F("TRIGGERED") : F("open"));
          output.print(F(" y:"));
          output.print(y ? F("TRIGGERED") : F("open"));
          output.print(F(" at step "));
          output.println(i);
          output.flush();
          last_x = x;
          last_y = y;
        }
        const bool pressed = x || y;
        if (bouncing) {
          if (pressed) {
            release_steps = 0;
          } else if (++release_steps >= kSwitchReleaseSteps) {
            bouncing = false;
            release_steps = 0;
          }
        } else if (pressed) {
          bouncing = true;
          setDirs(!dir_high);
          output.print(F("limit bounce at step "));
          output.println(i);
          output.flush();
        }
        digitalWrite(config::PIN_MOTOR_A_STEP, HIGH);
        digitalWrite(config::PIN_MOTOR_B_STEP, HIGH);
        delayMicroseconds(50);
        digitalWrite(config::PIN_MOTOR_A_STEP, LOW);
        digitalWrite(config::PIN_MOTOR_B_STEP, LOW);
        delayMicroseconds(2000);
        motion_.pumpIdle();
      }
      output.println(F("done"));
      output.flush();
      return true;
    }

    default:
      setError(error, error_size, "unsupported M-code");
      return false;
  }
}

}  // namespace drawbot
