#!/usr/bin/env python3
"""Hardware-free tests for the retained EMV trace host protocol."""
import os
import struct
import sys
import unittest
import zlib

CURRENT_DIR = os.path.split(os.path.abspath(__file__))[0]
sys.path.append(CURRENT_DIR.rsplit(os.sep, 1)[0])

from emv_trace import (  # noqa: E402
    ApduPayload,
    AppPayload,
    BEHAVIOR_ADAPTIVE_PROFILES,
    BEHAVIOR_DIRECT_AID_FALLBACK,
    BEHAVIOR_REACQUIRE_PROFILES,
    EmvTraceError,
    EmvTraceRequest,
    PROFILE_CUSTOM,
    PROFILE_SWEEP,
    POLLING_PATIENT,
    RfPayload,
    SummaryPayload,
    download_emv_trace,
    encode_get_request,
    encode_meta_request,
    encode_start_request,
    parse_get_response,
    parse_meta_response,
    parse_record,
    parse_start_response,
    trace_to_json,
)
import chameleon_com  # noqa: E402
from chameleon_cmd import ChameleonCMD  # noqa: E402
from chameleon_enum import Command, Status  # noqa: E402


def make_record(record_type, sequence, payload, stage=None, app_index=0,
                attempt=0, flags=0, status=0, timestamp=10):
    if stage is None:
        stage = 0xFF if record_type == 4 else 1
    body = (struct.pack("!BBIBBBBHI", 1, record_type, sequence, stage,
                        app_index, attempt, flags, status, timestamp) + payload)
    return struct.pack("!H", len(body)) + body


def make_page(scan_id, start, records, more):
    raw_records = b"".join(records)
    count = len(records)
    return (struct.pack("!BBIIIHH", 1, 0x01 if more else 0x02, scan_id,
                        start, start + count, count, len(raw_records)) + raw_records)


def make_meta(scan_id, records, observed=None, crc=None):
    raw_records = b"".join(records)
    observed = len(records) if observed is None else observed
    crc = zlib.crc32(raw_records) & 0xFFFFFFFF if crc is None else crc
    fixed = struct.pack(
        "!BBHIIIIIIIIHI", 1, 2, 0, 1, scan_id, len(records), observed,
        len(raw_records), len(raw_records), 0xFFFFFFFF, crc, 1, 123)
    return fixed + b"\x04\x01\x02\x03\x04\x04\x00\x20\x02\x75\x77"


class TestEmvTraceEncoding(unittest.TestCase):
    def test_start_request_is_exact_and_big_endian(self):
        request = EmvTraceRequest(
            flags=0x3F,
            max_aids=16,
            max_records=64,
            max_apdus=0x0123,
            budget_ms=0x1234,
            amount=bytes.fromhex("000000000100"),
            country=bytes.fromhex("0840"),
            currency=bytes.fromhex("0978"),
            date=bytes.fromhex("260710"),
            transaction_type=0x09,
            cryptogram_type=0x80,
        )
        encoded = encode_start_request(request)
        self.assertEqual(len(encoded), 25)
        self.assertEqual(
            encoded,
            bytes.fromhex(
                "013f1040012300001234000000000100084009782607100980"))
        self.assertEqual(encode_meta_request(0x01020304),
                         bytes.fromhex("0101020304"))
        self.assertEqual(encode_get_request(0x01020304, 0x05060708, 0x1234),
                          bytes.fromhex("0101020304050607081234"))

    def test_express_transit_option_uses_reserved_v1_bit(self):
        encoded = encode_start_request(EmvTraceRequest(flags=0x40))
        self.assertEqual(len(encoded), 25)
        self.assertEqual(encoded[:2], bytes.fromhex("0140"))

    def test_extended_terminal_profiles_are_explicit(self):
        sweep = encode_start_request(EmvTraceRequest(
            flags=0x40, terminal_profile=PROFILE_SWEEP))
        self.assertEqual(len(sweep), 30)
        self.assertEqual(sweep[:2], bytes.fromhex("01c0"))
        self.assertEqual(sweep[25:], bytes.fromhex("ff00000000"))

        custom = encode_start_request(EmvTraceRequest(
            terminal_profile=PROFILE_CUSTOM,
            custom_ttq=bytes.fromhex("12345678")))
        self.assertEqual(custom[25:], bytes.fromhex("fe12345678"))

        adaptive = encode_start_request(EmvTraceRequest(
            terminal_profile=PROFILE_SWEEP,
            polling_profile=POLLING_PATIENT,
            behavior=BEHAVIOR_DIRECT_AID_FALLBACK |
                     BEHAVIOR_ADAPTIVE_PROFILES |
                     BEHAVIOR_REACQUIRE_PROFILES))
        self.assertEqual(len(adaptive), 35)
        self.assertEqual(adaptive[25:], bytes.fromhex("ff000000000307000000"))

    def test_invalid_request_fields_are_rejected(self):
        invalid = [
            EmvTraceRequest(flags=0x80),
            EmvTraceRequest(max_aids=17),
            EmvTraceRequest(max_records=65),
            EmvTraceRequest(max_apdus=513),
            EmvTraceRequest(budget_ms=30001),
            EmvTraceRequest(amount=b"short"),
            EmvTraceRequest(cryptogram_type=1),
            EmvTraceRequest(terminal_profile=7),
            EmvTraceRequest(terminal_profile=PROFILE_CUSTOM, custom_ttq=b"bad"),
            EmvTraceRequest(behavior=0x80),
            EmvTraceRequest(polling_profile=4),
        ]
        for request in invalid:
            with self.subTest(request=request), self.assertRaises(EmvTraceError):
                encode_start_request(request)
        with self.assertRaises(EmvTraceError):
            encode_get_request(1, 0, 17)


