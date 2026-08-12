#!/usr/bin/env bash
set -eu
DIR="$(cd "$(dirname "$0")" && pwd)"
exec "$DIR/.venv/bin/python3" "$DIR/stream_gcode.py" "$@"
