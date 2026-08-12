#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
from pathlib import Path
import tempfile
import time


PROJECT = Path(__file__).parents[1]
SPEC = importlib.util.spec_from_file_location(
    "stream_gcode", PROJECT / "tools" / "stream_gcode.py"
)
assert SPEC and SPEC.loader
stream_gcode = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(stream_gcode)


class FakeSerial:
    def __init__(self, responses: list[bytes]) -> None:
        self.responses = iter(responses)
        self.written = bytearray()

    def readline(self) -> bytes:
        return next(self.responses, b"")

    def write(self, data: bytes) -> None:
        self.written.extend(data)

    def flush(self) -> None:
        pass


def main() -> None:
    ready = FakeSerial([b"boot note\n", b"ready\n"])
    stream_gcode._wait_for_ready(
        lambda: stream_gcode.read_protocol_line(ready, time.monotonic() + 0.1)
    )

    success = FakeSerial([b"position report\n", b"ok\n"])
    stream_gcode._send_one_usb(success, "M114", 0.1)
    assert success.written == b"M114\n"

    failure = FakeSerial([b"error: deliberate test\n"])
    try:
        stream_gcode._send_one_usb(failure, "G20", 0.1)
        raise AssertionError("firmware error was not propagated")
    except RuntimeError as error:
        assert "deliberate test" in str(error)

    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "sample.gcode"
        path.write_text("; comment\n\nG21\n(comment) G90\n")
        assert stream_gcode.commands_from_file(path) == ["G21", "(comment) G90"]

    print("All USB-sender tests passed.")


if __name__ == "__main__":
    main()
