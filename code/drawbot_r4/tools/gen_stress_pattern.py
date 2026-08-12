#!/usr/bin/env python3
"""Generate a long, varied stress drawing for the drawbot.

Full-workspace diagonal hatching (alternating +45/-45 degree blocks) plus a
border rectangle. Exercises both axes together, long straight segments at max
draw speed, frequent direction flips, accel/decel ramps, and pen up/down
cycling. Target duration defaults to 30 minutes.

Paper size is passed on the command line (defaults to 400x300 mm — the paper
on hand); nothing is hardcoded.

Usage: tools/.venv/bin/python tools/gen_stress_pattern.py --minutes 30 \
       --paper 400x300 --out examples/stress_30min.gcode
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path

MARGIN = 10.0
DRAW_FEED = 800.0     # mm/min (G1 max)
TRAVEL_FEED = 2000.0  # mm/min (G0 max)
PEN_SETTLE_S = 0.6    # M3+M5 servo settle overhead per line


def clip_diagonal(x0: float, y0: float, x1: float, y1: float, angle_deg: float,
                  offset: float) -> tuple[float, float, float, float] | None:
    """Return the segment of a diagonal line (at angle, at perpendicular
    offset) that lies inside the rectangle, or None if it misses it."""
    if angle_deg == 45:
        # y = x + b; intersect with the rectangle.
        b = offset / math.sqrt(2.0)
        lo = max(x0, y0 - b)
        hi = min(x1, y1 - b)
        if hi - lo <= 0.0:
            return None
        return lo, lo + b, hi, hi + b
    # -45 degrees: y = -x + b
    b = offset / math.sqrt(2.0)
    lo = max(x0, b - y1)
    hi = min(x1, b - y0)
    if hi - lo <= 0.0:
        return None
    return lo, b - lo, hi, b - hi


def hatch_block(x0: float, y0: float, x1: float, y1: float, angle_deg: float,
                pitch: float) -> list[tuple[float, float, float, float]]:
    lines: list[tuple[float, float, float, float]] = []
    # Project the rectangle corners onto the perpendicular axis to bound the
    # offset sweep.
    if angle_deg == 45:
        perp = (math.sqrt(0.5), -math.sqrt(0.5))  # (-1, 1)/sqrt2
    else:
        perp = (math.sqrt(0.5), math.sqrt(0.5))   # (1, 1)/sqrt2
    corners = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    projections = [c[0] * perp[0] + c[1] * perp[1] for c in corners]
    lo_off, hi_off = min(projections), max(projections)
    offset = lo_off
    while offset <= hi_off:
        segment = clip_diagonal(x0, y0, x1, y1, angle_deg, offset)
        if segment is not None:
            lines.append(segment)
        offset += pitch
    return lines


def total_segment_length(lines: list[tuple[float, float, float, float]]) -> float:
    return sum(math.hypot(s[2] - s[0], s[3] - s[1]) for s in lines)


def build_order(pitch: float, paper_w: float, paper_h: float) -> tuple[list[tuple[float, float, float, float]], int]:
    x0, y0 = MARGIN, MARGIN
    x1, y1 = paper_w - MARGIN, paper_h - MARGIN
    plus = hatch_block(x0, y0, x1, y1, 45, pitch)
    minus = hatch_block(x0, y0, x1, y1, -45, pitch)
    block = 8
    order: list[tuple[float, float, float, float]] = []
    for i in range(0, max(len(plus), len(minus)), block):
        if i < len(plus):
            order.extend(plus[i:i + block])
        if i < len(minus):
            order.extend(minus[i:i + block])
    return order, len(plus) + len(minus)


def estimate_minutes(order: list[tuple[float, float, float, float]], paper_w: float,
                     paper_h: float) -> tuple[float, float, int]:
    draw_mm = sum(math.hypot(s[2] - s[0], s[3] - s[1]) for s in order)
    travel_mm = 0.0
    prev_x = prev_y = MARGIN
    for seg in order:
        travel_mm += math.hypot(seg[0] - prev_x, seg[1] - prev_y)
        prev_x, prev_y = seg[2], seg[3]
    draw_s = draw_mm / (DRAW_FEED / 60.0)
    travel_s = travel_mm / (TRAVEL_FEED / 60.0)
    return (draw_s + travel_s + len(order) * PEN_SETTLE_S) / 60.0, draw_mm, len(order)


def generate(minutes: float, paper_w: float, paper_h: float) -> tuple[list[str], float, float, int]:
    x0, y0 = MARGIN, MARGIN
    x1, y1 = paper_w - MARGIN, paper_h - MARGIN

    lo, hi = 0.5, 60.0
    for _ in range(60):
        mid = 0.5 * (lo + hi)
        order, _n = build_order(mid, paper_w, paper_h)
        est, _draw, _cycles = estimate_minutes(order, paper_w, paper_h)
        if est > minutes:
            lo = mid
        else:
            hi = mid
    pitch = 0.5 * (lo + hi)
    order, _n = build_order(pitch, paper_w, paper_h)
    est_min, draw_mm, pen_cycles = estimate_minutes(order, paper_w, paper_h)

    commands: list[str] = ["G21", "G90"]
    prev_x = prev_y = x0
    for seg in order:
        commands.append(f"G0 X{seg[0]:.3f} Y{seg[1]:.3f} F{TRAVEL_FEED:.0f}")
        commands.append("M3")
        commands.append(f"G1 X{seg[2]:.3f} Y{seg[3]:.3f} F{DRAW_FEED:.0f}")
        commands.append("M5")
        prev_x, prev_y = seg[2], seg[3]

    # Border rectangle.
    border = [(x0, y0, x1, y0), (x1, y0, x1, y1), (x1, y1, x0, y1), (x0, y1, x0, y0)]
    for seg in border:
        commands.append(f"G0 X{seg[0]:.3f} Y{seg[1]:.3f} F{TRAVEL_FEED:.0f}")
        commands.append("M3")
        commands.append(f"G1 X{seg[2]:.3f} Y{seg[3]:.3f} F{DRAW_FEED:.0f}")
        commands.append("M5")
        pen_cycles += 1

    commands.append("M5")
    commands.append("M2")

    return commands, draw_mm, est_min, pen_cycles


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--minutes", type=float, default=30.0)
    parser.add_argument("--paper", default="400x300",
                        help="paper size as WxH in mm, measured from the home "
                             "corner (default 400x300 — not hardcoded limits)")
    parser.add_argument("--out", type=Path, default=Path("examples/stress_30min.gcode"))
    args = parser.parse_args()

    try:
        paper_w, paper_h = (float(v) for v in args.paper.lower().split("x"))
    except ValueError:
        parser.error("--paper must be like 400x300 (mm)")

    commands, draw_mm, minutes_est, pen_cycles = generate(args.minutes, paper_w, paper_h)
    args.out.write_text("\n".join(commands) + "\n")
    print(f"wrote {args.out}  (paper {paper_w:.0f}x{paper_h:.0f} mm)")
    print(f"  {len(commands)} commands, {pen_cycles} pen cycles")
    print(f"  {draw_mm:.0f} mm of pen-down travel")
    print(f"  estimated {minutes_est:.1f} min at max feed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
