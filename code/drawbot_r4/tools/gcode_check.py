#!/usr/bin/env python3
"""Validate a G-code file against the drawbot's command set and limits.

Usage: tools/.venv/bin/python tools/gcode_check.py path/to/file.gcode [--paper 400x300]

Checks: supported commands only, one command per line, 96-byte line limit,
soft-limit bounds, per-command feed limits, and pen-state consistency.
Exits 0 if the file is safe to draw, 1 otherwise.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

X_MAX = 400.0
Y_MAX = 320.0
LINE_MAX_BYTES = 96
G0_MAX_FEED = 2000.0
G1_MAX_FEED = 800.0

SUPPORTED = {
    "G21", "G90", "G0", "G1", "M2", "M3", "M5", "M17", "M18", "M50", "M114", "M119", "M280", "M30",
}
WORDS = re.compile(r"([A-Z])([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)")

PEN_UP = 0
PEN_DOWN = 1
PEN_UNKNOWN = 2


class Problem:
    def __init__(self) -> None:
        self.errors: list[str] = []
        self.warnings: list[str] = []

    def error(self, msg: str) -> None:
        self.errors.append(msg)

    def warn(self, msg: str) -> None:
        self.warnings.append(msg)


def check(path: Path, paper_w: float, paper_h: float) -> Problem:
    problems = Problem()
    pen = PEN_UNKNOWN
    saw_m3 = False

    for number, raw in enumerate(path.read_text(errors="replace").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith(";") or (line.startswith("(") and line.endswith(")")):
            continue
        if len(line.encode("ascii", errors="replace")) > LINE_MAX_BYTES:
            problems.error(f"line {number}: {LINE_MAX_BYTES} byte limit exceeded ({len(line)} chars)")
            continue

        words = dict((k, float(v)) for k, v in WORDS.findall(line))
        if not words:
            problems.warn(f"line {number}: no commands on this line: {line}")
            continue

        g = words.get("G")
        m = words.get("M")
        if g is not None and m is not None:
            problems.error(f"line {number}: only one G or M command per line: {line}")
            continue
        code = f"G{int(g)}" if g is not None else f"M{int(m)}"
        if code not in SUPPORTED:
            problems.error(f"line {number}: unsupported command {code} (G20/G91/G2/G3 are rejected)")
            continue

        x = words.get("X")
        y = words.get("Y")
        f = words.get("F")

        if code in ("G0", "G1"):
            if x is None and y is None:
                problems.error(f"line {number}: {code} requires X and/or Y")
            if x is not None and not (0.0 <= x <= paper_w):
                problems.error(f"line {number}: X {x} outside workspace 0..{paper_w:.0f}")
            if y is not None and not (0.0 <= y <= paper_h):
                problems.error(f"line {number}: Y {y} outside workspace 0..{paper_h:.0f}")
            limit = G0_MAX_FEED if code == "G0" else G1_MAX_FEED
            if f is not None and f > limit:
                problems.error(f"line {number}: {code} feed {f} exceeds {limit:.0f} mm/min")
            if code == "G0":
                pen = PEN_UP  # G0 forces the pen up
            elif pen != PEN_DOWN:
                problems.warn(f"line {number}: G1 while pen is up — nothing will be drawn "
                              "(insert M3 before drawing lines)")
        elif code == "M3":
            pen = PEN_DOWN
            saw_m3 = True
        elif code == "M5":
            pen = PEN_UP
        elif code in ("M2", "M30"):
            pen = PEN_UP
        elif code == "G28":
            problems.error("line {number}: G28 (auto-homing) does not work — the operator "
                           "homes manually with M50")

    if not saw_m3:
        problems.warn("file never lowers the pen (no M3) — the drawing will be empty")
    if pen == PEN_DOWN:
        problems.warn("file ends with the pen down; add M5 before M2")

    return problems


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("gcode", type=Path)
    parser.add_argument("--paper", default="400x300",
                        help="workspace as WxH in mm (default 400x300 — current paper)")
    args = parser.parse_args()

    try:
        paper_w, paper_h = (float(v) for v in args.paper.lower().split("x"))
    except ValueError:
        parser.error("--paper must be like 400x300 (mm)")

    problems = check(args.gcode, paper_w, paper_h)
    for warning in problems.warnings:
        print(f"warning: {warning}")
    for error in problems.errors:
        print(f"error:   {error}")
    if not problems.errors and not problems.warnings:
        print(f"{args.gcode} — OK: safe to draw on {paper_w:.0f}x{paper_h:.0f} mm paper")
    return 1 if problems.errors else 0


if __name__ == "__main__":
    sys.exit(main())
