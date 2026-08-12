#!/usr/bin/env python3
"""BLE helper for the drawbot: scan, short queries, fire-and-forget long runs.

The Arduino accepts one line at a time over the Nordic UART Service and
answers with lines ending in "ok" (or "error: ..."). Long synchronous M-codes
outlive the macOS BLE supervision timeout and the connection drops mid-run;
the board keeps executing, so "fire" treats a dropped link as expected and
just reports what it saw.

Run with tools/.venv/bin/python (bleak is installed there).
"""

from __future__ import annotations

import argparse
import asyncio
import sys

try:
    from bleak import BleakClient, BleakScanner
except ImportError:
    print("bleak is required; run with tools/.venv/bin/python", file=sys.stderr)
    sys.exit(2)

NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_TX_CHAR_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX_CHAR_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
DEFAULT_ADDRESS = "51974A51-AB5A-54FF-89A7-69167EA6986E"

EXIT_OK = 0
EXIT_NOT_FOUND = 1
EXIT_ERROR_RESPONSE = 2
EXIT_TIMEOUT = 3


def line_assembler(callback):
    """Return a notify handler that reassembles chunked lines and calls
    callback(line) once per completed line."""
    recv = bytearray()

    def handler(_sender, data: bytearray):
        nonlocal recv
        recv.extend(data)
        while b"\n" in recv:
            raw, recv = recv.split(b"\n", 1)
            line = raw.decode("utf-8", errors="replace").strip()
            if line:
                callback(line)

    return handler


async def find_drawbot(address: str | None, scan_timeout: float) -> str | None:
    if address:
        return address
    devices = await BleakScanner.discover(
        timeout=scan_timeout, service_uuids=[NUS_SERVICE_UUID]
    )
    if not devices:
        return None
    for d in devices:
        if d.name and "drawbot" in d.name.lower():
            return d.address
    return devices[0].address


async def open_client(address: str):
    client = BleakClient(address, timeout=60)
    await client.connect()
    rx_char = None
    for svc in client.services:
        for char in svc.characteristics:
            if char.uuid.lower() == NUS_RX_CHAR_UUID:
                rx_char = char
    if rx_char is None:
        await client.disconnect()
        raise RuntimeError("NUS RX characteristic not found")
    return client, rx_char


async def cmd_send(args: argparse.Namespace) -> int:
    command = args.command
    address = await find_drawbot(args.address, args.scan_timeout)
    if address is None:
        print("drawbot not found (24 V on? board running?)", file=sys.stderr)
        return EXIT_NOT_FOUND
    print(f"> {command}")
    result = {"line": None}
    stop = asyncio.Event()
    matched = [False]

    def on_line(line: str):
        print(f"< {line}")
        if matched[0]:
            return
        if line == "ok" or line.startswith("error:"):
            result["line"] = line
            matched[0] = True
            stop.set()

    async with BleakClient(address, timeout=60) as client:
        handler = line_assembler(on_line)
        await client.start_notify(NUS_TX_CHAR_UUID, handler)
        rx_char = None
        for svc in client.services:
            for char in svc.characteristics:
                if char.uuid.lower() == NUS_RX_CHAR_UUID:
                    rx_char = char
        if rx_char is None:
            print("NUS RX characteristic not found", file=sys.stderr)
            return EXIT_NOT_FOUND
        await client.write_gatt_char(rx_char, command.encode("ascii") + b"\n")
        try:
            await asyncio.wait_for(stop.wait(), timeout=args.timeout)
        except asyncio.TimeoutError:
            print(f"(no reply within {args.timeout}s)", file=sys.stderr)
            return EXIT_TIMEOUT
    if result["line"] == "ok":
        return EXIT_OK
    return EXIT_ERROR_RESPONSE


async def cmd_fire(args: argparse.Namespace) -> int:
    """Send a long-running command and listen until ok/error or --listen runs
    out. A mid-run BLE drop is expected; the board keeps executing."""
    command = args.command
    address = await find_drawbot(args.address, args.scan_timeout)
    if address is None:
        print("drawbot not found (24 V on? board running?)", file=sys.stderr)
        return EXIT_NOT_FOUND
    print(f"> {command}")
    matched = [False]
    result = {"line": None}
    stop = asyncio.Event()

    def on_line(line: str):
        print(f"< {line}")
        if matched[0]:
            return
        if line == "ok" or line.startswith("error:"):
            result["line"] = line
            matched[0] = True
            stop.set()

    async with BleakClient(address, timeout=60) as client:
        await client.start_notify(NUS_TX_CHAR_UUID, line_assembler(on_line))
        rx_char = None
        for svc in client.services:
            for char in svc.characteristics:
                if char.uuid.lower() == NUS_RX_CHAR_UUID:
                    rx_char = char
        if rx_char is None:
            print("NUS RX characteristic not found", file=sys.stderr)
            return EXIT_NOT_FOUND
        await client.write_gatt_char(rx_char, command.encode("ascii") + b"\n")
        try:
            await asyncio.wait_for(stop.wait(), timeout=args.listen)
        except asyncio.TimeoutError:
            print(
                f"(listened {args.listen}s without completion — BLE link may "
                "have dropped; the command keeps running on the board)",
                file=sys.stderr,
            )
            return EXIT_TIMEOUT
    if result["line"] == "ok":
        return EXIT_OK
    return EXIT_ERROR_RESPONSE


async def cmd_scan(args: argparse.Namespace) -> int:
    devices = await BleakScanner.discover(
        timeout=args.scan_timeout, service_uuids=[NUS_SERVICE_UUID]
    )
    if not devices:
        print("no device advertising the NUS service", file=sys.stderr)
        return EXIT_NOT_FOUND
    for d in devices:
        print(f"{d.address}  name={d.name!r}")
    return EXIT_OK


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="BLE helper for the Uno R4 drawbot (Nordic UART service)."
    )
    parser.add_argument("--address", default=DEFAULT_ADDRESS,
                        help="BLE address (default: the known drawbot address)")
    parser.add_argument("--scan-timeout", type=float, default=10.0)
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_scan = sub.add_parser("scan", help="list devices advertising NUS")
    p_scan.set_defaults(func=cmd_scan)

    p_send = sub.add_parser("send", help="send one short command, wait for ok/error")
    p_send.add_argument("command")
    p_send.add_argument("--timeout", type=float, default=15.0)
    p_send.set_defaults(func=cmd_send)

    p_fire = sub.add_parser(
        "fire",
        help="send a long-running command; listen for --listen seconds "
             "(BLE may drop mid-run, the board keeps going)",
    )
    p_fire.add_argument("command")
    p_fire.add_argument("--listen", type=float, default=45.0)
    p_fire.set_defaults(func=cmd_fire)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    return asyncio.run(args.func(args))


if __name__ == "__main__":
    sys.exit(main())