class TestEmvTraceParsing(unittest.TestCase):
    def test_start_meta_and_every_record_type(self):
        start = parse_start_response(struct.pack("!BBII", 1, 2, 7, 0x40))
        self.assertEqual((start.scan_id, start.flags), (7, 0x40))

        rf_raw = make_record(1, 0, struct.pack("!BHH", 1, 9, 2) + b"\xaa\x80")
        apdu_raw = make_record(
            2, 1, struct.pack("!HHH", 0x9000, 5, 4) + b"\x00\xa4\x04\x00\x00" +
            b"\x01\x02\x90\x00")
        app_raw = make_record(3, 2, b"\x05\xa0\x00\x00\x00\x03\x02")
        summary_raw = make_record(4, 3, struct.pack("!III", 3, 4, 5))

        rf = parse_record(rf_raw)
        apdu = parse_record(apdu_raw)
        app = parse_record(app_raw)
        summary = parse_record(summary_raw)
        self.assertEqual(rf.payload, RfPayload(1, 9, b"\xaa\x80"))
        self.assertIsInstance(apdu.payload, ApduPayload)
        self.assertEqual(apdu.payload.command, b"\x00\xa4\x04\x00\x00")
        self.assertEqual(apdu.payload.response, b"\x01\x02\x90\x00")
        self.assertEqual(app.payload, AppPayload(b"\xa0\x00\x00\x00\x03", 2))
        self.assertEqual(summary.payload, SummaryPayload(3, 4, 5))

        records = [rf_raw, apdu_raw, app_raw, summary_raw]
        meta_raw = make_meta(7, records)
        meta = parse_meta_response(meta_raw, 7)
        self.assertEqual(meta.uid, b"\x01\x02\x03\x04")
        self.assertEqual(meta.atqa, b"\x04\x00")
        self.assertEqual(meta.ats, b"\x75\x77")
        self.assertEqual(meta.raw, meta_raw)

    def test_malformed_pages_and_records_are_rejected(self):
        record = make_record(4, 0, struct.pack("!III", 0, 0, 0))
        valid = make_page(9, 0, [record], more=False)
        malformed = [
            valid[:17],
            valid[:-1],
            valid[:14] + b"\x00\x02" + valid[16:],
            valid[:16] + b"\x00\x00" + valid[18:],
            bytes([1, 3]) + valid[2:],
        ]
        for page in malformed:
            with self.subTest(page=page), self.assertRaises(EmvTraceError):
                parse_get_response(page, 9, 0)
        with self.assertRaises(EmvTraceError):
            parse_get_response(valid, 10, 0)
        with self.assertRaises(EmvTraceError):
            parse_get_response(valid, 9, 1)
        with self.assertRaises(EmvTraceError):
            parse_record(record[:-1])


