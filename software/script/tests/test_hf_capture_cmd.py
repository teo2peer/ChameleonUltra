import unittest

from chameleon_cmd import ChameleonCMD
from chameleon_enum import Command


class _FakeDevice:
    def __init__(self):
        self.calls = []
        self.response = object()

    def send_cmd_sync(self, command, payload=None, **kwargs):
        self.calls.append((command, payload, kwargs))
        return self.response


class HfCaptureCommandTest(unittest.TestCase):
    def setUp(self):
        self.device = _FakeDevice()
        self.commands = ChameleonCMD(self.device)

    def test_versioned_payloads_are_big_endian(self):
        session_id = 0x10203040

        self.assertIs(
            self.commands.hf_capture_start(1, start_token=0x12345678),
            self.device.response,
        )
        self.commands.hf_capture_status(session_id, 0x12345678)
        self.commands.hf_capture_get(
            session_id,
            ack_sequence=7,
            ack_delivery_token=0x0102030405060708,
            requested_bytes=1024,
        )
        self.commands.hf_capture_stop(session_id)

        self.assertEqual(self.device.calls, [
            (Command.HF_CAPTURE_START, b"\x02\x01\x12\x34\x56\x78", {}),
            (Command.HF_CAPTURE_STATUS, b"\x02\x10\x20\x30\x40\x12\x34\x56\x78", {}),
            (
                Command.HF_CAPTURE_GET,
                b"\x02\x10\x20\x30\x40\x01\x00\x00\x00\x07"
                b"\x01\x02\x03\x04\x05\x06\x07\x08\x04\x00",
                {},
            ),
            (Command.HF_CAPTURE_STOP, b"\x02\x10\x20\x30\x40", {}),
        ])

    def test_get_uses_explicit_ack_presence_and_clamps_page_size(self):
        self.commands.hf_capture_get(1, requested_bytes=1)
        self.commands.hf_capture_get(1, requested_bytes=9999)

        self.assertEqual(
            self.device.calls[0][1],
            b"\x02\x00\x00\x00\x01\x00\x00\x00\x00\x00"
            b"\x00\x00\x00\x00\x00\x00\x00\x00\x02\x5d",
        )
        self.assertEqual(self.device.calls[1][1][-2:], b"\x10\x00")

    def test_rejects_invalid_modes_and_session_ids(self):
        with self.assertRaises(ValueError):
            self.commands.hf_capture_start(0)
        with self.assertRaises(ValueError):
            self.commands.hf_capture_start(3)
        for start_token in (0, 0x100000000):
            with self.assertRaises(ValueError):
                self.commands.hf_capture_start(0, start_token=start_token)
        self.commands.hf_capture_status(0, 1)
        with self.assertRaises(ValueError):
            self.commands.hf_capture_get(1, ack_sequence=0)
        with self.assertRaises(ValueError):
            self.commands.hf_capture_get(1, ack_delivery_token=1)
        with self.assertRaises(ValueError):
            self.commands.hf_capture_get(
                1,
                ack_sequence=0,
                ack_delivery_token=0x8000000000000000,
            )
        for start_token in (0, 0x100000000):
            with self.assertRaises(ValueError):
                self.commands.hf_capture_status(0, start_token)
        for session_id in (-1, 0x100000000):
            with self.assertRaises(ValueError):
                self.commands.hf_capture_status(session_id, 1)
            with self.assertRaises(ValueError):
                self.commands.hf_capture_stop(session_id)


if __name__ == "__main__":
    unittest.main()
