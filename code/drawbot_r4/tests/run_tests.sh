#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${TMPDIR:-/tmp}/drawbot_r4_tests"

mkdir -p "$BUILD_DIR"
c++ -std=c++17 -Wall -Wextra -Werror \
  "$SCRIPT_DIR/test_firmware_core.cpp" \
  "$PROJECT_DIR/gcode_parser.cpp" \
  -o "$BUILD_DIR/test_firmware_core"
"$BUILD_DIR/test_firmware_core"
python3 "$SCRIPT_DIR/test_sender.py"
