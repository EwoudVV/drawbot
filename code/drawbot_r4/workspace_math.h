#pragma once

#include <math.h>

namespace drawbot {

inline bool isFiniteCoordinate(float value) {
  return isfinite(value);
}

inline bool isWithinWorkspace(float x_mm, float y_mm, float x_max_mm,
                              float y_max_mm) {
  return isFiniteCoordinate(x_mm) && isFiniteCoordinate(y_mm) &&
         x_mm >= 0.0f && y_mm >= 0.0f && x_mm <= x_max_mm &&
         y_mm <= y_max_mm;
}

}  // namespace drawbot
