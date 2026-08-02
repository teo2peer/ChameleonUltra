#!/usr/bin/env python3
"""Hardware-free tests for keyboard command framing and CLI behavior."""

import argparse
import contextlib
import io
import os
import struct
import sys
import unittest
import zlib


CURRENT_DIR = os.path.split(os.path.abspath(__file__))[0]
sys.path.append(CURRENT_DIR.rsplit(os.sep, 1)[0])

import chameleon_com  # noqa: E402
from chameleon_cli_unit import (  # noqa: E402
    HWKeyboardArm,
    HWKeyboardCompile,
    HWKeyboardRun,
    HWKeyboardUpload,
)
from chameleon_cmd import ChameleonCMD  # noqa: E402
from chameleon_enum import Command, Status  # noqa: E402
from chameleon_utils import UnexpectedResponseError  # noqa: E402


def response(data=b"", status=Status.SUCCESS):
    return chameleon_com.Response(0, status, data)


class FakeDevice:
    def __init__(self, responses=None, handler=None):
        self.responses = list(responses or [])
        self.handler = handler
        self.calls = []

    def send_cmd_sync(self, command, data=None):
        payload = b"" if data is None else bytes(data)
        self.calls.append((command, payload))
        if self.handler is not None:
            return self.handler(command, payload)
        reply = self.responses.pop(0) if self.responses else response()
        reply.cmd = command
        return reply


class TestKeyboardWireProtocol(unittest.TestCase):
    def test_exact_request_payloads_and_response_parsing(self):
        replies = [
            response(struct.pack("!BIHH", 1, 0x12345678, 0, 4089)),
            response(struct.pack("!IH", 0x12345678, 7)),
            response(struct.pack("!IHI", 0x87654321, 10, 0x89ABCDEF)),
            response(struct.pack("!I", 0xABCDEF01)), response(),
            response(struct.pack(
                "!BBBBIIIHHHHI", 1, 3, 0, 3, 0x12345678,
                0x87654321, 0xABCDEF01, 10, 10, 4, 10, 0x89ABCDEF)),
            response(),
        ]
        device = FakeDevice(replies)
        cmd = ChameleonCMD(device)

        self.assertEqual(
            cmd.keyboard_upload_begin(10, 0x89ABCDEF)["upload_id"], 0x12345678)
        self.assertEqual(
            cmd.keyboard_upload_chunk(0x12345678, 4, b"abc")["next_offset"], 7)
        self.assertEqual(
            cmd.keyboard_upload_commit(0x12345678)["commit_id"], 0x87654321)
        self.assertEqual(cmd.keyboard_run(0x87654321, 3)["run_id"], 0xABCDEF01)
        cmd.keyboard_cancel()
        self.assertEqual(cmd.keyboard_get_status()["state_name"], "running")
        cmd.keyboard_clear()

        self.assertEqual(device.calls, [
            (Command.KEYBOARD_UPLOAD_BEGIN,
             b"\x01\x00\x0a\x89\xab\xcd\xef"),
            (Command.KEYBOARD_UPLOAD_CHUNK,
             b"\x01\x12\x34\x56\x78\x00\x04abc"),
            (Command.KEYBOARD_UPLOAD_COMMIT, b"\x01\x12\x34\x56\x78"),
            (Command.KEYBOARD_RUN, b"\x01\x87\x65\x43\x21\x03"),
            (Command.KEYBOARD_CANCEL, b""),
            (Command.KEYBOARD_GET_STATUS, b""),
            (Command.KEYBOARD_CLEAR, b""),
        ])

    def test_invalid_arguments_never_reach_transport(self):
        device = FakeDevice()
        cmd = ChameleonCMD(device)
        calls = [
            lambda: cmd.keyboard_upload_begin(0, 0),
            lambda: cmd.keyboard_upload_begin(1, -1),
            lambda: cmd.keyboard_upload_chunk(0, 0, b""),
            lambda: cmd.keyboard_upload_chunk(0, 0, b"x" * 4090),
            lambda: cmd.keyboard_upload_chunk(0, 4096, b"x"),
            lambda: cmd.keyboard_upload_commit(0x100000000),
            lambda: cmd.keyboard_run(0, 0),
            lambda: cmd.keyboard_run(0, 4),
            lambda: cmd.keyboard_set_temporary_ble_name("é" * 14),
            lambda: cmd.keyboard_set_temporary_ble_name("bad\nname"),
            lambda: cmd.keyboard_arm_ble(0),
        ]
        for call in calls:
            with self.subTest(call=call), self.assertRaises(ValueError):
                call()
        self.assertEqual(device.calls, [])

    def test_temporary_name_and_arm_messages(self):
        device = FakeDevice([
            response(b"\x01\x06Lab KB"),
            response(struct.pack("!I", 0x10203040)),
            response(b"\x01\x00"),
        ])
        cmd = ChameleonCMD(device)

        self.assertEqual(
            cmd.keyboard_set_temporary_ble_name("Lab KB"), "Lab KB")
        self.assertEqual(cmd.keyboard_arm_ble(7)["run_id"], 0x10203040)
        self.assertEqual(cmd.keyboard_set_temporary_ble_name(), "")
        self.assertEqual(device.calls, [
            (Command.KEYBOARD_SET_TEMP_BLE_NAME, b"\x01\x06Lab KB"),
            (Command.KEYBOARD_ARM_BLE, b"\x01\x00\x00\x00\x07"),
            (Command.KEYBOARD_SET_TEMP_BLE_NAME, b"\x01\x00"),
        ])

    def test_armed_status_uses_existing_fixed_payload(self):
        payload = struct.pack(
            "!BBBBIIIHHHHI", 1, 7, 0, 2, 0, 9, 11, 0, 0, 0, 10, 1)
        status = ChameleonCMD(FakeDevice([response(payload)])).keyboard_get_status()
        self.assertEqual(status["state_name"], "armed")

    def test_malformed_success_and_error_status_are_rejected(self):
        malformed = [
            lambda: ChameleonCMD(FakeDevice([response(b"short")])).keyboard_upload_begin(1, 0),
            lambda: ChameleonCMD(FakeDevice([
                response(struct.pack("!BIHH", 2, 1, 0, 4089))
            ])).keyboard_upload_begin(1, 0),
            lambda: ChameleonCMD(FakeDevice([response(b"x")])).keyboard_run(1, 1),
            lambda: ChameleonCMD(FakeDevice([
                response(struct.pack(
                    "!BBBBIIIHHHHI", 1, 9, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0))
            ])).keyboard_get_status(),
        ]
        for call in malformed:
            with self.subTest(call=call), self.assertRaises(ValueError):
                call()
        with self.assertRaises(UnexpectedResponseError):
            ChameleonCMD(FakeDevice([
                response(status=Status.PAR_ERR)
            ])).keyboard_upload_commit(1)


