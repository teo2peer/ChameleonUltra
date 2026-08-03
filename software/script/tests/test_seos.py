#!/usr/bin/env python3
import os
import sys
import unittest
from types import SimpleNamespace

CURRENT_DIR = os.path.split(os.path.abspath(__file__))[0]
sys.path.append(CURRENT_DIR.rsplit(os.sep, 1)[0])

from chameleon_cmd import ChameleonCMD  # noqa: E402
from chameleon_enum import Command, Status  # noqa: E402


class FakeDevice:
    def __init__(self, response_data=b""):
        self.response_data = response_data
        self.calls = []

    def send_cmd_sync(self, command, data=None):
        self.calls.append((command, data))
        return SimpleNamespace(
            status=Status.SUCCESS,
            data=self.response_data,
            parsed=None,
        )


class TestSeosCommands(unittest.TestCase):
    def test_read_parses_exact_length_prefixed_response(self):
        payload = b"\x02ab\x01o\x02xy\x03div\x07\x09"
        command = ChameleonCMD(FakeDevice(payload))
        self.assertEqual(
            command.seos_read_emu_data(),
            {
                "data": b"ab",
                "oid": b"o",
                "tag": b"xy",
                "diversifier": b"div",
                "hash_alg": 7,
                "encr_alg": 9,
            },
        )

    def test_read_rejects_truncated_and_trailing_responses(self):
        malformed = (
            b"",
            b"\x02a",
            b"\x00\x00\x00\x00\x07",
            b"\x00\x00\x00\x00\x07\x09\x00",
            b"\x00\x00\x03abc\x00\x07\x09",
        )
        for payload in malformed:
            with self.subTest(payload=payload), self.assertRaises(ValueError):
                ChameleonCMD(FakeDevice(payload)).seos_read_emu_data()

    def test_write_uses_renumbered_command_and_validates_fields(self):
        device = FakeDevice()
        command = ChameleonCMD(device)
        command.seos_write_emu_data(b"ab", b"o", b"xy", b"div", 7, 9)
        self.assertEqual(device.calls[0][0], Command.SEOS_WRITE_EMU_DATA)
        self.assertEqual(device.calls[0][1], b"\x02ab\x01o\x02xy\x03div\x07\x09")
        with self.assertRaises(ValueError):
            command.seos_write_emu_data(b"", b"", b"abc", b"", 7, 9)
        with self.assertRaises(ValueError):
            command.seos_write_emu_data(b"ab", b"o", b"xy", b"div", 6, 9)

    def test_key_write_requires_three_aes_keys(self):
        device = FakeDevice()
        command = ChameleonCMD(device)
        command.seos_write_emu_keys(b"a" * 16, b"b" * 16, b"c" * 16)
        self.assertEqual(device.calls[0][0], Command.SEOS_WRITE_EMU_KEYS)
        self.assertEqual(len(device.calls[0][1]), 48)
        with self.assertRaises(ValueError):
            command.seos_write_emu_keys(b"a" * 15, b"b" * 16, b"c" * 16)


if __name__ == "__main__":
    unittest.main()
