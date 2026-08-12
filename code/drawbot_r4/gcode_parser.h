#pragma once

#include <stddef.h>

namespace drawbot {

enum class ParseStatus {
  kOk,
  kEmpty,
  kInvalidNumber,
  kDuplicateWord,
  kUnsupportedWord,
  kNonIntegerCode,
  kUnterminatedComment,
};

struct GcodeCommand {
  bool has_g = false;
  int g = -1;
  bool has_m = false;
  int m = -1;
  bool has_x = false;
  float x = 0.0f;
  bool has_y = false;
  float y = 0.0f;
  bool has_f = false;
  float f = 0.0f;
  bool has_s = false;
  float s = 0.0f;
};

ParseStatus parseGcodeLine(const char* line, GcodeCommand& command);
const char* parseStatusMessage(ParseStatus status);

}  // namespace drawbot
