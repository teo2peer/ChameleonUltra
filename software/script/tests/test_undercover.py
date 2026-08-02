#!/usr/bin/env python3

import os
import sys
import unittest

CURRENT_DIR = os.path.split(os.path.abspath(__file__))[0]
sys.path.append(CURRENT_DIR.rsplit(os.sep, 1)[0])

from chameleon_cmd import ChameleonCMD  # noqa: E402
from chameleon_enum import Command, Status  # noqa: E402


class _Response:
    def __init__(self):
        self.status = Status.SUCCESS
        self.parsed = None


class _Device:
    def __init__(self):
        self.calls = []

    def send_cmd_sync(self, command, data=None):
        self.calls.append((command, data))
        return _Response()


class TestUndercoverHostCommand(unittest.TestCase):
    def test_encodes_boolean_payload(self):
        device = _Device()
        command = ChameleonCMD(device)

        self.assertTrue(command.set_runtime_undercover_mode(True))
        self.assertTrue(command.set_runtime_undercover_mode(False))
        self.assertEqual(device.calls, [
            (Command.SET_RUNTIME_UNDERCOVER_MODE, b"\x01"),
            (Command.SET_RUNTIME_UNDERCOVER_MODE, b"\x00"),
        ])

    def test_rejects_non_boolean_payload(self):
        with self.assertRaises(ValueError):
            ChameleonCMD(_Device()).set_runtime_undercover_mode(1)


if __name__ == "__main__":
    unittest.main()
