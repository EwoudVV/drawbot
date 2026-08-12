#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "../command_rules.h"
#include "../corexy_math.h"
#include "../dda_math.h"
#include "../gcode_parser.h"
#include "../microstep_math.h"
#include "../workspace_math.h"

namespace {

void testCoreXyTransform() {
  // This machine's belt routing: A advances X-Y, B advances X+Y (verified on
  // hardware: DIR(H,H) moves +X, DIR(L,H) moves +Y). See corexy_math.h.
  const drawbot::CartesianSteps cartesian{100, 50};
  const drawbot::MotorSteps motors = drawbot::cartesianToMotors(cartesian);
  assert(motors.a == 50);
  assert(motors.b == 150);
  const drawbot::CartesianSteps round_trip =
      drawbot::motorsToCartesian(motors);
  assert(round_trip.x == cartesian.x);
  assert(round_trip.y == cartesian.y);

  const drawbot::MotorSteps pure_y =
      drawbot::cartesianToMotors(drawbot::CartesianSteps{0, 80});
  assert(pure_y.a == -80 && pure_y.b == 80);
}

void verifyDdaCounts(int32_t count_a, int32_t count_b) {
  const int32_t event_count = count_a > count_b ? count_a : count_b;
  drawbot::DdaState dda{count_a, count_b, event_count};
  int32_t actual_a = 0;
  int32_t actual_b = 0;
  for (int32_t i = 0; i < event_count; ++i) {
    const drawbot::DdaStep step = drawbot::nextDdaStep(dda);
    actual_a += step.motor_a ? 1 : 0;
    actual_b += step.motor_b ? 1 : 0;
  }
  assert(actual_a == count_a);
  assert(actual_b == count_b);
}

void testDda() {
  verifyDdaCounts(100, 100);
  verifyDdaCounts(100, 37);
  verifyDdaCounts(1, 17);
  verifyDdaCounts(409, 0);
}

void testDynamicMicrostepping() {
  using drawbot::MicrostepMode;
  assert(drawbot::baseUnitsPerPulse(MicrostepMode::kFull) == 16);
  assert(drawbot::baseUnitsPerPulse(MicrostepMode::kEighth) == 2);
  assert(drawbot::baseUnitsPerPulse(MicrostepMode::kSixteenth) == 1);

  const drawbot::MotorSteps full_move{160, -32};
  assert(drawbot::selectCompatibleMode(full_move, 0, 0,
                                       MicrostepMode::kFull) ==
         MicrostepMode::kFull);

  const drawbot::MotorSteps eighth_move{18, -34};
  assert(drawbot::selectCompatibleMode(eighth_move, 2, 14,
                                       MicrostepMode::kFull) ==
         MicrostepMode::kEighth);

  const drawbot::MotorSteps fine_move{7, -5};
  assert(drawbot::selectCompatibleMode(fine_move, 1, 15,
                                       MicrostepMode::kFull) ==
         MicrostepMode::kSixteenth);

  int phase = 0;
  phase = drawbot::advancePhase(phase, 1, MicrostepMode::kEighth);
  assert(phase == 2);
  phase = drawbot::advancePhase(phase, -1, MicrostepMode::kEighth);
  assert(phase == 0);
  phase = drawbot::advancePhase(0, -1, MicrostepMode::kSixteenth);
  assert(phase == 15);
}

void testParserAndRules() {
  drawbot::GcodeCommand command;
  assert(drawbot::parseGcodeLine("G1 X12.5 Y-2 F800 ; draw", command) ==
         drawbot::ParseStatus::kOk);
  assert(command.has_g && command.g == 1);
  assert(command.has_x && fabsf(command.x - 12.5f) < 0.0001f);
  assert(command.has_y && fabsf(command.y + 2.0f) < 0.0001f);
  assert(command.has_f && fabsf(command.f - 800.0f) < 0.0001f);

  assert(drawbot::parseGcodeLine("M280 S1200", command) ==
         drawbot::ParseStatus::kOk);
  assert(command.has_m && command.m == 280 && command.has_s);
  assert(drawbot::parseGcodeLine("G1 X1 X2", command) ==
         drawbot::ParseStatus::kDuplicateWord);
  assert(drawbot::parseGcodeLine("G1 Xwat", command) ==
         drawbot::ParseStatus::kInvalidNumber);
  assert(drawbot::parseGcodeLine("G1 (unfinished", command) ==
         drawbot::ParseStatus::kUnterminatedComment);
  assert(drawbot::parseGcodeLine("Q7", command) ==
         drawbot::ParseStatus::kUnsupportedWord);

  assert(drawbot::isSupportedGCode(0));
  assert(drawbot::isSupportedGCode(28));
  assert(!drawbot::isSupportedGCode(2));
  assert(!drawbot::isSupportedGCode(20));
  assert(!drawbot::isSupportedGCode(91));
  assert(drawbot::isSupportedMCode(119));
  assert(!drawbot::isSupportedMCode(4));
}

void testSoftLimits() {
  assert(drawbot::isWithinWorkspace(0.0f, 0.0f, 400.0f, 320.0f));
  assert(drawbot::isWithinWorkspace(400.0f, 320.0f, 400.0f, 320.0f));
  assert(!drawbot::isWithinWorkspace(-0.01f, 10.0f, 400.0f, 320.0f));
  assert(!drawbot::isWithinWorkspace(10.0f, 320.01f, 400.0f, 320.0f));
  assert(!drawbot::isWithinWorkspace(NAN, 10.0f, 400.0f, 320.0f));
}

}  // namespace

int main() {
  testCoreXyTransform();
  testDda();
  testDynamicMicrostepping();
  testParserAndRules();
  testSoftLimits();
  puts("All firmware-core tests passed.");
  return 0;
}
