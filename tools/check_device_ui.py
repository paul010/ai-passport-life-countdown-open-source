#!/usr/bin/env python3
"""Check the actual display/navigation, preserving profile and game values.

Serial diagnostic keys are not physical-button proof. Optional snapshots are
read-only device framebuffers, not photographs. Clock synchronization is an
explicit option. By default this never confirms reset or rewards. The optional
strictly guarded reward test writes one new claim, checks duplicates and undoes
only that claim; it never resets existing progress.
"""
import argparse
import datetime
import hashlib
import json
import time
from pathlib import Path

import serial
from device_serial_lines import SerialLines


class DeviceUI:
    def __init__(self, path):
        self.port = serial.Serial(port=None, baudrate=115200, timeout=0.2)
        self.port.dtr = False
        self.port.rts = False
        self.port.port = path
        self.port.open()
        self.port.reset_input_buffer()
        self.lines = SerialLines(self.port)
        self.minimum_heap = self.minimum_lv = 2**32
        self.keys = 0
        self.recent_actions = []
        self.initial_untagged = None
        self.request = int(time.time() * 1000) % 2000000000

    def next_request(self):
        self.request += 1
        return self.request

    def send(self, command):
        self.port.write((command + "\n").encode("ascii"))
        self.port.flush()

    def reply(self, command, timeout=5):
        if command in ("LQ_STATE", "LQ_PROFILE", "LQ_LAYOUT"):
            command += " " + str(self.next_request())
        self.send(command)
        until = time.monotonic() + timeout
        while time.monotonic() < until:
            frame = self.lines.read(max(0, until - time.monotonic()))
            if frame is None:
                break
            line = frame.decode("utf-8", "replace").strip()
            if line.startswith(command + " "):
                return json.loads(line[len(command) + 1:])
            if any(error in line for error in ("assert failed", "Guru Meditation", "watchdog")):
                raise AssertionError("Device crash detected")
        raise TimeoutError("No reply to " + command)

    def ready(self):
        for _ in range(15):
            try:
                value = self.state()
                self.initial_untagged = value.get("untagged_inputs")
                return value
            except TimeoutError:
                time.sleep(0.3)
        raise TimeoutError("Device not ready; connect and wake it")

    def state(self):
        for _ in range(30):
            value = self.reply("LQ_STATE")
            assert value["storage"], "Device reports a preserved storage error"
            self.minimum_heap = min(self.minimum_heap, value["free_heap"])
            self.minimum_lv = min(self.minimum_lv, value["lv_free"])
            assert value["free_heap"] > 12000 and value["lv_free"] > 2000
            if not value["pending"]:
                return value
            time.sleep(0.1)
        raise AssertionError("Save worker did not settle")

    def key(self, key):
        before = self.state()
        if self.initial_untagged is not None:
            assert before["untagged_inputs"] == self.initial_untagged, \
                "Local button activity: stop without sending another diagnostic key"
        self.keys += 1
        request = self.next_request()
        self.send(f"LQ_KEY {key} {request}")
        until = time.monotonic() + 8
        acknowledged = False
        while time.monotonic() < until:
            frame = self.lines.read(max(0, until - time.monotonic()))
            if frame is None:
                break
            line = frame.strip()
            if line == f"LQ_KEY_ACK {request} {key}".encode("ascii"):
                acknowledged = True
                break
            if line == f"LQ_KEY_BUSY {request}".encode("ascii"):
                raise AssertionError("Diagnostic key queue full; stopped without retrying an action")
        if not acknowledged:
            raise TimeoutError("Diagnostic key was not acknowledged")
        time.sleep(0.08)
        value = self.state()
        self.recent_actions.append({"key": key, "page": value["page"], "choice": value["choice"],
                                    "pending": value["pending"], "request": request,
                                    "last_input": value.get("last_input"),
                                    "untagged_inputs": value.get("untagged_inputs"),
                                    "last_untagged_input": value.get("last_untagged_input"),
                                    "adc_sample_errors": value.get("adc_sample_errors"),
                                    "input_count": value.get("input_count"), "button_mv": value.get("button_mv")})
        self.recent_actions = self.recent_actions[-10:]
        assert value["last_input"]["request"] == request, "Local input interrupted a diagnostic action"
        assert value["input_count"] == before["input_count"] + 1, "Unexpected extra input"
        return value

    def home_item(self, index):
        value = self.key("HOME")
        assert value["page"] == value["choice"] == 0, "Return-home input interrupted"
        for choice in range(index):
            value = self.key("DOWN")
            assert value["page"] == 0 and value["choice"] == choice + 1, "Home selection interrupted"
        return self.key("OK")

    def capture(self, destination):
        from PIL import Image
        self.send("FAP_SCREENSHOT_V1")
        until = time.monotonic() + 10
        while time.monotonic() < until:
            frame = self.lines.read(max(0, until - time.monotonic()))
            if frame is None:
                break
            if frame.strip() == b"FAP_SCREENSHOT_V1 240 320 RGB565LE 153600":
                break
        else:
            raise TimeoutError("Device screenshot header missing")
        if frame is None or frame.strip() != b"FAP_SCREENSHOT_V1 240 320 RGB565LE 153600":
            raise TimeoutError("Device screenshot header missing")
        pixels = bytearray()
        until = time.monotonic() + 30
        while len(pixels) < 153600 and time.monotonic() < until:
            pixels.extend(self.port.read(153600 - len(pixels)))
        assert len(pixels) == 153600, "Truncated device frame"
        rgb = bytearray()
        for offset in range(0, len(pixels), 2):
            value = pixels[offset] | pixels[offset + 1] << 8
            rgb.extend(((value >> 11) * 255 // 31, ((value >> 5) & 63) * 255 // 63,
                        (value & 31) * 255 // 31))
        Image.frombytes("RGB", (240, 320), bytes(rgb)).save(destination)
        return hashlib.sha256(destination.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--sync-clock", action="store_true")
    parser.add_argument("--test-reversible-reward", action="store_true",
                        help="Only for zero-XP, empty tasks already dated today; undo the sole test claim")
    parser.add_argument("--screenshots-directory", type=Path)
    args = parser.parse_args()
    device = DeviceUI(args.port)
    captures = {}
    checked_pages = set()

    def shot(name):
        layout = device.reply("LQ_LAYOUT")
        assert layout["clipped_text_or_images"] == 0, "Device content clips on page " + str(layout["page"])
        checked_pages.add(layout["page"])
        if args.screenshots_directory:
            destination = args.screenshots_directory / f"life-quest-device-{name}-130.png"
            captures[destination.name] = device.capture(destination)

    try:
        before = device.ready()
        assert before["version"] == "1.3.0-life-quest"
        initial_profile = device.reply("LQ_PROFILE")
        if args.sync_clock:
            now = datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=8)))
            device.send("LQ_CLOCK " + now.strftime("%Y-%m-%d %H:%M"))
            time.sleep(0.3)
            assert device.state()["clock_checked"]
        if args.screenshots_directory:
            args.screenshots_directory.mkdir(parents=True, exist_ok=True)
        reward_tested = False
        if args.test_reversible_reward:
            current = device.state()
            assert current["clock_checked"] and current["latest_day"] == current["day"]
            assert current["xp"] == current["count"] == current["tipo_used"] == 0, \
                "Reward test refused: existing progress must not be altered"
            assert device.home_item(0)["page"] == 1
            claimed = False
            try:
                claim = device.key("OK")
                assert claim["xp"] == 20 and claim["count"] == 1
                claimed = True
                shot("reward-feedback")
                assert device.key("OK")["xp"] == 20  # no duplicate reward
            finally:
                if claimed:
                    own_claim = device.state()
                    assert own_claim["xp"] == 20 and own_claim["count"] == 1 and \
                        own_claim["latest_day"] == current["day"], \
                        "Unexpected concurrent progress; refusing to undo someone else's action"
                    assert device.home_item(0)["page"] == 1
                    for _ in range(3):
                        device.key("DOWN")
                    assert device.state()["page"] == 1 and device.state()["choice"] == 3
                    restored = device.key("OK")  # undo only this newly created claim
            for field in ("xp", "count", "hp", "latest_day", "tipo_used", "partner", "sound", "motion"):
                assert restored[field] == current[field], "Test claim was not fully undone: " + field
            reward_tested = True
        device.key("HOME")
        shot("home")
        assert device.key("UP")["choice"] == 8
        assert device.key("OK")["page"] == 3
        shot("settings")
        assert device.key("OK")["page"] == 4
        for field in range(5):
            assert device.state()["choice"] == field
            device.key("UP")  # draft only, never save
            if field == 0:
                shot("profile-draft")
            if field < 4:
                device.key("OK")
        device.key("HOME")
        assert device.reply("LQ_PROFILE")["birthday"] == initial_profile["birthday"]
        device.home_item(8)
        device.key("DOWN")
        assert device.key("OK")["page"] == 5
        shot("clock")
        for field in range(5):
            assert device.state()["choice"] == field
            device.key("UP")
            if field < 4:
                device.key("OK")
        device.key("HOME")  # cancel changed clock draft
        assert device.home_item(0)["page"] == 1
        shot("tasks")
        for choice in range(8):
            assert device.state()["choice"] == choice, f"Task focus did not reach {choice}"
            if choice < 7:
                device.key("DOWN")
        assert device.key("OK")["page"] == 3  # last bottom tab = Settings
        assert device.home_item(7)["page"] == 2
        for section in range(5):
            assert device.state()["life_page"] == section
            shot(("numbers", "tokens", "china", "world", "milestones")[section])
            if section < 4:
                device.key("DOWN")
        for _ in range(4):
            device.key("DOWN")
        assert device.key("OK")["page"] == 3
        for _ in range(4):
            device.key("DOWN")
        assert device.key("OK")["page"] == 6
        shot("reset-cancel")
        assert device.key("OK")["page"] == 3  # default is cancel
        # Repeated page-tree replacement checks memory stability on real LVGL.
        for _ in range(10):
            device.home_item(0)
            device.home_item(7)
            device.home_item(8)
        device.key("HOME")
        after = device.state()
        profile = device.reply("LQ_PROFILE")
        for field in ("name", "birthday", "planning_age"):
            assert profile[field] == initial_profile[field], "Canceled draft modified profile"
        for field in ("xp", "latest_day", "tipo_used", "partner", "sound", "motion"):
            assert after[field] == before[field], "Navigation modified " + field
        assert after["untagged_inputs"] == before["untagged_inputs"], \
            "Uncommanded button activity during unattended regression"
        shot("home")
        report = {"version": after["version"], "source": "actual-device-serial-diagnostics",
                  "physical_buttons_verified": False, "profile_and_game_unchanged": True,
                  "one_test_claim_duplicate_and_exact_undo": reward_tested,
                  "actual_device_pages_bounds_checked": sorted(checked_pages),
                  "diagnostic_keys": device.keys, "minimum_observed_heap": device.minimum_heap,
                  "minimum_observed_lv_pool": device.minimum_lv, "final_page": after["page"],
                  "clock_checked": after["clock_checked"], "screenshots_sha256": captures}
        report["untagged_inputs_during_check"] = after["untagged_inputs"] - before["untagged_inputs"]
        report["adc_failed_samples_ignored"] = after["adc_sample_errors"] - before["adc_sample_errors"]
        if args.screenshots_directory:
            (args.screenshots_directory / "ui-validation-130.json").write_text(
                json.dumps(report, indent=2) + "\n")
        print("Actual-device navigation, canceled edits, reset cancellation and repeated redraw: PASS")
        print(json.dumps(report, indent=2))
    except Exception:
        print("Recent diagnostic actions:", json.dumps(device.recent_actions), flush=True)
        raise
    finally:
        device.port.close()


if __name__ == "__main__":
    main()