class TestKeyboardUpload(unittest.TestCase):
    def test_upload_chunks_and_verifies_all_metadata(self):
        program = bytes(range(256)) * 16
        checksum = zlib.crc32(program) & 0xFFFFFFFF

        def handler(command, payload):
            if command == Command.KEYBOARD_UPLOAD_BEGIN:
                version, total, crc = struct.unpack("!BHI", payload)
                self.assertEqual((version, total, crc), (1, 4096, checksum))
                return response(struct.pack("!BIHH", 1, 7, 0, 4089))
            if command == Command.KEYBOARD_UPLOAD_CHUNK:
                version, upload_id, offset = struct.unpack("!BIH", payload[:7])
                self.assertEqual((version, upload_id), (1, 7))
                return response(struct.pack("!IH", 7, offset + len(payload[7:])))
            if command == Command.KEYBOARD_UPLOAD_COMMIT:
                self.assertEqual(payload, b"\x01\x00\x00\x00\x07")
                return response(struct.pack("!IHI", 9, 4096, checksum))
            self.fail(f"unexpected command {command}")

        device = FakeDevice(handler=handler)
        committed = ChameleonCMD(device).keyboard_upload(program)
        self.assertEqual(committed["commit_id"], 9)
        chunks = [payload for command, payload in device.calls
                  if command == Command.KEYBOARD_UPLOAD_CHUNK]
        self.assertEqual([len(payload) - 7 for payload in chunks], [4089, 7])

    def test_acknowledgement_mismatch_aborts_upload(self):
        device = FakeDevice([
            response(struct.pack("!BIHH", 1, 1, 0, 4089)),
            response(struct.pack("!IH", 1, 0)),
        ])
        with self.assertRaisesRegex(ValueError, "chunk metadata mismatch"):
            ChameleonCMD(device).keyboard_upload(b"x")
        self.assertEqual(len(device.calls), 2)


class TestKeyboardCLI(unittest.TestCase):
    def test_compile_and_upload_expose_layout_choices_with_us_default(self):
        for unit_class in (HWKeyboardCompile, HWKeyboardUpload):
            parser = unit_class().args_parser()
            layout_action = next(
                action for action in parser._actions if action.dest == "layout")
            with self.subTest(unit=unit_class.__name__):
                self.assertEqual(layout_action.default, "us")
                self.assertEqual(
                    tuple(layout_action.choices), ("us", "uk", "es", "de", "fr", "it", "pt"))
                self.assertEqual(
                    parser.parse_args(["payload.txt", "--layout", "de"]).layout, "de")

    def test_run_attempts_command_over_usb_or_authenticated_ble(self):
        class FakeCommand:
            def __init__(self):
                self.run = None

            def keyboard_get_status(self):
                return {"commit_id": 42}

            def keyboard_run(self, commit_id, outputs):
                self.run = (commit_id, outputs)
                return {"run_id": 7}

        unit = HWKeyboardRun()
        fake = FakeCommand()
        unit._device_cmd = fake
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            unit.on_exec(argparse.Namespace(output="ble"))
        self.assertEqual(fake.run, (42, 2))
        self.assertIn("command transport is USB or authenticated BLE", output.getvalue())

    def test_arm_sets_name_before_arming(self):
        class FakeCommand:
            def __init__(self):
                self.calls = []

            def keyboard_get_status(self):
                return {"commit_id": 42}

            def keyboard_set_temporary_ble_name(self, name):
                self.calls.append(("name", name))
                return name or "ChameleonUltra"

            def keyboard_arm_ble(self, commit_id):
                self.calls.append(("arm", commit_id))
                return {"run_id": 8}

        unit = HWKeyboardArm()
        fake = FakeCommand()
        unit._device_cmd = fake
        with contextlib.redirect_stdout(io.StringIO()):
            unit.on_exec(argparse.Namespace(name="Lab KB"))
        self.assertEqual(fake.calls, [("name", "Lab KB"), ("arm", 42)])


if __name__ == "__main__":
    unittest.main()
