#!/usr/bin/env python3
"""Hardware-free tests for stateful real-card ISO-DEP host commands."""

import argparse
import contextlib
import io
import os
import struct
import sys
import unittest

CURRENT_DIR = os.path.split(os.path.abspath(__file__))[0]
sys.path.append(CURRENT_DIR.rsplit(os.sep, 1)[0])

import chameleon_com  # noqa: E402
from chameleon_cli_unit import (  # noqa: E402
    HF14AReaderSessionExchange,
    HF14AReaderSessionStart,
    HF14AReaderSessionStop,
)
from chameleon_cmd import ChameleonCMD  # noqa: E402
from chameleon_enum import Command, Status  # noqa: E402
from chameleon_utils import ArgsParserError, UnexpectedResponseError  # noqa: E402


def response(data=b"", status=Status.HF_TAG_OK):
    return chameleon_com.Response(0, status, data)


class FakeDevice:
    def __init__(self, responses):
        self.responses = list(responses)
        self.calls = []
        self.closed = False

    def send_cmd_sync(self, command, data=None, timeout=3):
        payload = b"" if data is None else bytes(data)
        self.calls.append((command, payload, timeout))
        reply = self.responses.pop(0)
        reply.cmd = command
        return reply

    def close(self):
        self.closed = True


def start_payload(session_id=0x10203040, uid=b"\x01\x02\x03\x04",
                  atqa=b"\x04\x00", sak=0x20, ats=b"\x02\x08"):
    return (struct.pack("!IB", session_id, len(uid)) + uid + atqa +
            bytes((sak, len(ats))) + ats)


class TestIsoDepSessionCommands(unittest.TestCase):
    def test_exact_payloads_and_success_parsing(self):
        device = FakeDevice([
            response(start_payload()),
            response(start_payload(session_id=0x50607080)),
            response(b"\x01\x02\x90\x00"),
            response(status=Status.SUCCESS),
        ])
        cmd = ChameleonCMD(device)

        started = cmd.hf14a_4_reader_session_start()
        self.assertEqual(started, {
            "session_id": 0x10203040,
            "uid": b"\x01\x02\x03\x04",
            "atqa": b"\x04\x00",
            "sak": 0x20,
            "ats": b"\x02\x08",
        })
        transit = cmd.hf14a_4_reader_session_start_apple_transit()
        self.assertEqual(transit, {
            "session_id": 0x50607080,
            "uid": b"\x01\x02\x03\x04",
            "atqa": b"\x04\x00",
            "sak": 0x20,
            "ats": b"\x02\x08",
        })
        self.assertEqual(
            cmd.hf14a_4_reader_session_exchange(0x10203040, b"\x00\x84"),
            b"\x01\x02\x90\x00")
        self.assertTrue(cmd.hf14a_4_reader_session_stop(0x10203040))
        self.assertEqual(device.calls, [
            (Command.HF14A_4_READER_SESSION_START, b"", 6),
            (Command.HF14A_4_READER_SESSION_START_APPLE_TRANSIT, b"", 6),
            (Command.HF14A_4_READER_SESSION_EXCHANGE,
             b"\x10\x20\x30\x40\x00\x84", 10),
            (Command.HF14A_4_READER_SESSION_STOP, b"\x10\x20\x30\x40", 6),
        ])

    def test_invalid_requests_do_not_reach_transport(self):
        device = FakeDevice([])
        cmd = ChameleonCMD(device)
        invalid = (
            lambda: cmd.hf14a_4_reader_session_exchange(0, b"x"),
            lambda: cmd.hf14a_4_reader_session_exchange(True, b"x"),
            lambda: cmd.hf14a_4_reader_session_exchange(1, b""),
            lambda: cmd.hf14a_4_reader_session_exchange(1, b"x" * 513),
            lambda: cmd.hf14a_4_reader_session_exchange(1, "0084"),
            lambda: cmd.hf14a_4_reader_session_stop(0x100000000),
        )
        for call in invalid:
            with self.subTest(call=call), self.assertRaises(ValueError):
                call()
        self.assertEqual(device.calls, [])

    def test_malformed_success_responses_are_rejected(self):
        malformed_starts = (
            b"short",
            start_payload(session_id=0),
            start_payload(uid=b"12345"),
            start_payload() + b"trailing",
            start_payload(sak=0x00),
            start_payload(ats=b"\x01"),
        )
        for payload in malformed_starts:
            with self.subTest(payload=payload), self.assertRaises(ValueError):
                ChameleonCMD(FakeDevice([response(payload)])).hf14a_4_reader_session_start()
        for payload in (b"\x90", b"x" * 513):
            with self.subTest(payload_len=len(payload)), self.assertRaises(ValueError):
                ChameleonCMD(FakeDevice([response(payload)])) \
                    .hf14a_4_reader_session_exchange(1, b"\x00")
        with self.assertRaises(ValueError):
            ChameleonCMD(FakeDevice([
                response(b"unexpected", Status.SUCCESS)
            ])).hf14a_4_reader_session_stop(1)

    def test_error_status_is_not_accepted_as_session_data(self):
        with self.assertRaises(UnexpectedResponseError):
            ChameleonCMD(FakeDevice([
                response(b"\x03\x03\x00", Status.HF_ERR_CRC)
            ])).hf14a_4_reader_session_exchange(1, b"\x00")
        for method in (
                "hf14a_4_reader_session_start",
                "hf14a_4_reader_session_start_apple_transit"):
            with self.subTest(method=method), self.assertRaises(
                    UnexpectedResponseError):
                getattr(ChameleonCMD(FakeDevice([
                    response(b"\x01\x62", Status.HF_COLLISION)
                ])), method)()

    def test_stateful_timeout_closes_transport_before_retry(self):
        class TimeoutDevice(FakeDevice):
            def send_cmd_sync(self, command, data=None, timeout=3):
                self.calls.append((command, data, timeout))
                raise TimeoutError("late response possible")

        for method, args in (
                ("hf14a_4_reader_session_start", ()),
                ("hf14a_4_reader_session_start_apple_transit", ()),
                ("hf14a_4_reader_session_exchange", (1, b"\x00")),
                ("hf14a_4_reader_session_stop", (1,))):
            device = TimeoutDevice([])
            with self.subTest(method=method), self.assertRaises(TimeoutError):
                getattr(ChameleonCMD(device), method)(*args)
            self.assertTrue(device.closed)


