#!/usr/bin/env python3
"""Explicit local-only Life Quest commands; never auto-reset, erase or flash."""
import argparse
import datetime
import json
import time
import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--command", default="LQ_STATE")
    parser.add_argument("--sync-clock", action="store_true")
    args = parser.parse_args()
    command = args.command
    if args.sync_clock:
        now = datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=8)))
        command = "LQ_CLOCK " + now.strftime("%Y-%m-%d %H:%M")
    allowed = ("LQ_STATE", "LQ_PROFILE", "LQ_SLEEP", "LQ_REBOOT", "LQ_KEY UP", "LQ_KEY DOWN",
               "LQ_KEY OK", "LQ_KEY HOME")
    if command not in allowed and not command.startswith("LQ_CLOCK "):
        parser.error("Unknown explicit command")
    if any(c in command for c in "\r\n") or len(command) > 79:
        parser.error("Command must be one bounded line")
    with serial.Serial(port=None, baudrate=115200, timeout=0.15) as port:
        port.dtr = False
        port.rts = False
        port.port = args.port
        port.open()
        port.reset_input_buffer()
        # Some USB drivers restart the board on opening even without asserted
        # control lines. Wait for the application before sending a mutation.
        ready = False
        until = time.monotonic() + 12
        while time.monotonic() < until:
            port.write(b"LQ_STATE\n")
            probe_until = time.monotonic() + 0.7
            while time.monotonic() < probe_until:
                reply = port.readline().decode("utf-8", "replace").strip()
                if reply.startswith("LQ_STATE "):
                    json.loads(reply.partition(" ")[2])
                    ready = True
                    break
            if ready:
                break
        if not ready:
            raise SystemExit("Application not ready; requested command was not sent.")
        port.write((command + "\n").encode("ascii"))
        port.flush()
        lines = []
        deadline = time.monotonic() + (2 if command.startswith("LQ_KEY") else 4)
        while time.monotonic() < deadline:
            line = port.readline().decode("utf-8", "replace").strip()
            if line:
                lines.append(line)
                if line.startswith("LQ_STATE ") or line.startswith("LQ_PROFILE "):
                    json.loads(line.partition(" ")[2])
                    break
                if line.startswith("LQ_CLOCK "):
                    break
        for line in lines:
            print(line)
        if command in ("LQ_STATE", "LQ_PROFILE") and not any(line.startswith(command + " ") for line in lines):
            raise SystemExit("No reply: check power, port, and installed firmware; no reset was attempted.")


if __name__ == "__main__":
    main()
