"""
Stream G-code to an ESP32 plotter over USB serial.

- Opens a serial port.
- Optionally waits for "ok" or "ready" at start (board may reboot on connect).
- Sends one non-empty line at a time.
- Waits for "ok" after each line.
- Stops immediately on "error:" from the device.

Install:
  If you are using a venv:
    python3 -m pip install pyserial
  If you are NOT using a venv:
    python3 -m pip install --user pyserial

Usage (macOS):
  ls /dev/cu.* | grep -i usb
  python3 stream_gcode.py --port /dev/cu.usbserial-0001 --file job.gcode --verbose

Recommended file header:
  G21
  G90
  M5
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path
from typing import Iterable, Optional

import serial


def iter_gcode_lines(path: Path) -> Iterable[str]:
  """Yield cleaned G-code lines, preserving original command text."""
  with path.open("r", encoding="utf-8", errors="ignore") as f:
    for raw in f:
      line = raw.strip()
      if not line:
        continue
      yield line


def read_device_line(ser: serial.Serial, timeout_s: float) -> Optional[str]:
  """Read a line from the device, return decoded string or None on timeout."""
  start = time.time()
  while (time.time() - start) < timeout_s:
    if ser.in_waiting:
      data = ser.readline()
      try:
        return data.decode("utf-8", errors="ignore").strip()
      except UnicodeDecodeError:
        return ""
    time.sleep(0.005)
  return None


def wait_for_ack_or_error(
  ser: serial.Serial,
  timeout_s: float,
  *,
  verbose: bool = False,
  accept_ready: bool = False,
) -> None:
  """
  Block until 'ok' (or optionally 'ready') is seen.
  Raise on 'error:' or timeout.
  """
  while True:
    resp = read_device_line(ser, timeout_s)
    if resp is None:
      raise TimeoutError("Timed out waiting for device response")

    if verbose and resp:
      print(f"< {resp}")

    low = resp.lower()
    if low == "ok":
      return
    if accept_ready and low == "ready":
      return
    if low.startswith("error:"):
      raise RuntimeError(resp)
    # Ignore other chatter lines and keep waiting.


def stream_gcode(
  port: str,
  baud: int,
  file_path: Path,
  *,
  start_timeout_s: float = 5.0,
  line_timeout_s: float = 10.0,
  verbose: bool = False,
  dry_run: bool = False,
) -> None:
  """Stream G-code file to plotter with ok-handshake."""
  if not file_path.exists():
    raise FileNotFoundError(file_path)

  lines = list(iter_gcode_lines(file_path))
  if not lines:
    raise ValueError("G-code file contains no non-empty lines")

  if dry_run:
    print("DRY RUN. First 20 lines:")
    for i, line in enumerate(lines[:20], start=1):
      print(f"{i:04d}: {line}")
    print(f"Total lines: {len(lines)}")
    return

  with serial.Serial(port, baudrate=baud, timeout=0.1) as ser:
    # Some ESP32 boards reboot on serial open.
    time.sleep(1.5)
    ser.reset_input_buffer()
    ser.reset_output_buffer()

    # Optional: accept either "ok" or "ready" at start.
    try:
      wait_for_ack_or_error(
        ser,
        start_timeout_s,
        verbose=verbose,
        accept_ready=True,
      )
    except TimeoutError:
      if verbose:
        print("! No initial ok/ready seen. Proceeding anyway.")

    t0 = time.time()
    for idx, line in enumerate(lines, start=1):
      if verbose:
        print(f"> {line}")
      ser.write((line + "\n").encode("utf-8"))
      ser.flush()

      try:
        wait_for_ack_or_error(ser, line_timeout_s, verbose=verbose)
      except TimeoutError as e:
        raise TimeoutError(
          f"Timeout after sending line {idx}/{len(lines)}: {line}"
        ) from e
      except RuntimeError as e:
        raise RuntimeError(
          f"Device error at line {idx}/{len(lines)}: {line}\n{e}"
        ) from e

      if idx % 100 == 0:
        elapsed = time.time() - t0
        print(f"Sent {idx}/{len(lines)} lines in {elapsed:.1f}s")

    elapsed = time.time() - t0
    print(f"Done. Sent {len(lines)} lines in {elapsed:.1f}s")


def parse_args(argv: list[str]) -> argparse.Namespace:
  p = argparse.ArgumentParser(description="Stream G-code to ESP32 plotter.")
  p.add_argument("--port", required=True, help="Serial port, e.g. /dev/cu.usbserial-XXXX")
  p.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
  p.add_argument("--file", required=True, type=Path, help="Path to .gcode file")
  p.add_argument("--verbose", action="store_true", help="Print device chatter and sent lines")
  p.add_argument("--dry-run", action="store_true", help="Print file summary, do not send")
  p.add_argument("--start-timeout", type=float, default=5.0, help="Initial ok/ready timeout seconds")
  p.add_argument("--line-timeout", type=float, default=10.0, help="Per-line ok timeout seconds")
  return p.parse_args(argv)


def main(argv: list[str]) -> int:
  args = parse_args(argv)
  try:
    stream_gcode(
      port=args.port,
      baud=args.baud,
      file_path=args.file,
      start_timeout_s=args.start_timeout,
      line_timeout_s=args.line_timeout,
      verbose=args.verbose,
      dry_run=args.dry_run,
    )
    return 0
  except Exception as e:
    print(f"ERROR: {e}", file=sys.stderr)
    return 1


if __name__ == "__main__":
  raise SystemExit(main(sys.argv[1:]))