class TestEmvTraceDownload(unittest.TestCase):
    def setUp(self):
        self.records = [
            make_record(3, 0, b"\x05\xa0\x00\x00\x00\x03\x01"),
            make_record(2, 1, struct.pack("!HHH", 0x9000, 2, 2) +
                        b"\x00\xa4\x90\x00"),
            make_record(4, 2, struct.pack("!III", 2, 2, 1)),
        ]
        self.scan_id = 0x10203040

    def test_multi_page_assembly_preserves_raw_data(self):
        meta_raw = make_meta(self.scan_id, self.records)
        calls = []

        def fetch_meta(scan_id):
            self.assertEqual(scan_id, self.scan_id)
            return meta_raw

        def fetch_page(scan_id, start, max_payload):
            calls.append((scan_id, start, max_payload))
            if start == 0:
                return make_page(scan_id, 0, self.records[:2], more=True)
            return make_page(scan_id, 2, self.records[2:], more=False)

        download = download_emv_trace(
            self.scan_id, fetch_meta, fetch_page, max_payload=1000)
        self.assertEqual(calls, [(self.scan_id, 0, 1000),
                                 (self.scan_id, 2, 1000)])
        self.assertEqual(download.raw_records, b"".join(self.records))
        self.assertEqual([record.sequence for record in download.records], [0, 1, 2])
        self.assertEqual(len(download.pages), 2)
        exported = trace_to_json(download)
        self.assertEqual(exported["raw_records"], b"".join(self.records).hex())
        self.assertEqual(exported["pages"][0]["raw"],
                         make_page(self.scan_id, 0, self.records[:2], True).hex())

    def test_crc_mismatch_is_rejected(self):
        with self.assertRaisesRegex(EmvTraceError, "CRC32"):
            download_emv_trace(
                self.scan_id,
                lambda _: make_meta(self.scan_id, self.records, crc=0),
                lambda scan, start, maximum: make_page(
                    scan, start, self.records[start:], more=False),
            )

    def test_no_progress_and_bad_cursor_are_rejected(self):
        meta_raw = make_meta(self.scan_id, self.records)
        no_progress = struct.pack(
            "!BBIIIHH", 1, 1, self.scan_id, 0, 0, 0, 0)
        with self.assertRaisesRegex(EmvTraceError, "no progress"):
            download_emv_trace(
                self.scan_id, lambda _: meta_raw,
                lambda scan, start, maximum: no_progress)

        wrong_cursor = make_page(self.scan_id, 1, self.records[:1], more=True)
        with self.assertRaisesRegex(EmvTraceError, "cursor mismatch"):
            download_emv_trace(
                self.scan_id, lambda _: meta_raw,
                lambda scan, start, maximum: wrong_cursor)


class TestEmvTraceCommands(unittest.TestCase):
    def test_high_level_command_uses_all_three_wire_requests(self):
        scan_id = 0x01020304
        records = [make_record(4, 0, struct.pack("!III", 0, 0, 1))]
        meta_raw = make_meta(scan_id, records)
        page_raw = make_page(scan_id, 0, records, more=False)

        class FakeDevice:
            def __init__(self):
                self.calls = []

            def send_cmd_sync(inner_self, command, data=None, status=0, timeout=3):
                inner_self.calls.append((command, bytes(data or b""), timeout))
                if command == Command.HF14A_4_EMV_TRACE_START:
                    return chameleon_com.Response(
                        command, Status.HF_TAG_OK,
                        struct.pack("!BBII", 1, 2, scan_id, 1))
                if command == Command.HF14A_4_EMV_TRACE_META:
                    return chameleon_com.Response(command, Status.SUCCESS, meta_raw)
                return chameleon_com.Response(command, Status.SUCCESS, page_raw)

        device = FakeDevice()
        request = EmvTraceRequest(flags=0)
        download = ChameleonCMD(device).hf14a_4_emv_trace_download(
            request, max_payload=1000)
        self.assertEqual(download.raw_records, b"".join(records))
        self.assertEqual([call[0] for call in device.calls], [
            Command.HF14A_4_EMV_TRACE_START,
            Command.HF14A_4_EMV_TRACE_META,
            Command.HF14A_4_EMV_TRACE_GET,
        ])
        self.assertEqual(device.calls[0][1], encode_start_request(request))
        self.assertEqual(device.calls[1][1], encode_meta_request(scan_id))
        self.assertEqual(device.calls[2][1], encode_get_request(scan_id, 0, 1000))


if __name__ == "__main__":
    unittest.main()
