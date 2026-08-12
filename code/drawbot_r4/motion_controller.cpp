#include "motion_controller.h"

#include <math.h>
#include <stdlib.h>

#include "config.h"
#include "corexy_math.h"
#include "dda_math.h"
#include "pen_controller.h"
#include "workspace_math.h"

namespace drawbot {
namespace {

int32_t absoluteSteps(int32_t value) { return value < 0 ? -value : value; }

int signOf(int32_t value) {
  if (value > 0) return 1;
  if (value < 0) return -1;
  return 0;
}

}  // namespace

const char* motionStatusMessage(MotionStatus status) {
  switch (status) {
    case MotionStatus::kOk:
      return "ok";
    case MotionStatus::kNotHomed:
      return "machine is not homed; run G28";
    case MotionStatus::kOutsideWorkspace:
      return "target is outside the 0..400 by 0..320 mm workspace";
    case MotionStatus::kInvalidFeed:
      return "feed must be greater than zero and within the configured limit";
    case MotionStatus::kUnexpectedHomeSwitch:
      return "unexpected home switch activation";
    case MotionStatus::kHomeSwitchStuck:
      return "home switch did not release";
    case MotionStatus::kHomeTimeout:
      return "home switch was not reached before the travel limit";
  }
  return "motion failure";
}

void MotionController::begin() {
  // Establish safe output levels. pinMode(OUTPUT) must come FIRST on the
  // Uno R4: calling it after digitalWrite() resets the output data register
  // and clobbers the just-written value (verified empirically).
  pinMode(config::PIN_MOTOR_A_STEP, OUTPUT);
  pinMode(config::PIN_MOTOR_B_STEP, OUTPUT);
  pinMode(config::PIN_MOTOR_A_DIR, OUTPUT);
  pinMode(config::PIN_MOTOR_B_DIR, OUTPUT);
  pinMode(config::PIN_MICROSTEP_MS1, OUTPUT);
  pinMode(config::PIN_MICROSTEP_MS2, OUTPUT);
  pinMode(config::PIN_MICROSTEP_MS3, OUTPUT);
  pinMode(config::PIN_DRIVER_ENABLE, OUTPUT);
  pinMode(config::PIN_HOME_X, INPUT_PULLUP);
  pinMode(config::PIN_HOME_Y, INPUT_PULLUP);
  pinMode(config::PIN_PROBE, INPUT_PULLUP);

  digitalWrite(config::PIN_MOTOR_A_STEP, LOW);
  digitalWrite(config::PIN_MOTOR_B_STEP, LOW);
  digitalWrite(config::PIN_MICROSTEP_MS1, LOW);
  digitalWrite(config::PIN_MICROSTEP_MS2, LOW);
  digitalWrite(config::PIN_MICROSTEP_MS3, LOW);
  digitalWrite(config::PIN_DRIVER_ENABLE,
               config::DRIVER_ENABLE_ACTIVE_LEVEL == LOW ? HIGH : LOW);
  digitalWrite(config::PIN_MOTOR_A_DIR, LOW);
  digitalWrite(config::PIN_MOTOR_B_DIR, LOW);

  setMicrostepMode(MicrostepMode::kFull);
  disableDrivers();
}

void MotionController::enableDrivers() {
  pinMode(config::PIN_DRIVER_ENABLE, OUTPUT);
  digitalWrite(config::PIN_DRIVER_ENABLE, config::DRIVER_ENABLE_ACTIVE_LEVEL);
  drivers_enabled_ = true;
  delay(2);
}

void MotionController::disableDrivers() {
  pinMode(config::PIN_DRIVER_ENABLE, OUTPUT);
  digitalWrite(config::PIN_DRIVER_ENABLE,
               config::DRIVER_ENABLE_ACTIVE_LEVEL == LOW ? HIGH : LOW);
  drivers_enabled_ = false;
  homed_ = false;
}

PositionMm MotionController::positionMm() const {
  return {position_units_.x / config::BASE_UNITS_PER_MM,
          position_units_.y / config::BASE_UNITS_PER_MM};
}

bool MotionController::xHomeActive() const {
  return digitalRead(config::PIN_HOME_X) == config::HOME_SWITCH_ACTIVE_LEVEL;
}

bool MotionController::yHomeActive() const {
  return digitalRead(config::PIN_HOME_Y) == config::HOME_SWITCH_ACTIVE_LEVEL;
}

bool MotionController::probeActive() const {
  return digitalRead(config::PIN_PROBE) == config::PROBE_ACTIVE_LEVEL;
}

void MotionController::setMicrostepMode(MicrostepMode mode) {
  bool ms1 = false;
  bool ms2 = false;
  bool ms3 = false;
  switch (mode) {
    case MicrostepMode::kFull:
      break;
    case MicrostepMode::kHalf:
      ms1 = true;
      break;
    case MicrostepMode::kQuarter:
      ms2 = true;
      break;
    case MicrostepMode::kEighth:
      ms1 = true;
      ms2 = true;
      break;
    case MicrostepMode::kSixteenth:
      ms1 = true;
      ms2 = true;
      ms3 = true;
      break;
  }
  pinMode(config::PIN_MICROSTEP_MS1, OUTPUT);
  digitalWrite(config::PIN_MICROSTEP_MS1, ms1 ? HIGH : LOW);
  pinMode(config::PIN_MICROSTEP_MS2, OUTPUT);
  digitalWrite(config::PIN_MICROSTEP_MS2, ms2 ? HIGH : LOW);
  pinMode(config::PIN_MICROSTEP_MS3, OUTPUT);
  digitalWrite(config::PIN_MICROSTEP_MS3, ms3 ? HIGH : LOW);
  microstep_mode_ = mode;
  delayMicroseconds(2);
}

MicrostepMode MotionController::preferredMode(float path_mm,
                                              bool pen_down) const {
  (void)path_mm;
  (void)pen_down;
  // Forced to 1/16 microstepping: positions are tracked in 1/16-step base
  // units and the recovered calibration (40.4 pulses/mm at 1/8) is only
  // consistent at 80.8 base units/mm, i.e. 1/16 stepping.
  // NOTE: the old claim that MS3/D13 must stay HIGH to release an A4988
  // "RESET rail" on this shield is refuted — the official Protoneer V3.00
  // design has no D13-to-RESET connection (there is no R10); D12/D13 are the
  // SpnEn/SpnDir headers. MS pins only select stepping resolution; they
  // cannot disable the drivers. If 1/16 stepping is ever verified missing on
  // the actual MS pads, re-tune BASE_UNITS_PER_MM instead of reviving that
  // theory.
  return MicrostepMode::kSixteenth;
}

int32_t MotionController::pulsesForDistance(float distance_mm,
                                            MicrostepMode mode) const {
  const float pulses_per_mm =
      config::BASE_UNITS_PER_MM / baseUnitsPerPulse(mode);
  return static_cast<int32_t>(ceilf(distance_mm * pulses_per_mm));
}

uint32_t MotionController::homingPeriodUs(float feed_mm_min,
                                          MicrostepMode mode) const {
  const float pulses_per_second =
      feed_mm_min * config::BASE_UNITS_PER_MM /
      (60.0f * baseUnitsPerPulse(mode));
  return static_cast<uint32_t>(lroundf(1000000.0f / pulses_per_second));
}

void MotionController::delayMicrosecondsLong(uint32_t duration_us) const {
  while (duration_us > 16000U) {
    delayMicroseconds(16000U);
    duration_us -= 16000U;
  }
  if (duration_us > 0U) delayMicroseconds(duration_us);
}

void MotionController::setMotorDirections(int direction_a, int direction_b) {
  const bool a_positive = direction_a >= 0;
  const bool b_positive = direction_b >= 0;
  pinMode(config::PIN_MOTOR_A_DIR, OUTPUT);
  digitalWrite(config::PIN_MOTOR_A_DIR,
               (a_positive != config::MOTOR_A_DIRECTION_INVERTED) ? HIGH
                                                                  : LOW);
  pinMode(config::PIN_MOTOR_B_DIR, OUTPUT);
  digitalWrite(config::PIN_MOTOR_B_DIR,
               (b_positive != config::MOTOR_B_DIRECTION_INVERTED) ? HIGH
                                                                  : LOW);
}

void MotionController::pulseMotors(bool step_a, bool step_b,
                                   uint32_t period_us) {
  if (step_a) digitalWrite(config::PIN_MOTOR_A_STEP, HIGH);
  if (step_b) digitalWrite(config::PIN_MOTOR_B_STEP, HIGH);
  delayMicroseconds(config::STEP_PULSE_HIGH_US);
  if (step_a) digitalWrite(config::PIN_MOTOR_A_STEP, LOW);
  if (step_b) digitalWrite(config::PIN_MOTOR_B_STEP, LOW);

  if (period_us > config::STEP_PULSE_HIGH_US) {
    delayMicrosecondsLong(period_us - config::STEP_PULSE_HIGH_US);
  }
}

void MotionController::updateMotorPhases(bool step_a, bool step_b,
                                         int direction_a, int direction_b) {
  if (step_a) {
    motor_phase_a_ =
        advancePhase(motor_phase_a_, direction_a, microstep_mode_);
  }
  if (step_b) {
    motor_phase_b_ =
        advancePhase(motor_phase_b_, direction_b, microstep_mode_);
  }
}

bool MotionController::stableSwitchActive(uint8_t pin) const {
  if (digitalRead(pin) != config::HOME_SWITCH_ACTIVE_LEVEL) return false;
  delay(config::SWITCH_DEBOUNCE_MS);
  return digitalRead(pin) == config::HOME_SWITCH_ACTIVE_LEVEL;
}

bool MotionController::stableSwitchInactive(uint8_t pin) const {
  if (digitalRead(pin) == config::HOME_SWITCH_ACTIVE_LEVEL) return false;
  delay(config::SWITCH_DEBOUNCE_MS);
  return digitalRead(pin) != config::HOME_SWITCH_ACTIVE_LEVEL;
}

void MotionController::failMotion() {
  homed_ = false;
  disableDrivers();
}

void MotionController::setPosition(float x_mm, float y_mm) {
  position_units_ = {
      static_cast<int32_t>(lroundf(x_mm * config::BASE_UNITS_PER_MM)),
      static_cast<int32_t>(lroundf(y_mm * config::BASE_UNITS_PER_MM))};
  homed_ = true;
}

MotionStatus MotionController::moveTo(float x_mm, float y_mm,
                                      float feed_mm_min,
                                      PenController& pen) {
  if (!homed_) return MotionStatus::kNotHomed;
  if (!isWithinWorkspace(x_mm, y_mm, config::X_MAX_MM, config::Y_MAX_MM)) {
    return MotionStatus::kOutsideWorkspace;
  }
  if (!isfinite(feed_mm_min) || feed_mm_min <= 0.0f ||
      feed_mm_min > config::MAX_FEED_MM_MIN) {
    return MotionStatus::kInvalidFeed;
  }

  const CartesianSteps target = {
      static_cast<int32_t>(lroundf(x_mm * config::BASE_UNITS_PER_MM)),
      static_cast<int32_t>(lroundf(y_mm * config::BASE_UNITS_PER_MM))};
  return runCoordinatedMove(target, feed_mm_min, pen);
}

MotionStatus MotionController::runCoordinatedMove(
    const CartesianSteps& target, float feed_mm_min, PenController& pen) {
  const CartesianSteps delta = {target.x - position_units_.x,
                                target.y - position_units_.y};
  const MotorSteps motor_delta = cartesianToMotors(delta);
  const float dx_mm = delta.x / config::BASE_UNITS_PER_MM;
  const float dy_mm = delta.y / config::BASE_UNITS_PER_MM;
  const float path_mm = sqrtf(dx_mm * dx_mm + dy_mm * dy_mm);
  if (path_mm == 0.0f) {
    return MotionStatus::kOk;
  }

  const MicrostepMode selected = selectCompatibleMode(
      motor_delta, motor_phase_a_, motor_phase_b_,
      preferredMode(path_mm, pen.isDown()));
  setMicrostepMode(selected);
  const int quantum = baseUnitsPerPulse(selected);
  const int32_t count_a = absoluteSteps(motor_delta.a) / quantum;
  const int32_t count_b = absoluteSteps(motor_delta.b) / quantum;
  const int32_t event_count = count_a > count_b ? count_a : count_b;

  if (!drivers_enabled_) enableDrivers();
  if (xHomeActive() || yHomeActive()) {
    failMotion();
    return MotionStatus::kUnexpectedHomeSwitch;
  }

  const int direction_a = signOf(motor_delta.a);
  const int direction_b = signOf(motor_delta.b);
  setMotorDirections(direction_a, direction_b);
  delayMicroseconds(2);

  const float distance_per_event = path_mm / event_count;
  const float nominal_speed = feed_mm_min / 60.0f;
  const float minimum_speed =
      nominal_speed < config::MIN_RAMP_SPEED_MM_S
          ? nominal_speed
          : config::MIN_RAMP_SPEED_MM_S;
  DdaState dda{count_a, count_b, event_count};

  for (int32_t event = 0; event < event_count; ++event) {
    if (xHomeActive() || yHomeActive()) {
      failMotion();
      return MotionStatus::kUnexpectedHomeSwitch;
    }

    const DdaStep step = nextDdaStep(dda);
    const float travelled_mm = (event + 1) * distance_per_event;
    const float remaining_mm = path_mm - travelled_mm;
    const float accelerating = sqrtf(minimum_speed * minimum_speed +
                                     2.0f * config::ACCELERATION_MM_S2 *
                                         travelled_mm);
    const float decelerating = sqrtf(minimum_speed * minimum_speed +
                                     2.0f * config::ACCELERATION_MM_S2 *
                                         (remaining_mm > 0.0f ? remaining_mm
                                                              : 0.0f));
    float speed = nominal_speed;
    if (speed > accelerating) speed = accelerating;
    if (speed > decelerating) speed = decelerating;
    if (speed < minimum_speed) speed = minimum_speed;
    uint32_t period_us = static_cast<uint32_t>(
        lroundf(1000000.0f * distance_per_event / speed));
    if (period_us <= config::STEP_PULSE_HIGH_US) {
      period_us = config::STEP_PULSE_HIGH_US + 1U;
    }

    pulseMotors(step.motor_a, step.motor_b, period_us);
    updateMotorPhases(step.motor_a, step.motor_b, direction_a, direction_b);

    if (idle_callback_ && (++poll_counter_ & 63) == 0) idle_callback_();
  }

  position_units_ = target;
  return MotionStatus::kOk;
}

void MotionController::stepHomingAxis(bool x_axis, int direction,
                                      uint32_t period_us) {
  // Match the machine's CoreXY convention (see corexy_math.h): X motion is
  // A=B, Y motion is A=-B with Y = (B-A)/2.
  const int direction_a = x_axis ? direction : -direction;
  const int direction_b = direction;
  setMotorDirections(direction_a, direction_b);
  delayMicroseconds(2);
  pulseMotors(true, true, period_us);
  updateMotorPhases(true, true, direction_a, direction_b);
}

MotionStatus MotionController::homeAxis(bool x_axis, float travel_mm,
                                        uint8_t switch_pin,
                                        bool coarse_seek) {
  // Always 1/16 (see preferredMode): positions are 1/16 base units.
  const MicrostepMode seek_mode = MicrostepMode::kSixteenth;
  setMicrostepMode(seek_mode);
  uint32_t seek_period =
      homingPeriodUs(config::HOME_SEEK_FEED_MM_MIN, seek_mode);
  int32_t release_limit = pulsesForDistance(
      config::HOME_BACKOFF_MM + config::HOME_FINAL_RELEASE_MM, seek_mode);

  auto poll = [this] { if (idle_callback_ && (++poll_counter_ & 63) == 0) idle_callback_(); };

  if (digitalRead(switch_pin) == config::HOME_SWITCH_ACTIVE_LEVEL) {
    bool released = false;
    for (int32_t i = 0; i < release_limit; ++i) {
      stepHomingAxis(x_axis, 1, seek_period);
      poll();
      if (stableSwitchInactive(switch_pin)) {
        released = true;
        break;
      }
    }
    if (!released) return MotionStatus::kHomeSwitchStuck;
  }

  const int32_t search_steps = pulsesForDistance(
      travel_mm + config::HOME_SEARCH_MARGIN_MM, seek_mode);
  bool found = false;
  for (int32_t i = 0; i < search_steps; ++i) {
    if (!x_axis && xHomeActive()) {
      return MotionStatus::kUnexpectedHomeSwitch;
    }
    stepHomingAxis(x_axis, -1, seek_period);
    poll();
    if (stableSwitchActive(switch_pin)) {
      found = true;
      break;
    }
  }
  if (!found) return MotionStatus::kHomeTimeout;

  const int32_t backoff_steps =
      pulsesForDistance(config::HOME_BACKOFF_MM, seek_mode);
  for (int32_t i = 0; i < backoff_steps; ++i) {
    if (!x_axis && xHomeActive()) {
      return MotionStatus::kUnexpectedHomeSwitch;
    }
    stepHomingAxis(x_axis, 1, seek_period);
    poll();
  }
  if (!stableSwitchInactive(switch_pin)) {
    return MotionStatus::kHomeSwitchStuck;
  }

  if (coarse_seek) {
    motor_phase_a_ = 0;
    motor_phase_b_ = 0;
  }
  setMicrostepMode(MicrostepMode::kSixteenth);
  const uint32_t latch_period = homingPeriodUs(
      config::HOME_LATCH_FEED_MM_MIN, MicrostepMode::kSixteenth);
  release_limit = pulsesForDistance(
      config::HOME_BACKOFF_MM + config::HOME_FINAL_RELEASE_MM,
      MicrostepMode::kSixteenth);

  found = false;
  for (int32_t i = 0; i < release_limit; ++i) {
    if (!x_axis && xHomeActive()) {
      return MotionStatus::kUnexpectedHomeSwitch;
    }
    stepHomingAxis(x_axis, -1, latch_period);
    poll();
    if (stableSwitchActive(switch_pin)) {
      found = true;
      break;
    }
  }
  if (!found) return MotionStatus::kHomeTimeout;

  bool released = false;
  for (int32_t i = 0; i < release_limit; ++i) {
    if (!x_axis && xHomeActive()) {
      return MotionStatus::kUnexpectedHomeSwitch;
    }
    stepHomingAxis(x_axis, 1, latch_period);
    poll();
    if (stableSwitchInactive(switch_pin)) {
      released = true;
      break;
    }
  }
  if (!released) return MotionStatus::kHomeSwitchStuck;

  const int32_t clearance_steps = pulsesForDistance(
      config::HOME_FINAL_RELEASE_MM, MicrostepMode::kSixteenth);
  for (int32_t i = 0; i < clearance_steps; ++i) {
    if (!x_axis && xHomeActive()) {
      return MotionStatus::kUnexpectedHomeSwitch;
    }
    stepHomingAxis(x_axis, 1, latch_period);
    poll();
  }
  return MotionStatus::kOk;
}

MotionStatus MotionController::home(PenController& pen) {
  homed_ = false;
  pen.forceSafeUp();

  // 1/16 microstepping: positions are tracked in 1/16 base units (see
  // preferredMode). MS pins select resolution only; they do not touch any
  // driver reset rail on this shield.
  setMicrostepMode(MicrostepMode::kSixteenth);
  motor_phase_a_ = 0;
  motor_phase_b_ = 0;
  enableDrivers();

  MotionStatus status =
      homeAxis(true, config::X_MAX_MM, config::PIN_HOME_X, true);
  if (status != MotionStatus::kOk) {
    failMotion();
    return status;
  }

  status = homeAxis(false, config::Y_MAX_MM, config::PIN_HOME_Y, false);
  if (status != MotionStatus::kOk) {
    failMotion();
    return status;
  }

  position_units_ = {0, 0};
  homed_ = true;
  return MotionStatus::kOk;
}

}  // namespace drawbot
