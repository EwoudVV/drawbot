#include "pen_controller.h"

#include "config.h"

namespace drawbot {
namespace {

int clampPulse(int pulse_us) {
  if (pulse_us < config::SERVO_SAFE_MIN_US) return config::SERVO_SAFE_MIN_US;
  if (pulse_us > config::SERVO_SAFE_MAX_US) return config::SERVO_SAFE_MAX_US;
  return pulse_us;
}

}  // namespace

void PenController::begin() {
  down_ = false;
  if (config::PEN_CALIBRATED) {
    ensureAttached();
    writeBoundedPulse(config::PEN_UP_US);
  }
}

bool PenController::isCalibrated() const { return config::PEN_CALIBRATED; }

void PenController::ensureAttached() {
  if (!attached_) {
    servo_.attach(config::PIN_SERVO, config::SERVO_SAFE_MIN_US,
                  config::SERVO_SAFE_MAX_US);
    attached_ = true;
  }
}

void PenController::writeBoundedPulse(int pulse_us) {
  ensureAttached();
  last_pulse_us_ = clampPulse(pulse_us);
  servo_.writeMicroseconds(last_pulse_us_);
}

bool PenController::raise() {
  if (!isCalibrated()) return false;
  writeBoundedPulse(config::PEN_UP_US);
  down_ = false;
  delay(config::SERVO_SETTLE_MS);
  return true;
}

bool PenController::lower() {
  if (!isCalibrated()) return false;
  writeBoundedPulse(clampPulse(config::PEN_DOWN_CENTER_US +
                               config::PEN_PRESSURE_OFFSET_US));
  down_ = true;
  delay(config::SERVO_SETTLE_MS);
  return true;
}

void PenController::forceSafeUp() {
  down_ = false;
  if (isCalibrated()) writeBoundedPulse(config::PEN_UP_US);
}

void PenController::updateForPosition() {
  if (down_ && isCalibrated()) {
    writeBoundedPulse(clampPulse(config::PEN_DOWN_CENTER_US +
                                 config::PEN_PRESSURE_OFFSET_US));
  }
}

bool PenController::writeCalibrationPulse(int pulse_us) {
  if (pulse_us < config::SERVO_SAFE_MIN_US ||
      pulse_us > config::SERVO_SAFE_MAX_US) {
    return false;
  }
  down_ = false;
  writeBoundedPulse(pulse_us);
  return true;
}

}  // namespace drawbot
