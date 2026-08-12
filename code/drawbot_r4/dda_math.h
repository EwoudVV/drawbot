#pragma once

#include <stdint.h>

namespace drawbot {

struct DdaStep {
  bool motor_a;
  bool motor_b;
};

struct DdaState {
  int32_t count_a;
  int32_t count_b;
  int32_t event_count;
  int32_t accumulator_a = 0;
  int32_t accumulator_b = 0;
};

inline DdaStep nextDdaStep(DdaState& state) {
  state.accumulator_a += state.count_a;
  state.accumulator_b += state.count_b;
  const bool step_a = state.accumulator_a >= state.event_count;
  const bool step_b = state.accumulator_b >= state.event_count;
  if (step_a) state.accumulator_a -= state.event_count;
  if (step_b) state.accumulator_b -= state.event_count;
  return {step_a, step_b};
}

}  // namespace drawbot
