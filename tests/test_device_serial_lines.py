import sys
import unittest
from collections import deque
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from device_serial_lines import SerialLines


class Port:
    def __init__(self, chunks):
        self.chunks = deque(chunks)

    def readline(self):
        return self.chunks.popleft() if self.chunks else b""


class SerialLineTests(unittest.TestCase):
    def test_fragmented_json_and_multiple_lines(self):
        reader = SerialLines(Port([b'LQ_STATE 17 {"choice":', b'', b'4}\nNEXT\n']))
        self.assertEqual(reader.read(1), b'LQ_STATE 17 {"choice":4}\n')
        self.assertEqual(reader.read(1), b'NEXT\n')

    def test_timeout_retains_partial_message(self):
        port = Port([b'LQ_KEY_ACK 20 '])
        reader = SerialLines(port)
        self.assertIsNone(reader.read(0.005))
        port.chunks.append(b'DOWN\n')
        self.assertEqual(reader.read(1), b'LQ_KEY_ACK 20 DOWN\n')

    def test_snapshot_header_leaves_no_buffered_binary(self):
        reader = SerialLines(Port([b'FAP_SCREENSHOT_V1 240 ', b'320 RGB565LE 153600\n']))
        self.assertEqual(reader.read(1), b'FAP_SCREENSHOT_V1 240 320 RGB565LE 153600\n')
        self.assertEqual(reader.buffer, b'')


if __name__ == '__main__':
    unittest.main()
