#!/usr/bin/env python3
"""Hardware-free tests for the aggregate device-settings response."""

import os
import struct
import sys
import unittest


CURRENT_DIR = os.path.split(os.path.abspath(__file__))[0]
sys.path.append(CURRENT_DIR.rsplit(os.sep, 1)[0])

import chameleon_com  # noqa: E402
from chameleon_cmd import ChameleonCMD  # noqa: E402
from chameleon_enum import ButtonPressFunction, Status  # noqa: E402
from chameleon_utils import UnexpectedResponseError  # noqa: E402


class FakeDevice:
    def __init__(self, payload):
        self.payload = payload

    def send_cmd_sync(self, command, data=None):
        return chameleon_com.Response(command, Status.SUCCESS, self.payload)


class TestDeviceSettings(unittest.TestCase):
    def test_reader_keys_button_value_is_supported(self):
        self.assertEqual(ButtonPressFunction.READERKEYS.value, 6)

    def test_parses_exact_v6_payload(self):
        payload = struct.pack(
            "!BBBBBBB6sB", 6, 3, 1, 2, 4, 5, 1, b"654321", 42)

        parsed = ChameleonCMD(FakeDevice(payload)).get_device_settings()

        self.assertEqual(parsed["settings_version"], 6)
        self.assertEqual(parsed["ble_pairing_key"], b"654321")
        self.assertEqual(parsed["sleep_timeout"], 42)

    def test_rejects_non_exact_v6_lengths(self):
        valid = struct.pack(
            "!BBBBBBB6sB", 6, 3, 1, 2, 4, 5, 1, b"654321", 42)
        for payload in (b"", valid[:-1], valid + b"\x00"):
            with self.subTest(length=len(payload)):
                with self.assertRaises(UnexpectedResponseError):
                    ChameleonCMD(FakeDevice(payload)).get_device_settings()


if __name__ == "__main__":
    unittest.main()
