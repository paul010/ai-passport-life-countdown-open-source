import sys
import types
import unittest
from collections import deque
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
try:
    import serial
except ImportError:
    sys.modules['serial'] = types.ModuleType('serial')  # no hardware in host tests
from check_device_ui import DeviceUI


class Reader:
    def __init__(self, frames):
        self.frames = deque(frames)

    def read(self, timeout):
        return self.frames.popleft() if self.frames else None


class DeviceProtocolTests(unittest.TestCase):
    def device(self, frames, after=None):
        device = DeviceUI.__new__(DeviceUI)
        device.request = 40
        device.keys = 0
        device.initial_untagged = 0
        device.recent_actions = []
        device.lines = Reader(frames)
        device.commands = []
        device.send = device.commands.append
        before = {'untagged_inputs': 0, 'input_count': 3}
        states = deque([before, after])
        device.state = lambda: states.popleft()
        return device

    def test_missing_ack_stops_without_resending(self):
        device = self.device([])
        with self.assertRaises(TimeoutError):
            device.key('OK')
        self.assertEqual(device.commands, ['LQ_KEY OK 41'])

    def test_unmatched_old_ack_is_not_success(self):
        device = self.device([b'LQ_KEY_ACK 39 OK\n'])
        with self.assertRaises(TimeoutError):
            device.key('OK')
        self.assertEqual(len(device.commands), 1)

    @patch('check_device_ui.time.sleep')
    def test_correlated_ack_and_exactly_one_input(self, sleep):
        after = {'page': 1, 'choice': 0, 'pending': False, 'untagged_inputs': 0,
                 'input_count': 4, 'last_input': {'request': 41}}
        device = self.device([b'LQ_KEY_ACK 41 OK\n'], after)
        self.assertEqual(device.key('OK'), after)

    @patch('check_device_ui.time.sleep')
    def test_extra_local_input_aborts(self, sleep):
        after = {'page': 0, 'choice': 0, 'pending': False, 'untagged_inputs': 1,
                 'input_count': 5, 'last_input': {'request': 0}}
        device = self.device([b'LQ_KEY_ACK 41 OK\n'], after)
        with self.assertRaises(AssertionError):
            device.key('OK')
        self.assertEqual(len(device.commands), 1)

    def test_local_activity_stops_before_write(self):
        device = self.device([])
        device.state = lambda: {'untagged_inputs': 1, 'input_count': 4}
        with self.assertRaises(AssertionError):
            device.key('OK')
        self.assertEqual(device.commands, [])


if __name__ == '__main__':
    unittest.main()
