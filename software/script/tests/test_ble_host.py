#!/usr/bin/env python3
"""Hardware-free tests for BLE command parsing and CLI pagination."""
import argparse
import contextlib
import io
import os
import struct
import sys
import unittest
from unittest import mock

CURRENT_DIR = os.path.split(os.path.abspath(__file__))[0]
sys.path.append(CURRENT_DIR.rsplit(os.sep, 1)[0])

import chameleon_com  # noqa: E402
from chameleon_cli_unit import BLEPing, BLEWrite, _drain_ble_pages  # noqa: E402
from chameleon_cmd import (  # noqa: E402
    ChameleonCMD,
    build_ble_apple_proximity_profile,
    build_ble_advertising_profile,
    build_ble_fast_pair_profile,
    validate_ble_advertising_data,
)
from chameleon_enum import Command, Status  # noqa: E402
from chameleon_utils import UnexpectedResponseError  # noqa: E402


class FakeDevice:
    def __init__(self, responses=None, handler=None):
        self.responses = list(responses or [])
        self.handler = handler
        self.calls = []

    def send_cmd_sync(self, command, data=None):
        payload = b'' if data is None else bytes(data)
        self.calls.append((command, payload))
        if self.handler is not None:
            return self.handler(command, payload)
        if self.responses:
            response = self.responses.pop(0)
            response.cmd = command
            return response
        return chameleon_com.Response(command, Status.SUCCESS)


def response(data=b'', status=Status.SUCCESS):
    return chameleon_com.Response(0, status, data)


