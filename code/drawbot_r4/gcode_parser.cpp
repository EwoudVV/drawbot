#include "gcode_parser.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>

namespace drawbot {
namespace {

bool setFloatWord(char word, float value, GcodeCommand& command) {
  switch (word) {
    case 'X':
      if (command.has_x) return false;
      command.has_x = true;
      command.x = value;
      return true;
    case 'Y':
      if (command.has_y) return false;
      command.has_y = true;
      command.y = value;
      return true;
    case 'F':
      if (command.has_f) return false;
      command.has_f = true;
      command.f = value;
      return true;
    case 'S':
      if (command.has_s) return false;
      command.has_s = true;
      command.s = value;
      return true;
    default:
      return false;
  }
}

}  // namespace

ParseStatus parseGcodeLine(const char* line, GcodeCommand& command) {
  command = GcodeCommand{};
  const char* cursor = line;
  bool saw_word = false;

  while (*cursor != '\0') {
    while (isspace(static_cast<unsigned char>(*cursor))) ++cursor;
    if (*cursor == '\0' || *cursor == ';') break;

    if (*cursor == '(') {
      while (*cursor != '\0' && *cursor != ')') ++cursor;
      if (*cursor == '\0') return ParseStatus::kUnterminatedComment;
      ++cursor;
      continue;
    }

    const char word = static_cast<char>(
        toupper(static_cast<unsigned char>(*cursor++)));
    if (word != 'G' && word != 'M' && word != 'X' && word != 'Y' &&
        word != 'F' && word != 'S') {
      return ParseStatus::kUnsupportedWord;
    }

    while (isspace(static_cast<unsigned char>(*cursor))) ++cursor;
    char* end = nullptr;
    const double parsed = strtod(cursor, &end);
    if (end == cursor || !isfinite(parsed)) return ParseStatus::kInvalidNumber;
    cursor = end;
    saw_word = true;

    if (word == 'G' || word == 'M') {
      const double rounded = round(parsed);
      if (fabs(parsed - rounded) > 0.0001) {
        return ParseStatus::kNonIntegerCode;
      }
      if (word == 'G') {
        if (command.has_g) return ParseStatus::kDuplicateWord;
        command.has_g = true;
        command.g = static_cast<int>(rounded);
      } else {
        if (command.has_m) return ParseStatus::kDuplicateWord;
        command.has_m = true;
        command.m = static_cast<int>(rounded);
      }
      continue;
    }

    if (!setFloatWord(word, static_cast<float>(parsed), command)) {
      return ParseStatus::kDuplicateWord;
    }
  }

  return saw_word ? ParseStatus::kOk : ParseStatus::kEmpty;
}

const char* parseStatusMessage(ParseStatus status) {
  switch (status) {
    case ParseStatus::kOk:
      return "ok";
    case ParseStatus::kEmpty:
      return "empty line";
    case ParseStatus::kInvalidNumber:
      return "invalid number";
    case ParseStatus::kDuplicateWord:
      return "duplicate word";
    case ParseStatus::kUnsupportedWord:
      return "unsupported word";
    case ParseStatus::kNonIntegerCode:
      return "G and M codes must be integers";
    case ParseStatus::kUnterminatedComment:
      return "unterminated parenthesized comment";
  }
  return "parse failure";
}

}  // namespace drawbot