class TestIsoDepSessionCLI(unittest.TestCase):
    def test_start_parser_selects_express_transit(self):
        parser = HF14AReaderSessionStart().args_parser()
        self.assertFalse(parser.parse_args([]).express_transit)
        self.assertTrue(parser.parse_args(["--express-transit"]).express_transit)

    def test_exchange_and_stop_parsers_validate_ids_and_apdus(self):
        exchange = HF14AReaderSessionExchange().args_parser()
        parsed = exchange.parse_args(["0x10203040", "00A40400"])
        self.assertEqual(parsed.session_id, 0x10203040)
        self.assertEqual(parsed.apdu, b"\x00\xA4\x04\x00")
        stop = HF14AReaderSessionStop().args_parser()
        self.assertEqual(stop.parse_args(["7"]).session_id, 7)
        for argv in (["0", "00"], ["1", "0"], ["1", "zz"]):
            with self.subTest(argv=argv), self.assertRaises(ArgsParserError):
                exchange.parse_args(argv)

    def test_cli_actions_call_matching_wrappers(self):
        class FakeCommand:
            def __init__(self):
                self.calls = []

            def hf14a_4_reader_session_start(self):
                self.calls.append(("start",))
                return {
                    "session_id": 9, "uid": b"\x01\x02\x03\x04",
                    "atqa": b"\x04\x00", "sak": 0x20,
                    "ats": b"\x02\x08",
                }

            def hf14a_4_reader_session_start_apple_transit(self):
                self.calls.append(("start-apple-transit",))
                return {
                    "session_id": 10, "uid": b"\x01\x02\x03\x04",
                    "atqa": b"\x04\x00", "sak": 0x20,
                    "ats": b"\x02\x08",
                }

            def hf14a_4_reader_session_exchange(self, session_id, apdu):
                self.calls.append(("exchange", session_id, apdu))
                return b"\x90\x00"

            def hf14a_4_reader_session_stop(self, session_id):
                self.calls.append(("stop", session_id))
                return True

        fake = FakeCommand()
        units_and_args = (
            (HF14AReaderSessionStart(), argparse.Namespace(express_transit=False)),
            (HF14AReaderSessionStart(), argparse.Namespace(express_transit=True)),
            (HF14AReaderSessionExchange(),
             argparse.Namespace(session_id=9, apdu=b"\x00\x84")),
            (HF14AReaderSessionStop(), argparse.Namespace(session_id=9)),
        )
        with contextlib.redirect_stdout(io.StringIO()):
            for unit, args in units_and_args:
                unit._device_cmd = fake
                unit.on_exec(args)
        self.assertEqual(fake.calls, [
            ("start",),
            ("start-apple-transit",),
            ("exchange", 9, b"\x00\x84"),
            ("stop", 9),
        ])


if __name__ == "__main__":
    unittest.main()