class TestBLEPayloads(unittest.TestCase):
    def setUp(self):
        self.device = FakeDevice()
        self.cmd = ChameleonCMD(self.device)

    def test_multibyte_payloads_are_big_endian(self):
        self.cmd.ble_connect(b'\x01\x02\x03\x04\x05\x06', 3)
        self.assertEqual(
            self.device.calls[-1],
            (Command.BLE_CONNECT, b'\x03\x01\x02\x03\x04\x05\x06'))

        self.cmd.ble_gatt_read_start(0x1234)
        self.assertEqual(self.device.calls[-1], (Command.BLE_GATT_READ, b'\x12\x34'))

        self.cmd.ble_gatt_write_start(0x2345, b'\xaa\xbb')
        self.assertEqual(
            self.device.calls[-1], (Command.BLE_GATT_WRITE, b'\x23\x45\xaa\xbb'))

        self.cmd.ble_fuzz_start(0x3456, 0x1234, 0x0203)
        self.assertEqual(
            self.device.calls[-1],
            (Command.BLE_FUZZ_START, b'\x34\x56\x12\x34\x02\x03'))

    def test_paged_indexes_use_protocol_width_and_byte_order(self):
        self.device.responses.extend([response(), response(), response(), response()])
        self.cmd.ble_scan_get_results(0x7F)
        self.cmd.ble_gatt_get_chars(0x80)
        self.cmd.ble_get_notifications(0x1234)
        self.cmd.ble_fuzz_get_log(0xFEDC)
        self.assertEqual(self.device.calls, [
            (Command.BLE_SCAN_GET_RESULTS, b'\x7f'),
            (Command.BLE_GATT_GET_CHARS, b'\x80'),
            (Command.BLE_GET_NOTIFICATIONS, b'\x12\x34'),
            (Command.BLE_FUZZ_GET_LOG, b'\xfe\xdc'),
        ])

    def test_advertising_lab_encodes_rotating_names(self):
        status = bytes((1, 2, 2, 1, 0, 0, 2, 10, 0,
                        0, 160, 3, 232, 0, 0, 0, 0, 0, 0, 0))
        self.device.responses.append(response(status))
        parsed = self.cmd.ble_adv_lab_start(
            b'\x02\x01\x06', names=('Lab A', 'Lab B'), name_target=1,
            interval_ms=100, rotation_ms=1000, profile=2)
        expected = struct.pack('!BBBBHHHBBBB', 1, 2, 1, 1, 160, 1000,
                               0, 0, 3, 0, 2)
        expected += b'\x02\x01\x06\x05Lab A\x05Lab B'
        self.assertEqual(self.device.calls[-1],
                         (Command.BLE_ADV_LAB_START, expected))
        self.assertTrue(parsed['running'])
        self.assertEqual(parsed['rotation_ms'], 1000)

    def test_advertising_lab_status_rejects_wrong_length(self):
        self.device.responses.append(response(b'\x01' * 19))
        with self.assertRaisesRegex(ValueError, 'expected 20'):
            self.cmd.ble_adv_lab_status()

    def test_advertising_lab_validation_happens_before_transport(self):
        invalid_values = (
            dict(advertising_data=b'\x05\x09bad'),
            dict(advertising_data=b'\x02\x01\x06', names=('A',), name_target=0),
            dict(advertising_data=b'\x02\x01\x06', names=('A', 'B'),
                 name_target=1, rotation_ms=50),
            dict(advertising_data=b'', scan_response_data=b'\x02\x01\x06'),
        )
        for values in invalid_values:
            with self.subTest(values=values), self.assertRaises(ValueError):
                self.cmd.ble_adv_lab_start(**values)
        self.assertEqual(self.device.calls, [])

    def test_advertising_lab_rotation_uses_quantized_interval(self):
        with self.assertRaisesRegex(ValueError, 'rotation_ms'):
            self.cmd.ble_adv_lab_start(
                b'\x02\x01\x06', names=('A', 'B'), name_target=1,
                interval_ms=101, rotation_ms=101, profile=2)
        self.assertEqual(self.device.calls, [])

    def test_advertising_profile_is_vendor_neutral_and_little_endian(self):
        advertising, scan = build_ble_advertising_profile(
            service_uuid=0x180F, company_id=0x1234,
            manufacturer_data=b'\xaa\xbb')
        self.assertEqual(advertising, b'\x02\x01\x06\x03\x03\x0f\x18')
        self.assertEqual(scan, b'\x05\xff\x34\x12\xaa\xbb')
        self.assertFalse(validate_ble_advertising_data(advertising))
        self.assertFalse(validate_ble_advertising_data(scan, scan_response=True))

    def test_advertising_profile_encodes_generic_service_data(self):
        advertising, scan = build_ble_advertising_profile(
            service_uuid=0x181A, service_data_uuid=0x181A,
            service_data=b'\x01\x02')
        self.assertEqual(advertising, b'\x02\x01\x06\x03\x03\x1a\x18')
        self.assertEqual(scan, b'\x05\x16\x1a\x18\x01\x02')
        with self.assertRaisesRegex(ValueError, 'service_data_uuid'):
            build_ble_advertising_profile(service_data=b'\x01')

    def test_pairing_discovery_profiles_are_complete_ad_structures(self):
        apple, apple_scan = build_ble_apple_proximity_profile(0x0E20)
        self.assertEqual(len(apple), 31)
        self.assertEqual(apple[:9], b'\x1e\xff\x4c\x00\x07\x19\x07\x0e\x20')
        self.assertEqual(apple_scan, b'')

        android, android_scan = build_ble_fast_pair_profile(0x2D7A23)
        self.assertEqual(
            android,
            bytes.fromhex('02 01 06 03 03 2C FE 06 16 2C FE 2D 7A 23 02 0A EC'))
        self.assertEqual(android_scan, b'')

    def test_invalid_payload_arguments_are_rejected_before_transport(self):
        invalid_calls = [
            lambda: self.cmd.ble_connect(b'12345', 0),
            lambda: self.cmd.ble_connect(6, 0),
            lambda: self.cmd.ble_connect(b'123456', 4),
            lambda: self.cmd.ble_gatt_read_start(0),
            lambda: self.cmd.ble_gatt_write_start(1, b''),
            lambda: self.cmd.ble_gatt_write_start(1, 3),
            lambda: self.cmd.ble_gatt_write_start(1, b'x' * 245),
            lambda: self.cmd.ble_subscribe(0, 1),
            lambda: self.cmd.ble_subscribe(1, 3),
            lambda: self.cmd.ble_scan_get_results(256),
            lambda: self.cmd.ble_get_notifications(65536),
        ]
        for call in invalid_calls:
            with self.subTest(call=call), self.assertRaises(ValueError):
                call()
        self.assertEqual(self.device.calls, [])


