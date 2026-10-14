"""Reassemble newline-framed messages across short serial read timeouts."""
import time


class SerialLines:
    def __init__(self, port, clock=time.monotonic):
        self.port = port
        self.clock = clock
        self.buffer = bytearray()

    def read(self, timeout):
        deadline = self.clock() + timeout
        while self.clock() < deadline:
            end = self.buffer.find(b"\n")
            if end >= 0:
                line = bytes(self.buffer[:end + 1])
                del self.buffer[:end + 1]
                return line
            self.buffer.extend(self.port.readline())
            if len(self.buffer) > 16384:
                raise ValueError("Unexpected oversized serial text frame")
        return None  # retain an unfinished line; never parse partial JSON
