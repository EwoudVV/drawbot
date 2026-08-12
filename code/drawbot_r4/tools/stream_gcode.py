#!/usr/bin/env python3
"""Stream newline-delimited G-code to the drawbot over USB serial or BLE."""

from __future__ import annotations

import argparse
import asyncio
from pathlib import Path
import re
import sys
import time

# ---------------------------------------------------------------------------
# USB (pyserial) path
# ---------------------------------------------------------------------------

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None
    list_ports = None

# ---------------------------------------------------------------------------
# BLE (bleak) path
# ---------------------------------------------------------------------------

try:
    import bleak
except ImportError:
    bleak = None


BAUD = 115200

NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_TX_CHAR_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX_CHAR_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

# ---------------------------------------------------------------------------
# Shared helpers
# ---------------------------------------------------------------------------


def commands_from_file(path: Path) -> list[str]:
    commands: list[str] = []
    for line_number, raw_line in enumerate(path.read_text().splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith(";") or (
            line.startswith("(") and line.endswith(")")
        ):
            continue
        encoded = line.encode("ascii", errors="strict")
        if len(encoded) > 96:
            raise SystemExit(f"{path}:{line_number}: line exceeds 96 bytes")
        commands.append(line)
    if not commands:
        raise SystemExit(f"{path}: no commands to send")
    return commands


# ---------------------------------------------------------------------------
# USB serial implementation
# ---------------------------------------------------------------------------


def choose_port(requested: str | None) -> str:
    if requested:
        return requested
    if list_ports is None:
        raise SystemExit(
            "pyserial is required for USB. Install it with: python3 -m pip install pyserial"
        )

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

    details = "\n".join(
        f"  {port.device}: {port.description}" for port in ports
    ) or "  (none found)"
    raise SystemExit(
        "Could not choose one serial port. Pass --port explicitly.\n" + details
    )


def read_protocol_line(connection: serial.Serial, deadline: float) -> str:
    while time.monotonic() < deadline:
        raw = connection.readline()
        if raw:
            return raw.decode("utf-8", errors="replace").strip()
    raise TimeoutError("timed out waiting for the drawbot")


def usb_stream(args: argparse.Namespace, commands: list[str]) -> int:
    if serial is None:
        raise SystemExit("pyserial is required. Install it with: python3 -m pip install pyserial")
    port = choose_port(args.port)
    print(f"Opening {port} at {BAUD} baud")

    try:
        with serial.Serial(port, BAUD, timeout=0.2, write_timeout=2.0) as connection:
            connection.dtr = False
            time.sleep(0.1)
            connection.reset_input_buffer()
            connection.dtr = True
            _wait_for_ready(lambda: read_protocol_line(connection, time.monotonic() + args.ready_timeout))
            for command in commands:
                _send_one_usb(connection, command, args.command_timeout)
    except (serial.SerialException, TimeoutError, RuntimeError) as exc:
        print(f"Stopped: {exc}", file=sys.stderr)
        return 1
    print(f"Completed {len(commands)} commands.")
    return 0


def _send_one_usb(connection: serial.Serial, command: str, timeout: float) -> None:
    print(f"> {command}")
    connection.write(command.encode("ascii") + b"\n")
    connection.flush()
    deadline = time.monotonic() + timeout
    while True:
        line = read_protocol_line(connection, deadline)
        if not line:
            continue
        print(f"< {line}")
        if line == "ok":
            return
        if line.startswith("error:"):
            raise RuntimeError(line)
        if line == "ready":
            raise RuntimeError("controller reset while a command was in flight")


# ---------------------------------------------------------------------------
# BLE implementation (bleak) — drop-tolerant with position-reconciled resume
# ---------------------------------------------------------------------------

_POS_RE = re.compile(r"X:([-+]?\d*\.?\d+)\s+Y:([-+]?\d*\.?\d+)\s+homed:(yes|no)")
_WORD_RE = re.compile(r"(X|Y)\s*([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)\b")
_MOVE_RE = re.compile(r"^(?:G0|G1|G00|G01)\b")
_M50_RE = re.compile(r"^M50\b")


class TrackedPosition:
    """The position the machine WILL have once every acked command is applied."""

    def __init__(self) -> None:
        self.x = 0.0
        self.y = 0.0

    def apply(self, command: str) -> None:
        if not (_MOVE_RE.search(command) or _M50_RE.search(command)):
            return
        words = dict(_WORD_RE.findall(command))
        if "X" in words:
            self.x = float(words["X"])
        if "Y" in words:
            self.y = float(words["Y"])

    def move_delta(self, command: str) -> tuple[float, float] | None:
        """Position delta a move command produces, or None if it is not a move."""
        if not _MOVE_RE.search(command):
            return None
        words = dict(_WORD_RE.findall(command))
        dx = float(words["X"]) - self.x if "X" in words else 0.0
        dy = float(words["Y"]) - self.y if "Y" in words else 0.0
        return dx, dy

    def near(self, x: float, y: float, tol: float = 0.1) -> bool:
        return abs(self.x - x) <= tol and abs(self.y - y) <= tol


def _make_line_handler(received: list[str]):
    recv = bytearray()

    def handler(_sender, data: bytearray):
        nonlocal recv
        recv.extend(data)
        while b"\n" in recv:
            raw, recv = recv.split(b"\n", 1)
            line = raw.decode("utf-8", errors="replace").strip()
            if line:
                print(f"< {line}")
                received.append(line)

    return handler


async def _find_rx_characteristic(client: bleak.BleakClient):
    for svc in client.services:
        if svc.uuid.lower() == NUS_SERVICE_UUID:
            for char in svc.characteristics:
                if char.uuid.lower() == NUS_RX_CHAR_UUID:
                    return char
    return None


async def _write_line(client: bleak.BleakClient, rx_char, command: str) -> None:
    """Write a line in <=20-byte chunks with pacing.

    The NUS characteristic is 20 bytes; the Uno R4's BLE stack overwrites
    pending values if writes arrive faster than the firmware drains them,
    which corrupts multi-chunk lines. Pacing lets each chunk land cleanly.
    """
    payload = command.encode("ascii") + b"\n"
    for i in range(0, len(payload), 20):
        await client.write_gatt_char(rx_char, payload[i:i + 20])
        await asyncio.sleep(0.015)
    await asyncio.sleep(0.03)


async def _send_line(
    client: bleak.BleakClient,
    rx_char,
    received: list[str],
    disconnect_event: asyncio.Event,
    command: str,
    timeout: float,
) -> str | None:
    """Send one line and wait for 'ok'/'error:...'.

    Returns 'ok', 'error:...', or None if the link dropped / timed out before
    an answer arrived (the board may or may not have executed the command).
    """
    print(f"> {command}")
    await _write_line(client, rx_char, command)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if disconnect_event.is_set():
            return None
        for line in received:
            if line == "ok":
                received.remove(line)
                return "ok"
            if line.startswith("error:"):
                received.remove(line)
                return line
            if line == "ready":
                received.remove(line)
                return None  # board reset: treat like a drop, reconcile on resume
        await asyncio.sleep(0.05)
    if disconnect_event.is_set():
        return None
    raise TimeoutError(f"no reply within {timeout:.0f}s for: {command}")


async def _query_position(
    client: bleak.BleakClient,
    rx_char,
    received: list[str],
    disconnect_event: asyncio.Event,
    timeout: float,
) -> tuple[float, float, bool] | None:
    response = await _send_line(client, rx_char, received, disconnect_event, "M114", timeout)
    if response is None or response != "ok":
        return None
    for line in received:
        match = _POS_RE.search(line)
        if match:
            received.remove(line)
            return float(match.group(1)), float(match.group(2)), match.group(3) == "yes"
    return None


async def _connect_with_retry(address: str, attempts: int, delay: float,
                              disconnected_callback) -> bleak.BleakClient | None:
    for attempt in range(1, attempts + 1):
        try:
            client = bleak.BleakClient(address, timeout=60,
                                       disconnected_callback=disconnected_callback)
            await client.connect()
            return client
        except Exception as exc:
            print(f"  connect attempt {attempt}/{attempts} failed: {exc}")
            if attempt < attempts:
                await asyncio.sleep(delay)
    return None


async def ble_stream(args: argparse.Namespace, commands: list[str]) -> int:
    if bleak is None:
        raise SystemExit("bleak is required for BLE. Install it with: python3 -m pip install bleak")

    address = await _find_drawbot(args)
    if address is None:
        print("Drawbot not found.", file=sys.stderr)
        return 1

    expected = TrackedPosition()
    next_index = 0  # next command to transmit
    total = len(commands)
    print(f"Streaming {total} commands to {address} (drop-tolerant resume).")

    while next_index < total:
        received: list[str] = []
        disconnect_event = asyncio.Event()

        def disconnected_callback(_client):
            disconnect_event.set()

        client = await _connect_with_retry(address, 10, 3.0, disconnected_callback)
        if client is None:
            print("giving up: could not reconnect to the drawbot", file=sys.stderr)
            return 1
        try:
            await client.start_notify(NUS_TX_CHAR_UUID, _make_line_handler(received))
            rx_char = await _find_rx_characteristic(client)
            if rx_char is None:
                print("Could not find NUS RX characteristic.", file=sys.stderr)
                await client.disconnect()
                return 1

            # Flush any partial line left by a previous aborted session.
            await _write_line(client, rx_char, "")
            await asyncio.sleep(0.2)

            # Reconcile with the board before resuming.
            position = await _query_position(client, rx_char, received,
                                             disconnect_event, args.command_timeout)
            if position is None:
                print("  lost link during reconciliation; reconnecting...")
                await client.disconnect()
                continue
            board_x, board_y, board_homed = position
            if not board_homed:
                # Operator parks the carriage at the home corner before the
                # job starts, so the physical position is known. After a
                # mid-job reset the gantry never moved, so the tracked
                # position is still exact.
                home_x = expected.x if next_index > 0 else 0.0
                home_y = expected.y if next_index > 0 else 0.0
                print(f"  board not homed; marking M50 X{home_x:.3f} Y{home_y:.3f}")
                ok = await _send_line(client, rx_char, received, disconnect_event,
                                      f"M50 X{home_x:.3f} Y{home_y:.3f}",
                                      args.command_timeout)
                if ok != "ok":
                    print(f"  could not mark position ({ok}); stopping", file=sys.stderr)
                    await client.disconnect()
                    return 2
            elif next_index > 0:
                pending = commands[next_index]
                if expected.near(board_x, board_y):
                    pass  # pending command never executed — resend it
                else:
                    delta = expected.move_delta(pending)
                    if delta is not None:
                        after_x, after_y = expected.x + delta[0], expected.y + delta[1]
                        if abs(board_x - after_x) <= 0.1 and abs(board_y - after_y) <= 0.1:
                            # Pending move already executed; skip it.
                            expected.x, expected.y = board_x, board_y
                            next_index += 1
                            print(f"  pending move already executed; skipping to line {next_index + 1}")
                        else:
                            print(f"  WARNING position drift: board X{board_x} Y{board_y} "
                                  f"vs tracked X{expected.x} Y{expected.y}; adopting board position")
                            expected.x, expected.y = board_x, board_y
                    else:
                        print(f"  WARNING position drift: board X{board_x} Y{board_y} "
                              f"vs tracked X{expected.x} Y{expected.y}; adopting board position")
                        expected.x, expected.y = board_x, board_y

            # Main send loop.
            while next_index < total:
                command = commands[next_index]
                response = await _send_line(client, rx_char, received, disconnect_event,
                                            command, args.command_timeout)
                if response == "ok":
                    expected.apply(command)
                    next_index += 1
                elif response is not None and response.startswith("error:"):
                    print(f"Board rejected {command}: {response}", file=sys.stderr)
                    await client.disconnect()
                    return 2
                else:
                    print("  link lost mid-command; reconnecting to resume...")
                    break
        except TimeoutError as exc:
            print(f"Stopped: {exc}", file=sys.stderr)
            await client.disconnect()
            return 3
        finally:
            try:
                await client.disconnect()
            except Exception:
                pass
        if next_index < total:
            await asyncio.sleep(3.0)

    print(f"Completed {total} commands.")
    return 0


async def _find_drawbot(args: argparse.Namespace) -> str | None:
    if args.address:
        return args.address

    scanner = bleak.BleakScanner()
    devices = await scanner.discover(timeout=args.ble_scan_timeout)
    for d in devices:
        if d.name and "drawbot" in d.name.lower():
            return d.address
    # macOS often leaves advertised names unresolved (name=None); fall back to
    # scanning for the Nordic UART service itself.
    nus_devices = await bleak.BleakScanner.discover(
        timeout=args.ble_scan_timeout, service_uuids=[NUS_SERVICE_UUID]
    )
    for d in nus_devices:
        if d.name and "drawbot" in d.name.lower():
            return d.address
    if nus_devices:
        return nus_devices[0].address
    return None


async def _wait_for_ble_ready(client: bleak.BleakClient, received: list[str], timeout: float) -> None:
    # The board only prints "ready" at boot. If it is already running when we
    # connect, no fresh "ready" will ever arrive — that is the normal case,
    # not an error.
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        for line in received:
            if line == "ready":
                received.clear()
                return
        await asyncio.sleep(0.1)
    print("(board already running; no fresh 'ready' — continuing)")


# ---------------------------------------------------------------------------
# Common entrypoint (USB or BLE)
# ---------------------------------------------------------------------------


def _wait_for_ready(get_line):
    while True:
        line = get_line()
        if line:
            print(f"< {line}")
        if line == "ready":
            return


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Stream a G-code file to the Uno R4 drawbot, one ok at a time."
    )
    parser.add_argument("gcode", type=Path, help="G-code file to stream")
    parser.add_argument("--port", help="USB serial device (auto-detected if unambiguous)")
    parser.add_argument("--address", help="BLE MAC address (scan if omitted)")
    parser.add_argument("--ble", action="store_true", help="Use BLE instead of USB serial")
    parser.add_argument("--ready-timeout", type=float, default=15.0)
    parser.add_argument("--command-timeout", type=float, default=180.0,
                        help="seconds allowed per command, including a full homing cycle")
    parser.add_argument("--ble-scan-timeout", type=float, default=5.0,
                        help="BLE scan timeout in seconds")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    commands = commands_from_file(args.gcode)
    if args.ble:
        return asyncio.run(ble_stream(args, commands))
    return usb_stream(args, commands)


if __name__ == "__main__":
    raise SystemExit(main())
