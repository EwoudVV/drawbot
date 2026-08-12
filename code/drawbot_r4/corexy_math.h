#pragma once

#include "machine_types.h"

namespace drawbot {

// CoreXY transform for THIS machine's belt routing. Verified on hardware:
// DIR(H,H) moves +X, DIR(L,H) moves +Y, so motor A advances X-Y and motor B
// advances X+Y (the opposite of the textbook "A advances X+Y" convention;
// adopting the textbook convention here produced Y motion inverted, which
// made homing drive away from the Y- switch).
constexpr MotorSteps cartesianToMotors(const CartesianSteps& cartesian) {
  return {cartesian.x - cartesian.y, cartesian.x + cartesian.y};
}

// Valid CoreXY motor-step pairs always have matching parity, so division by
// two is exact for values produced by cartesianToMotors().
constexpr CartesianSteps motorsToCartesian(const MotorSteps& motors) {
  return {(motors.a + motors.b) / 2, (motors.b - motors.a) / 2};
}

}  // namespace drawbot
