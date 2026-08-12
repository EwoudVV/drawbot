#!/usr/bin/env python3
"""Sweep the pen servo through a pulse range over BLE in one session.

Usage: tools/.venv/bin/python tools/servo_sweep.py [start stop step] [--hold-us N]

Sends M280 for each pulse value, pausing between steps so the mechanism can
be watched directly. Defaults to a broad diagnostic sweep. Run with 24 V on
(servo buck powered), pen near the paper.
"""

from __future__ import annotations

import argparse
import asyncio
import sys

from bleak import BleakClient

NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_TX_CHAR_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX_CHAR_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
ADDRESS = "51974A51-AB5A-54FF-89A7-69167EA6986E"


async def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("values", nargs="*", type=int,
                        help="explicit pulse values to visit")
    parser.add_argument("--range", type=int, nargs=3,
                        metavar=("START", "STOP", "STEP"),
                        help="sweep START..STOP in STEP increments (inclusive)")
    parser.add_argument("--pause", type=float, default=1.5,
                        help="seconds to wait after each pulse")
    parser.add_argument("--hold", type=int, default=None,
                        help="finish by holding this pulse")
    args = parser.parse_args()

    if args.range:
        start, stop, step = args.range
        values = list(range(start, stop + step, step))
    elif args.values:
        values = args.values
    else:
        values = list(range(1000, 1201, 20)) + \
                 list(range(980, 899, -20)) + [1000]
    if args.hold is not None:
        values.append(args.hold)
    values = [max(500, min(2200, v)) for v in values]

    client = BleakClient(ADDRESS, timeout=60)
    await client.connect()
    rx_char = None
    for svc in client.services:
        for char in svc.characteristics:
            if char.uuid.lower() == NUS_RX_CHAR_UUID:
                rx_char = char
    if rx_char is None:
        print("NUS RX not found", file=sys.stderr)
        await client.disconnect()
        return 2

    print(f"watching {len(values)} pulses; pause {args.pause}s between steps")
    for v in values:
        print(f"> M280 S{v}")
        await client.write_gatt_char(rx_char, f"M280 S{v}\n".encode())
        await asyncio.sleep(args.pause)
    await client.disconnect()
    print("sweep done")
    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
