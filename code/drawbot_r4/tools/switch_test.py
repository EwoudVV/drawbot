#!/usr/bin/env python3
"""Poll M119 and print switch state transitions.

Connects over USB serial (default) or BLE, then repeatedly sends M119 and
prints each state change with a timestamp, so a hand can confirm each switch
is recognized: only the field belonging to the pressed switch may flip.

Usage:
  tools/switch_test.py                # USB, auto-detect port
  tools/switch_test.py --port /dev/cu.usbmodemXXXX
  tools/switch_test.py --ble          # over BLE (24 V on, USB off)
"""

from __future__ import annotations

import argparse
import asyncio
import sys
import time

from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from stream_gcode import NUS_TX_CHAR_UUID, NUS_RX_CHAR_UUID  # noqa: E402

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None
    list_ports = None

try:
    import bleak
except ImportError:
    bleak = None

BAUD = 115200
POLL_INTERVAL_S = 0.25


def choose_port(requested: str | None) -> str:
    if requested:
        return requested
    if list_ports is None:
        raise SystemExit("pyserial is required for USB")
    ports = list(list_ports.comports())
    preferred = [
        port
        for port in ports
        if "arduino" in f"{port.description} {port.manufacturer}".lower()
        or "uno r4" in f"{port.description} {port.product}".lower()
    ]
    candidates = preferred or ports
    if len(candidates) == 1:
        return candidates[0].device
    raise SystemExit(
        "Could not choose one serial port. Pass --port explicitly.\n"
        + "\n".join(f"  {p.device}: {p.description}" for p in ports)
        or "  (none found)"
    )


def parse_state(line: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for token in line.split():
        if token.endswith(":") or ":" not in token:
            continue
        key, value = token.rsplit(":", 1)
        fields[key] = value
    return fields


def usb_poll(args: argparse.Namespace) -> int:
    if serial is None:
        raise SystemExit("pyserial is required. Install with: pip install pyserial")
    port = choose_port(args.port)
    print(f"Opening {port} at {BAUD} baud", flush=True)
    last: dict[str, str] | None = None
    samples = 0
    with serial.Serial(port, BAUD, timeout=0.2) as connection:
        connection.dtr = False
        time.sleep(0.1)
        connection.reset_input_buffer()
        connection.dtr = True
        print("Probing for boot/ready output...", flush=True)
        deadline = time.monotonic() + 4.0
        while time.monotonic() < deadline:
            line = connection.readline().decode("utf-8", errors="replace").strip()
            if line == "ready":
                break
        print("Polling M119 now. Press each switch and watch the fields flip.", flush=True)
        print("Press Ctrl-C to stop.\n", flush=True)
        while True:
            connection.reset_input_buffer()
            connection.write(b"M119\n")
            connection.flush()
            deadline = time.monotonic() + 2.0
            response = None
            while time.monotonic() < deadline:
                line = connection.readline().decode("utf-8", errors="replace").strip()
                if line.startswith("x_home:"):
                    response = line
                    break
            if response is None:
                print("[no M119 response]", flush=True)
                continue
            fields = parse_state(response)
            samples += 1
            if fields != last:
                stamp = time.strftime("%H:%M:%S")
                shown = " ".join(
                    f"{k}:{fields.get(k, '?')}" for k in ("x_home", "y_home", "probe")
                )
                print(f"{stamp}  {shown}", flush=True)
                last = fields
            time.sleep(POLL_INTERVAL_S)


def make_ble_handler(printed, last):
    def handler(_sender, data: bytearray):
        line = data.decode("utf-8", errors="replace").strip()
        if not line:
            return
        if line.startswith("x_home:"):
            fields = parse_state(line)
            if fields != last[0]:
                stamp = time.strftime("%H:%M:%S")
                shown = " ".join(
                    f"{k}:{fields.get(k, '?')}" for k in ("x_home", "y_home", "probe")
                )
                print(f"{stamp}  {shown}", flush=True)
                last[0] = fields
        elif line.startswith("error:"):
            print(f"[{line}]", flush=True)
    return handler


async def ble_poll(args: argparse.Namespace) -> int:
    if bleak is None:
        raise SystemExit("bleak is required for BLE")
    from bleak import BleakClient, BleakScanner

    address = args.address
    if not address:
        devices = await BleakScanner.discover(
            timeout=args.scan_timeout, service_uuids=[NUS_RX_CHAR_UUID]
        )
        if not devices:
            print("drawbot not found (24 V on? board running?)", file=sys.stderr)
            return 1
        address = devices[0].address
    print(f"Connecting to {address}...")
    last: list[dict[str, str] | None] = [None]
    async with BleakClient(address, timeout=60) as client:
        handler = make_ble_handler(print, last)
        await client.start_notify(NUS_TX_CHAR_UUID, handler)
        rx_char = None
        for svc in client.services:
            for char in svc.characteristics:
                if char.uuid.lower() == NUS_RX_CHAR_UUID:
                    rx_char = char
        if rx_char is None:
            print("NUS RX characteristic not found", file=sys.stderr)
            return 1
        print("Connected. Press each switch and watch the fields flip.")
        print("Press Ctrl-C to stop.\n")
        try:
            while True:
                await client.write_gatt_char(rx_char, b"M119\n")
                await asyncio.sleep(POLL_INTERVAL_S)
        except asyncio.CancelledError:
            pass
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="USB serial device (auto-detected)")
    parser.add_argument("--ble", action="store_true", help="use BLE instead of USB")
    parser.add_argument("--address", default="", help="BLE address (default: scan)")
    parser.add_argument("--scan-timeout", type=float, default=10.0)
    args = parser.parse_args()
    if args.ble and args.port:
        parser.error("--port and --ble are mutually exclusive")
    return args


def main() -> int:
    args = parse_args()
    if args.ble:
        return asyncio.run(ble_poll(args))
    return usb_poll(args)


if __name__ == "__main__":
    sys.exit(main())
