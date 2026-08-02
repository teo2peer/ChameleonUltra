#!/usr/bin/env python3
"""Hardware-free command-1050 wire and decoder tests."""

import os
import struct
import sys
import unittest

CURRENT_DIR = os.path.split(os.path.abspath(__file__))[0]
sys.path.append(CURRENT_DIR.rsplit(os.sep, 1)[0])

import chameleon_com  # noqa: E402
from chameleon_cmd import ChameleonCMD  # noqa: E402
from chameleon_enum import Command, Status, TagSpecificType  # noqa: E402
from chameleon_utils import UnexpectedResponseError  # noqa: E402


class FakeDevice:
    def __init__(self, responses):
        self.responses = list(responses)
        self.requests = []
        self.closed = False

    def send_cmd_sync(self, command, data=None, timeout=3):
        self.requests.append((command, bytes(data or b""), timeout))
        status, payload = self.responses.pop(0)
        return chameleon_com.Response(command, status, payload)

    def close(self):
        self.closed = True


class TestActiveSlotSnapshot(unittest.TestCase):
    def test_command_id_and_exact_round_trip(self):
        revision = 0x10203040
        owner_generation = 0x55667788
        begin = struct.pack(
            "!BBBHII", 2, 0, 3, TagSpecificType.MIFARE_4096,
            owner_generation, revision)
        device = FakeDevice([
            (Status.SUCCESS, begin),
            (Status.SUCCESS, struct.pack("!BBI", 2, 1, revision)),
        ])
        cmd = ChameleonCMD(device)

        transaction = cmd.active_slot_snapshot_begin()
        saved_revision = cmd.active_slot_snapshot_save_release(revision)

        self.assertEqual(int(Command.ACTIVE_SLOT_SNAPSHOT), 1050)
        self.assertEqual(transaction["slot"], 3)
        self.assertEqual(transaction["tag_type"], TagSpecificType.MIFARE_4096)
        self.assertEqual(transaction["owner_generation"], owner_generation)
        self.assertEqual(transaction["revision"], revision)
        self.assertEqual(saved_revision, revision)
        self.assertEqual(device.requests, [
            (Command.ACTIVE_SLOT_SNAPSHOT, b"\x02\x00", 3),
            (Command.ACTIVE_SLOT_SNAPSHOT,
             b"\x02\x01\x10\x20\x30\x40", 55),
        ])

    def test_abort_uses_big_endian_revision(self):
        revision = 0x89ABCDEF
        device = FakeDevice([
            (Status.SUCCESS, struct.pack("!BBI", 2, 2, revision)),
        ])

        self.assertEqual(
            ChameleonCMD(device).active_slot_snapshot_abort(revision), revision)
        self.assertEqual(device.requests[0][1], b"\x02\x02\x89\xAB\xCD\xEF")
        self.assertEqual(device.requests[0][2], 5)

    def test_malformed_begin_aborts_when_revision_is_available(self):
        valid = struct.pack(
            "!BBBHII", 2, 0, 0, TagSpecificType.MIFARE_1024, 3, 1)
        malformed = (
            valid + b"\x00",
            struct.pack("!BBBHII", 3, 0, 0, TagSpecificType.MIFARE_1024, 3, 1),
            struct.pack("!BBBHII", 2, 1, 0, TagSpecificType.MIFARE_1024, 3, 1),
            struct.pack("!BBBHII", 2, 0, 8, TagSpecificType.MIFARE_1024, 3, 1),
            struct.pack("!BBBHII", 2, 0, 0, TagSpecificType.NTAG_213, 3, 1),
            struct.pack("!BBBHII", 2, 0, 0, TagSpecificType.MIFARE_1024, 0, 1),
        )
        for payload in malformed:
            with self.subTest(payload=payload.hex()):
                device = FakeDevice([
                    (Status.SUCCESS, payload),
                    (Status.SUCCESS, struct.pack("!BBI", 2, 2, 1)),
                ])
                with self.assertRaises(UnexpectedResponseError):
                    ChameleonCMD(device).active_slot_snapshot_begin()
                self.assertEqual(device.requests[-1][1],
                                 b"\x02\x02\x00\x00\x00\x01")
                self.assertFalse(device.closed)

    def test_malformed_begin_without_v2_revision_closes_transport(self):
        version_one = struct.pack(
            "!BBBHI", 1, 0, 0, TagSpecificType.MIFARE_1024, 1)
        for payload in (b"", version_one, version_one + b"\x00\x00\x00"):
            with self.subTest(payload=payload.hex()):
                device = FakeDevice([(Status.SUCCESS, payload)])
                with self.assertRaises(UnexpectedResponseError):
                    ChameleonCMD(device).active_slot_snapshot_begin()
                self.assertTrue(device.closed)
                self.assertEqual(len(device.requests), 1)

    def test_rejects_malformed_save_responses_and_closes(self):
        revision = 7
        for payload in (
                b"",
                struct.pack("!BBI", 2, 2, revision),
                struct.pack("!BBI", 2, 1, revision + 1)):
            with self.subTest(end_payload=payload.hex()):
                device = FakeDevice([(Status.SUCCESS, payload)])
                with self.assertRaises(UnexpectedResponseError):
                    ChameleonCMD(device).active_slot_snapshot_save_release(
                        revision)
                self.assertTrue(device.closed)

    def test_save_timeout_closes_transport(self):
        class TimeoutDevice(FakeDevice):
            def send_cmd_sync(self, command, data=None, timeout=3):
                self.requests.append((command, bytes(data or b""), timeout))
                raise TimeoutError("delayed FDS completion")

        device = TimeoutDevice([])
        with self.assertRaises(TimeoutError):
            ChameleonCMD(device).active_slot_snapshot_save_release(7)
        self.assertTrue(device.closed)
        self.assertEqual(device.requests[0][2], 55)

    def test_rejects_invalid_revision_before_transport(self):
        device = FakeDevice([])
        for revision in (0, -1, 0x100000000, True, None):
            with self.subTest(revision=revision):
                with self.assertRaises(ValueError):
                    ChameleonCMD(device).active_slot_snapshot_abort(revision)
        self.assertEqual(device.requests, [])

    def test_surfaces_flash_failure_without_decoding_payload(self):
        device = FakeDevice([(Status.FLASH_WRITE_FAIL, b"")])
        with self.assertRaises(UnexpectedResponseError):
            ChameleonCMD(device).active_slot_snapshot_save_release(1)


if __name__ == "__main__":
    unittest.main()