class TestBLEParsing(unittest.TestCase):
    def command_with(self, data, status=Status.SUCCESS):
        return ChameleonCMD(FakeDevice([response(data, status)]))

    def test_variable_and_fixed_records_parse_exactly(self):
        scan_data = (b'\x01\x02\x03\x04\x05\x06' +
                     struct.pack('!BbB', 2, -42, 3) + b'adv')
        self.assertEqual(self.command_with(scan_data).ble_scan_get_results(), [{
            'addr': b'\x01\x02\x03\x04\x05\x06',
            'addr_type': 2,
            'rssi': -42,
            'adv': b'adv',
        }])

        char_data = struct.pack('!HBBH', 0x1234, 0x1A, 1, 0x2A00)
        self.assertEqual(self.command_with(char_data).ble_gatt_get_chars(), [{
            'handle': 0x1234, 'props': 0x1A, 'uuid_type': 1, 'uuid': 0x2A00,
        }])

        notif_data = struct.pack('!HB', 0x2233, 3) + b'xyz'
        self.assertEqual(self.command_with(notif_data).ble_get_notifications(), [{
            'handle': 0x2233, 'data': b'xyz',
        }])

        fuzz_data = struct.pack('!HBB', 0x3456, 20, 7) + bytes(range(16))
        self.assertEqual(self.command_with(fuzz_data).ble_fuzz_get_log(), [{
            'index': 0x3456, 'len': 20, 'status': 7, 'data': bytes(range(16)),
        }])

    def test_scalar_and_state_responses_do_not_fall_back(self):
        self.assertEqual(self.command_with(b'\x05').ble_scan_get_count(), 5)
        self.assertEqual(self.command_with(b'\x00\xf7').ble_get_mtu(), 247)
        state = self.command_with(bytes([2, 2, 4, 0, 0x12, 0x34,
                                         1, 0, 2, 0, 3, 4])).ble_central_state()
        self.assertEqual(state['fuzz_sent'], 0x1234)
        self.assertEqual((state['probe_index'], state['probe_total']), (3, 4))

        extended = self.command_with(bytes([
            2, 2, 4, 0, 0x12, 0x34, 1, 0, 2, 0, 3, 4,
            1, 0, 0, 0, 9, 2, 3, 0, 5,
        ])).ble_central_state()
        self.assertEqual(extended['flood_state'], 1)
        self.assertEqual(extended['flood_sent'], 9)
        self.assertEqual(extended['read_state'], 2)
        self.assertEqual(extended['write_state'], 3)
        self.assertEqual(extended['notification_count'], 5)

        with self.assertRaises(ValueError):
            self.command_with(b'').ble_scan_get_count()
        with self.assertRaises(ValueError):
            self.command_with(b'\x00').ble_get_mtu()
        with self.assertRaises(ValueError):
            self.command_with(b'\x00' * 11).ble_central_state()
        with self.assertRaises(UnexpectedResponseError):
            self.command_with(b'', Status.PAR_ERR).ble_scan_get_count()

    def test_malformed_record_lengths_are_rejected(self):
        malformed = [
            lambda: self.command_with(b'\x00' * 8).ble_scan_get_results(),
            lambda: self.command_with(b'\x00' * 9 + b'\x01').ble_scan_get_results(),
            lambda: self.command_with(b'\x00' * 5).ble_gatt_get_chars(),
            lambda: self.command_with(b'\x02\x00').ble_get_descs(),
            lambda: self.command_with(b'\x02\x00').ble_get_svcs(),
            lambda: self.command_with(b'\x00\x01').ble_gatt_read_result(),
            lambda: self.command_with(b'\x02\x00\x02x').ble_gatt_read_result(),
            lambda: self.command_with(b'\x00\x01').ble_get_cccd(),
            lambda: self.command_with(b'\x12\x34\x02x').ble_get_notifications(),
            lambda: self.command_with(b'\x00\x01\x10\x00short').ble_fuzz_get_log(),
            lambda: self.command_with(b'\x02\x01\x2a\x00\x00\x02x').ble_get_devinfo(),
        ]
        for parse in malformed:
            with self.subTest(parse=parse), self.assertRaises(ValueError):
                parse()


