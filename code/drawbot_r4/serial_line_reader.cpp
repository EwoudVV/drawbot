#include "serial_line_reader.h"

namespace drawbot {

LineReadStatus SerialLineReader::poll(Stream& stream) {
  while (stream.available() > 0) {
    const char character = static_cast<char>(stream.read());
    if (character == '\r') continue;

    if (character == '\n') {
      if (discarding_) {
        discarding_ = false;
        length_ = 0;
        buffer_[0] = '\0';
        return LineReadStatus::kOverflow;
      }
      buffer_[length_] = '\0';
      length_ = 0;
      return LineReadStatus::kLine;
    }

    if (discarding_) continue;
    if (length_ >= config::SERIAL_LINE_MAX) {
      discarding_ = true;
      continue;
    }
    buffer_[length_++] = character;
  }
  return LineReadStatus::kNone;
}

LineReadStatus SerialLineReader::feed(char c) {
  if (c == '\r') return LineReadStatus::kNone;
  if (c == '\n') {
    if (discarding_) {
      discarding_ = false;
      length_ = 0;
      buffer_[0] = '\0';
      return LineReadStatus::kOverflow;
    }
    buffer_[length_] = '\0';
    length_ = 0;
    return LineReadStatus::kLine;
  }
  if (discarding_) return LineReadStatus::kNone;
  if (length_ >= config::SERIAL_LINE_MAX) {
    discarding_ = true;
    return LineReadStatus::kNone;
  }
  buffer_[length_++] = c;
  return LineReadStatus::kNone;
}

}  // namespace drawbot
