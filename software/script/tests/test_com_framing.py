#!/usr/bin/env python3
"""Regression tests for the request-frame builder and LRC checksum.

The wire frame is: SOF(0x11) | lrc1 | cmd[2 BE] | status[2 BE] | len[2 BE] | lrc2
| data[len] | lrc3. Each lrc is chosen so that the byte span it terminates sums
to 0 (mod 256); a receiver validates by checking that same property.
"""
import os
import struct
import sys
import unittest

CURRENT_DIR = os.path.split(os.path.abspath(__file__))[0]
sys.path.append(CURRENT_DIR.rsplit(os.sep, 1)[0])

import chameleon_com  # noqa: E402

lrc = chameleon_com.ChameleonCom.lrc_calc


class TestLrc(unittest.TestCase):
    def test_complement_sum(self):
        # lrc = (0x100 - sum) & 0xFF, so byte-span-plus-its-lrc sums to 0.
        self.assertEqual(lrc(b"\x11"), 0xEF)
        self.assertEqual((0x11 + 0xEF) & 0xFF, 0)
        self.assertEqual(lrc(b""), 0)
        self.assertEqual(lrc(b"\x00\x00"), 0)


class TestFrameBuilder(unittest.TestCase):
    def setUp(self):
        # __init__ is cheap and starts no threads / opens no port.
        self.com = chameleon_com.ChameleonCom()

    def test_header_fields_are_big_endian(self):
        cmd, status, data = 1002, 0x0068, b"\x01\x02\x03"
        frame = self.com.make_data_frame_bytes(cmd, data, status)
        sof, _lrc1, f_cmd, f_status, f_len, _lrc2 = struct.unpack("!BBHHHB", frame[:9])
        self.assertEqual(sof, 0x11)
        self.assertEqual(f_cmd, cmd)
        self.assertEqual(f_status, status)
        self.assertEqual(f_len, len(data))
        self.assertEqual(frame[9:9 + len(data)], data)
        self.assertEqual(len(frame), 9 + len(data) + 1)

    def test_all_three_lrcs_are_valid(self):
        frame = self.com.make_data_frame_bytes(0x2710, b"\xaa\xbb", 0x1234)
        self.assertEqual(lrc(frame[:2]), 0)   # SOF + lrc1
        self.assertEqual(lrc(frame[:9]), 0)   # header + lrc2
        self.assertEqual(lrc(frame), 0)       # whole frame + lrc3

    def test_empty_payload(self):
        frame = self.com.make_data_frame_bytes(0x03E8, None, 0)
        self.assertEqual(len(frame), 10)      # 9 header + 0 data + 1 lrc
        self.assertEqual(lrc(frame), 0)
        _sof, _l1, f_cmd, f_status, f_len, _l2 = struct.unpack("!BBHHHB", frame[:9])
        self.assertEqual((f_cmd, f_status, f_len), (0x03E8, 0, 0))


if __name__ == "__main__":
    unittest.main()
