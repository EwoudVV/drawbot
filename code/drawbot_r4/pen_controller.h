#pragma once

#include <Arduino.h>
#include <Servo.h>

namespace drawbot {

class PenController {
 public:
  void begin();

  bool raise();
  bool lower();
  void forceSafeUp();
  void updateForPosition();
  bool writeCalibrationPulse(int pulse_us);

  bool isDown() const { return down_; }
  bool isCalibrated() const;
  int lastPulseUs() const { return last_pulse_us_; }

 private:
  void ensureAttached();
  void writeBoundedPulse(int pulse_us);

  Servo servo_;
  bool attached_ = false;
  bool down_ = false;
  int last_pulse_us_ = 0;
};

}  // namespace drawbot
