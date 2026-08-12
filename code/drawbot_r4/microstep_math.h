#pragma once

#include <stdint.h>

#include "machine_types.h"

namespace drawbot {

// The numeric value is the number of microsteps per full motor step.
enum class MicrostepMode : uint8_t {
  kFull = 1,
  kHalf = 2,
  kQuarter = 4,
  kEighth = 8,
  kSixteenth = 16,
};

constexpr int microstepsPerFullStep(MicrostepMode mode) {
  return static_cast<int>(mode);
}

// All positions are tracked in 1/16-step base units. This is the number of
// base units advanced by one STEP pulse in the selected mode.
constexpr int baseUnitsPerPulse(MicrostepMode mode) {
  return 16 / microstepsPerFullStep(mode);
}

constexpr bool isAlignedToMode(int phase, MicrostepMode mode) {
  return phase % baseUnitsPerPulse(mode) == 0;
}

constexpr bool isMoveRepresentable(const MotorSteps& delta, int phase_a,
                                   int phase_b, MicrostepMode mode) {
  const int quantum = baseUnitsPerPulse(mode);
  return isAlignedToMode(phase_a, mode) && isAlignedToMode(phase_b, mode) &&
         delta.a % quantum == 0 && delta.b % quantum == 0;
}

inline MicrostepMode selectCompatibleMode(const MotorSteps& delta, int phase_a,
                                          int phase_b,
                                          MicrostepMode coarsest_allowed) {
  constexpr MicrostepMode MODES[] = {
      MicrostepMode::kFull, MicrostepMode::kHalf, MicrostepMode::kQuarter,
      MicrostepMode::kEighth, MicrostepMode::kSixteenth};
  bool allowed = false;
  for (MicrostepMode mode : MODES) {
    if (mode == coarsest_allowed) allowed = true;
    if (allowed && isMoveRepresentable(delta, phase_a, phase_b, mode)) {
      return mode;
    }
  }
  return MicrostepMode::kSixteenth;
}

inline int advancePhase(int phase, int direction, MicrostepMode mode) {
  int next = (phase + direction * baseUnitsPerPulse(mode)) % 16;
  if (next < 0) next += 16;
  return next;
}

inline const char* microstepModeName(MicrostepMode mode) {
  switch (mode) {
    case MicrostepMode::kFull:
      return "1";
    case MicrostepMode::kHalf:
      return "1/2";
    case MicrostepMode::kQuarter:
      return "1/4";
    case MicrostepMode::kEighth:
      return "1/8";
    case MicrostepMode::kSixteenth:
      return "1/16";
  }
  return "?";
}

}  // namespace drawbot
