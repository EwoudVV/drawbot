#pragma once

#include <stdint.h>

namespace drawbot {

struct CartesianSteps {
  int32_t x = 0;
  int32_t y = 0;
};

struct MotorSteps {
  int32_t a = 0;
  int32_t b = 0;
};

struct PositionMm {
  float x = 0.0f;
  float y = 0.0f;
};

}  // namespace drawbot
