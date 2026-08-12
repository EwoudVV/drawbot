#pragma once

#include <Arduino.h>

#include "gcode_parser.h"

namespace drawbot {

class MotionController;
class PenController;
enum class MotionStatus;

class CommandProcessor {
 public:
  CommandProcessor(MotionController& motion, PenController& pen)
      : motion_(motion), pen_(pen) {}

  bool execute(const GcodeCommand& command, Print& output, char* error,
               size_t error_size);

 private:
  bool executeG(const GcodeCommand& command, Print& output, char* error,
                size_t error_size);
  bool executeM(const GcodeCommand& command, Print& output, char* error,
                size_t error_size);
  bool requireNoArguments(const GcodeCommand& command, char* error,
                          size_t error_size) const;
  bool reportMotionResult(MotionStatus status, char* error,
                          size_t error_size) const;

  MotionController& motion_;
  PenController& pen_;
};

}  // namespace drawbot
