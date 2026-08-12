#pragma once

#include <Arduino.h>

#include "config.h"

namespace drawbot {

enum class LineReadStatus { kNone, kLine, kOverflow };

class SerialLineReader {
 public:
  LineReadStatus poll(Stream& stream);
  LineReadStatus feed(char c);
  const char* line() const { return buffer_; }

 private:
  char buffer_[config::SERIAL_LINE_MAX + 1]{};
  size_t length_ = 0;
  bool discarding_ = false;
};

}  // namespace drawbot
