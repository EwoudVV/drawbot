#!/usr/bin/env python3
"""Render data/heightmap.csv to a dependency-free SVG heatmap."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path


COLORS = (
    (0.00, (35, 72, 148)),
    (0.25, (34, 155, 188)),
    (0.50, (105, 190, 121)),
    (0.75, (248, 211, 72)),
    (1.00, (207, 55, 61)),
)


def color(value: float, minimum: float, maximum: float) -> str:
    fraction = (value - minimum) / (maximum - minimum)
    fraction = max(0.0, min(1.0, fraction))
    for (left_t, left), (right_t, right) in zip(COLORS, COLORS[1:]):
        if fraction <= right_t:
            mix = (fraction - left_t) / (right_t - left_t)
            rgb = tuple(round(a + mix * (b - a)) for a, b in zip(left, right))
            return f"rgb{rgb}"
    return f"rgb{COLORS[-1][1]}"


def read_grid(path: Path) -> tuple[list[float], list[float], list[list[float]]]:
    with path.open(newline="") as source:
        rows = list(csv.DictReader(source))
    xs = sorted({float(row["x_mm"]) for row in rows})
    ys = sorted({float(row["y_mm"]) for row in rows})
    values = {(float(r["x_mm"]), float(r["y_mm"])): float(r["servo_us"]) for r in rows}
    expected = len(xs) * len(ys)
    if len(rows) != expected or len(values) != expected:
        raise SystemExit("CSV must contain one value at every point of a regular grid")
    return xs, ys, [[values[(x, y)] for x in xs] for y in ys]


def interpolate(
    x: float, y: float, xs: list[float], ys: list[float], grid: list[list[float]]
) -> float:
    x = max(xs[0], min(xs[-1], x))
    y = max(ys[0], min(ys[-1], y))
    ix = min(next((i for i in range(len(xs) - 1) if x <= xs[i + 1]), len(xs) - 2), len(xs) - 2)
    iy = min(next((i for i in range(len(ys) - 1) if y <= ys[i + 1]), len(ys) - 2), len(ys) - 2)
    tx = (x - xs[ix]) / (xs[ix + 1] - xs[ix])
    ty = (y - ys[iy]) / (ys[iy + 1] - ys[iy])
    lower = grid[iy][ix] + tx * (grid[iy][ix + 1] - grid[iy][ix])
    upper = grid[iy + 1][ix] + tx * (grid[iy + 1][ix + 1] - grid[iy + 1][ix])
    return lower + ty * (upper - lower)


def render(source: Path, destination: Path) -> None:
    xs, ys, grid = read_grid(source)
    width, height = 1000, 720
    left, top, plot_w, plot_h = 90, 100, 720, 525
    minimum = min(min(row) for row in grid)
    maximum = max(max(row) for row in grid)
    sample_x, sample_y = 180, 140
    cell_w, cell_h = plot_w / sample_x, plot_h / sample_y

    elements = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#f7f5ef"/>',
        '<text x="90" y="42" font-family="system-ui,sans-serif" font-size="25" font-weight="700" fill="#172033">Recovered drawbot surface map</text>',
        '<text x="90" y="67" font-family="system-ui,sans-serif" font-size="14" fill="#566072">Servo pulse (microseconds); center reference = 945 us</text>',
    ]

    for row in range(sample_y):
        y_mm = ys[-1] - (row + 0.5) / sample_y * (ys[-1] - ys[0])
        for column in range(sample_x):
            x_mm = xs[0] + (column + 0.5) / sample_x * (xs[-1] - xs[0])
            value = interpolate(x_mm, y_mm, xs, ys, grid)
            elements.append(
                f'<rect x="{left + column * cell_w:.2f}" y="{top + row * cell_h:.2f}" '
                f'width="{cell_w + 0.2:.2f}" height="{cell_h + 0.2:.2f}" fill="{color(value, minimum, maximum)}"/>'
            )

    def px(x_mm: float) -> float:
        return left + (x_mm - xs[0]) / (xs[-1] - xs[0]) * plot_w

    def py(y_mm: float) -> float:
        return top + (ys[-1] - y_mm) / (ys[-1] - ys[0]) * plot_h

    for x in xs:
        elements.append(f'<line x1="{px(x):.2f}" y1="{top}" x2="{px(x):.2f}" y2="{top + plot_h}" stroke="#fff" stroke-opacity=".25"/>')
        elements.append(f'<text x="{px(x):.2f}" y="{top + plot_h + 23}" text-anchor="middle" font-family="system-ui,sans-serif" font-size="12" fill="#3c4659">{x:g}</text>')
    for y in ys:
        elements.append(f'<line x1="{left}" y1="{py(y):.2f}" x2="{left + plot_w}" y2="{py(y):.2f}" stroke="#fff" stroke-opacity=".25"/>')
        elements.append(f'<text x="{left - 12}" y="{py(y) + 4:.2f}" text-anchor="end" font-family="system-ui,sans-serif" font-size="12" fill="#3c4659">{y:g}</text>')
    for yi, y in enumerate(ys):
        for xi, x in enumerate(xs):
            elements.append(f'<circle cx="{px(x):.2f}" cy="{py(y):.2f}" r="3.2" fill="#111827" stroke="#fff" stroke-width="1"/>')
            elements.append(f'<text x="{px(x):.2f}" y="{py(y) - 7:.2f}" text-anchor="middle" font-family="system-ui,sans-serif" font-size="9" font-weight="700" fill="#111827" stroke="#fff" stroke-width="0.8" paint-order="stroke">{grid[yi][xi]:.0f}</text>')

    elements.extend([
        f'<rect x="{left}" y="{top}" width="{plot_w}" height="{plot_h}" fill="none" stroke="#172033" stroke-width="1.5"/>',
        f'<text x="{left + plot_w / 2}" y="{height - 20}" text-anchor="middle" font-family="system-ui,sans-serif" font-size="14" fill="#172033">X position (mm)</text>',
        f'<text x="22" y="{top + plot_h / 2}" transform="rotate(-90 22 {top + plot_h / 2})" text-anchor="middle" font-family="system-ui,sans-serif" font-size="14" fill="#172033">Y position (mm)</text>',
    ])

    bar_x, bar_y, bar_w, bar_h = 860, 120, 28, 450
    for index in range(100):
        value = maximum - index / 99 * (maximum - minimum)
        elements.append(f'<rect x="{bar_x}" y="{bar_y + index * bar_h / 100:.2f}" width="{bar_w}" height="{bar_h / 100 + .2:.2f}" fill="{color(value, minimum, maximum)}"/>')
    elements.extend([
        f'<rect x="{bar_x}" y="{bar_y}" width="{bar_w}" height="{bar_h}" fill="none" stroke="#172033"/>',
        f'<text x="{bar_x + bar_w + 10}" y="{bar_y + 5}" font-family="system-ui,sans-serif" font-size="12" fill="#172033">{maximum:.0f} us</text>',
        f'<text x="{bar_x + bar_w + 10}" y="{bar_y + bar_h / 2 + 4}" font-family="system-ui,sans-serif" font-size="12" fill="#172033">{(minimum + maximum) / 2:.0f} us</text>',
        f'<text x="{bar_x + bar_w + 10}" y="{bar_y + bar_h + 4}" font-family="system-ui,sans-serif" font-size="12" fill="#172033">{minimum:.0f} us</text>',
        '</svg>',
    ])
    destination.write_text("\n".join(elements))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", nargs="?", type=Path, default=Path(__file__).parents[1] / "data" / "heightmap.csv")
    parser.add_argument("destination", nargs="?", type=Path, default=Path(__file__).parents[1] / "data" / "heightmap.svg")
    args = parser.parse_args()
    render(args.source, args.destination)
    print(f"Wrote {args.destination}")


if __name__ == "__main__":
    main()
