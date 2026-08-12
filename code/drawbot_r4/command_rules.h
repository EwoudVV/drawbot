#pragma once

namespace drawbot {

constexpr bool isSupportedGCode(int code) {
  return code == 0 || code == 1 || code == 21 || code == 28 || code == 90;
}

constexpr bool isSupportedMCode(int code) {
  return code == 2 || code == 3 || code == 5 || code == 17 || code == 18 ||
         code == 30 || code == 50 || code == 100 || code == 101 ||
         code == 114 || code == 119 || code == 280;
}

}  // namespace drawbot
