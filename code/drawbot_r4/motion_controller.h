#pragma once

#include <Arduino.h>

#include "machine_types.h"
#include "microstep_math.h"

namespace drawbot {

class PenController;

using IdleCallback = void (*)();

enum class MotionStatus {
  kOk,
  kNotHomed,
  kOutsideWorkspace,
  kInvalidFeed,
  kUnexpectedHomeSwitch,
  kHomeSwitchStuck,
  kHomeTimeout,
};

const char* motionStatusMessage(MotionStatus status);

class MotionController {
 public:
  void begin();
  void setIdleCallback(IdleCallback cb) { idle_callback_ = cb; }
  void pumpIdle() {
    if (idle_callback_ && (++poll_counter_ & 63) == 0) idle_callback_();
  }

  void enableDrivers();
  void disableDrivers();
  bool driversEnabled() const { return drivers_enabled_; }

  MotionStatus moveTo(float x_mm, float y_mm, float feed_mm_min,
                      PenController& pen);
  MotionStatus home(PenController& pen);
  void setPosition(float x_mm, float y_mm);

  PositionMm positionMm() const;
  MicrostepMode microstepMode() const { return microstep_mode_; }
  bool isHomed() const { return homed_; }
  bool xHomeActive() const;
  bool yHomeActive() const;
  bool probeActive() const;

 private:
  MotionStatus runCoordinatedMove(const CartesianSteps& target,
                                  float feed_mm_min, PenController& pen);
  MotionStatus homeAxis(bool x_axis, float travel_mm, uint8_t switch_pin,
                        bool coarse_seek);
  void stepHomingAxis(bool x_axis, int direction, uint32_t period_us);
  void setMicrostepMode(MicrostepMode mode);
  MicrostepMode preferredMode(float path_mm, bool pen_down) const;
  int32_t pulsesForDistance(float distance_mm, MicrostepMode mode) const;
  uint32_t homingPeriodUs(float feed_mm_min, MicrostepMode mode) const;
  void updateMotorPhases(bool step_a, bool step_b, int direction_a,
                         int direction_b);
  void setMotorDirections(int direction_a, int direction_b);
  void pulseMotors(bool step_a, bool step_b, uint32_t period_us);
  bool stableSwitchActive(uint8_t pin) const;
  bool stableSwitchInactive(uint8_t pin) const;
  void delayMicrosecondsLong(uint32_t duration_us) const;
  void failMotion();

  CartesianSteps position_units_{};
  MicrostepMode microstep_mode_ = MicrostepMode::kFull;
  int motor_phase_a_ = 0;
  int motor_phase_b_ = 0;
  bool homed_ = false;
  bool drivers_enabled_ = false;
  IdleCallback idle_callback_ = nullptr;
  uint32_t poll_counter_ = 0;
};

}  // namespace drawbot