class TestBLEPagination(unittest.TestCase):
    @staticmethod
    def scan_record(index):
        adv = bytes([index]) * (index % 3)
        return (bytes([index]) * 6 + struct.pack('!BbB', index % 4, -30 - index, len(adv)) + adv)

    def test_variable_records_are_drained_using_record_indexes(self):
        records = [self.scan_record(i) for i in range(5)]

        def handler(command, payload):
            self.assertEqual(command, Command.BLE_SCAN_GET_RESULTS)
            start = payload[0]
            return response(b''.join(records[start:start + 2]))

        device = FakeDevice(handler=handler)
        cmd = ChameleonCMD(device)
        result = _drain_ble_pages(
            cmd.ble_scan_get_results, expected_count=5, index_limit=0xFF)
        self.assertEqual([item['adv'] for item in result],
                         [b'', b'\x01', b'\x02\x02', b'', b'\x04'])
        self.assertEqual([payload for _, payload in device.calls],
                         [b'\x00', b'\x02', b'\x04'])

    def test_fixed_and_stateful_pages_advance_and_terminate(self):
        calls = []

        def fixed_page(start):
            calls.append(start)
            return {0: [0, 1], 2: [2, 3], 4: []}[start]

        self.assertEqual(_drain_ble_pages(fixed_page), [0, 1, 2, 3])
        self.assertEqual(calls, [0, 2, 4])

        stateful_calls = []

        def stateful_page(start):
            stateful_calls.append(start)
            return {'state': 2, 'items': {0: ['a'], 1: ['b'], 2: []}[start]}

        self.assertEqual(_drain_ble_pages(stateful_page, stateful=True),
                         {'state': 2, 'items': ['a', 'b']})
        self.assertEqual(stateful_calls, [0, 1, 2])

    def test_incomplete_and_unbounded_paging_fail(self):
        with self.assertRaises(RuntimeError):
            _drain_ble_pages(lambda start: [], expected_count=1)
        with self.assertRaises(RuntimeError):
            _drain_ble_pages(lambda start: [start], max_pages=3)


class TestBLECLI(unittest.TestCase):
    def test_ping_parser_supports_current_link_and_typed_address(self):
        parser = BLEPing().args_parser()
        current = parser.parse_args([])
        self.assertFalse(current.all)
        self.assertIsNone(current.addr)
        addressed = parser.parse_args(['--addr', 'AA:BB:CC:DD:EE:FF', '--type', '3'])
        self.assertEqual((addressed.addr, addressed.type), ('AA:BB:CC:DD:EE:FF', 3))

    def test_address_ping_passes_type_and_little_endian_address(self):
        class FakePingCommand:
            def __init__(self):
                self.connected = None
                self.probed = False
                self.disconnected = False
                self.states = iter([
                    {'conn_state': 2},
                    {'probe_state': 2},
                ])

            def ble_connect(self, addr, addr_type):
                self.connected = (addr, addr_type)
                return response()

            def ble_central_state(self):
                return next(self.states)

            def ble_link_probe(self):
                self.probed = True

            def ble_disconnect(self):
                self.disconnected = True

        unit = BLEPing()
        fake = FakePingCommand()
        unit._device_cmd = fake
        args = argparse.Namespace(addr='AA:BB:CC:DD:EE:FF', all=False, type=3)
        with mock.patch('chameleon_cli_unit.time.sleep'), contextlib.redirect_stdout(io.StringIO()):
            unit.on_exec(args)
        self.assertEqual(fake.connected, (b'\xff\xee\xdd\xcc\xbb\xaa', 3))
        self.assertTrue(fake.probed)
        self.assertTrue(fake.disconnected)

    def test_ping_without_selector_uses_current_link(self):
        class FakePingCommand:
            def __init__(self):
                self.probed = False
                self.states = iter([
                    {'conn_state': 2},
                    {'probe_state': 2},
                ])

            def ble_central_state(self):
                return next(self.states)

            def ble_link_probe(self):
                self.probed = True

        unit = BLEPing()
        fake = FakePingCommand()
        unit._device_cmd = fake
        args = argparse.Namespace(addr=None, all=False, type=0)
        with mock.patch('chameleon_cli_unit.time.sleep'), contextlib.redirect_stdout(io.StringIO()):
            unit.on_exec(args)
        self.assertTrue(fake.probed)

    def test_write_over_current_mtu_is_not_dispatched(self):
        class FakeWriteCommand:
            def __init__(self):
                self.writes = []

            def ble_get_mtu(self):
                return 23

            def ble_gatt_write_start(self, handle, data):
                self.writes.append((handle, data))

        unit = BLEWrite()
        fake = FakeWriteCommand()
        unit._device_cmd = fake
        args = argparse.Namespace(handle=1, data='00' * 21)
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            unit.on_exec(args)
        self.assertEqual(fake.writes, [])
        self.assertIn('allows at most 20 bytes', output.getvalue())


if __name__ == '__main__':
    unittest.main()
