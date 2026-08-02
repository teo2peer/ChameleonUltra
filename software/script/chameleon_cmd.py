import struct
import ctypes
import zlib
from typing import Optional, Union

import chameleon_com
from emv_trace import (
    EmvTraceRequest,
    download_emv_trace,
    encode_get_request,
    encode_meta_request,
    encode_start_request,
    parse_get_response,
    parse_meta_response,
    parse_start_response,
)
from chameleon_utils import expect_response, reconstruct_full_nt, parity_to_str, UnexpectedResponseError
from chameleon_enum import Command, SlotNumber, Status, TagSenseType, TagSpecificType
from chameleon_enum import ButtonPressFunction, ButtonType, MifareClassicDarksideStatus
from chameleon_enum import MfcKeyType, MfcValueBlockOperator

CURRENT_VERSION_SETTINGS = 6
ACTIVE_SLOT_SNAPSHOT_VERSION = 2
ACTIVE_SLOT_SNAPSHOT_BEGIN = 0
ACTIVE_SLOT_SNAPSHOT_SAVE_RELEASE = 1
ACTIVE_SLOT_SNAPSHOT_ABORT = 2
ACTIVE_SLOT_SNAPSHOT_SAVE_TIMEOUT_SECONDS = 55

new_key = b'\x20\x20\x66\x66'
old_keys = [b'\x51\x24\x36\x48', b'\x19\x92\x04\x27']


def _require_snapshot_revision(revision):
    if (isinstance(revision, bool) or not isinstance(revision, int) or
            not 1 <= revision <= 0xFFFFFFFF):
        raise ValueError("snapshot revision must be a nonzero 32-bit integer")


def _parse_snapshot_end(resp, operation, revision):
    if len(resp.data) != 6:
        raise UnexpectedResponseError(
            f"malformed active-slot snapshot response: expected 6 bytes, got {len(resp.data)}")
    version, response_operation, response_revision = struct.unpack("!BBI", resp.data)
    if (version != ACTIVE_SLOT_SNAPSHOT_VERSION or
            response_operation != operation or response_revision != revision):
        raise UnexpectedResponseError(
            "malformed active-slot snapshot response: transaction identity mismatch")
    return response_revision


def _close_snapshot_device(device):
    close = getattr(device, "close", None)
    if callable(close):
        try:
            close()
        except Exception:
            pass


def _abort_snapshot_revision(device, revision):
    payload = struct.pack(
        "!BBI", ACTIVE_SLOT_SNAPSHOT_VERSION,
        ACTIVE_SLOT_SNAPSHOT_ABORT, revision)
    resp = device.send_cmd_sync(
        Command.ACTIVE_SLOT_SNAPSHOT, payload, timeout=5)
    if resp.status != Status.SUCCESS:
        raise UnexpectedResponseError(
            f"active-slot snapshot ABORT failed with status {resp.status}")
    _parse_snapshot_end(resp, ACTIVE_SLOT_SNAPSHOT_ABORT, revision)


def _require_ble_length(resp, expected, label):
    """Reject successful BLE replies that do not match their wire format."""
    expected_lengths = (expected,) if isinstance(expected, int) else tuple(expected)
    if len(resp.data) not in expected_lengths:
        wanted = " or ".join(str(length) for length in expected_lengths)
        raise ValueError(
            f"malformed {label} response: expected {wanted} byte(s), got {len(resp.data)}")


def _require_iso_dep_session_id(session_id):
    if (isinstance(session_id, bool) or not isinstance(session_id, int) or
            not 1 <= session_id <= 0xFFFFFFFF):
        raise ValueError("session_id must be a nonzero 32-bit integer")


def _require_iso_dep_apdu(apdu):
    if not isinstance(apdu, (bytes, bytearray, memoryview)):
        raise ValueError("APDU must be bytes")
    apdu = bytes(apdu)
    if not 1 <= len(apdu) <= 512:
        raise ValueError("APDU must contain 1..512 bytes")
    return apdu


def _parse_iso_dep_session_start(resp):
    data = bytes(resp.data)
    if len(data) < 9:
        raise ValueError("malformed ISO-DEP session START response: too short")
    session_id, uid_len = struct.unpack_from("!IB", data)
    if session_id == 0:
        raise ValueError("malformed ISO-DEP session START response: zero session ID")
    if uid_len not in (4, 7, 10):
        raise ValueError(
            f"malformed ISO-DEP session START response: invalid UID length {uid_len}")
    fixed_end = 5 + uid_len + 2 + 1 + 1
    if len(data) < fixed_end:
        raise ValueError("malformed ISO-DEP session START response: truncated metadata")
    uid = data[5:5 + uid_len]
    atqa = data[5 + uid_len:7 + uid_len]
    sak = data[7 + uid_len]
    ats_len = data[8 + uid_len]
    if ats_len < 2 or len(data) != fixed_end + ats_len:
        raise ValueError("malformed ISO-DEP session START response: invalid ATS length")
    if not sak & 0x20:
        raise ValueError("malformed ISO-DEP session START response: target is not ISO-DEP")
    return {
        "session_id": session_id,
        "uid": uid,
        "atqa": atqa,
        "sak": sak,
        "ats": data[fixed_end:],
    }


def validate_ble_advertising_data(data, *, scan_response=False):
    """Validate complete legacy BLE AD structures, not arbitrary RF frames."""
    if not isinstance(data, (bytes, bytearray, memoryview)):
        raise ValueError("advertising data must be bytes")
    data = bytes(data)
    if len(data) > 31:
        raise ValueError("legacy advertising data must be at most 31 bytes")
    offset = 0
    has_name = False
    while offset < len(data):
        field_length = data[offset]
        if field_length == 0 or offset + 1 + field_length > len(data):
            raise ValueError("advertising data contains a truncated AD structure")
        field_type = data[offset + 1]
        if scan_response and field_type == 0x01:
            raise ValueError("flags are not valid in scan-response data")
        if field_type in (0x08, 0x09):
            if has_name:
                raise ValueError("advertising data contains multiple local names")
            has_name = True
        offset += field_length + 1
    return has_name


def build_ble_advertising_profile(*, flags=0x06, service_uuid=None,
                                  service_data_uuid=None, service_data=b'',
                                  company_id=None, manufacturer_data=b''):
    """Build a neutral legacy profile; identifiers are always operator supplied."""
    if not 0 <= flags <= 0xFF:
        raise ValueError("flags must be 0..255")
    advertising = bytearray((2, 0x01, flags))
    scan_response = bytearray()
    if service_uuid is not None:
        if not 0 <= service_uuid <= 0xFFFF:
            raise ValueError("service_uuid must be 0..65535")
        advertising.extend((3, 0x03, service_uuid & 0xFF,
                            (service_uuid >> 8) & 0xFF))
    service_data = bytes(service_data)
    if service_data_uuid is None:
        if service_data:
            raise ValueError("service_data_uuid is required with service_data")
    else:
        if not 0 <= service_data_uuid <= 0xFFFF:
            raise ValueError("service_data_uuid must be 0..65535")
        if len(service_data) > 27:
            raise ValueError("service_data must be at most 27 bytes")
        value = bytes((service_data_uuid & 0xFF,
                       (service_data_uuid >> 8) & 0xFF)) + service_data
        scan_response.extend((len(value) + 1, 0x16))
        scan_response.extend(value)
    manufacturer_data = bytes(manufacturer_data)
    if company_id is None:
        if manufacturer_data:
            raise ValueError("company_id is required with manufacturer_data")
    else:
        if not 0 <= company_id <= 0xFFFF:
            raise ValueError("company_id must be 0..65535")
        if len(manufacturer_data) > 27:
            raise ValueError("manufacturer_data must be at most 27 bytes")
        value = bytes((company_id & 0xFF, (company_id >> 8) & 0xFF)) + manufacturer_data
        scan_response.extend((len(value) + 1, 0xFF))
        scan_response.extend(value)
    validate_ble_advertising_data(advertising)
    validate_ble_advertising_data(scan_response, scan_response=True)
    return bytes(advertising), bytes(scan_response)


def build_ble_apple_proximity_profile(model_code=0x0E20):
    """Build a reverse-engineered Apple Continuity proximity-pairing record."""
    if not 0 <= model_code <= 0xFFFF:
        raise ValueError("model_code must be 0..65535")
    payload = bytes((
        0x1E, 0xFF, 0x4C, 0x00, 0x07, 0x19, 0x07,
        (model_code >> 8) & 0xFF, model_code & 0xFF,
        0x75, 0xAA, 0x30, 0x01, 0x00, 0x00,
        0x45, 0x12, 0x12, 0x12,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    ))
    validate_ble_advertising_data(payload)
    return payload, b''


def build_ble_fast_pair_profile(model_id=0x2D7A23, tx_power=-20):
    """Build a discoverable Google Fast Pair model-ID advertisement record."""
    if not 0 <= model_id <= 0xFFFFFF:
        raise ValueError("model_id must be 0..16777215")
    if not -127 <= tx_power <= 20:
        raise ValueError("tx_power must be -127..20 dBm")
    payload = bytes((
        0x02, 0x01, 0x06,
        0x03, 0x03, 0x2C, 0xFE,
        0x06, 0x16, 0x2C, 0xFE,
        (model_id >> 16) & 0xFF, (model_id >> 8) & 0xFF, model_id & 0xFF,
        0x02, 0x0A, tx_power & 0xFF,
    ))
    validate_ble_advertising_data(payload)
    return payload, b''


def _parse_ble_adv_lab_status(resp):
    _require_ble_length(resp, 20, "BLE advertising lab status")
    data = resp.data
    if data[0] != 1:
        raise ValueError(f"unsupported BLE advertising lab status version {data[0]}")
    if (data[1] not in (0, 2, 3, 4) or data[2] > 3 or data[3] > 2 or
            data[4] not in (0, 1, 2, 3, 4, 7) or data[6] > 32 or
            data[7] > 31 or data[8] > 31 or
            (data[6] == 0 and data[5] != 0xFF) or
            (data[6] != 0 and (data[5] == 0xFF or data[5] >= data[6]))):
        raise ValueError("malformed BLE advertising lab status values")
    return {
        'state': data[1],
        'running': data[1] == 2,
        'connected': data[1] == 3,
        'profile': data[2],
        'mode': data[3],
        'reason': data[4],
        'active_name_index': None if data[5] == 0xFF else data[5],
        'name_count': data[6],
        'advertising_length': data[7],
        'scan_response_length': data[8],
        'interval_units': struct.unpack('!H', data[9:11])[0],
        'rotation_ms': struct.unpack('!H', data[11:13])[0],
        'duration_units': struct.unpack('!H', data[13:15])[0],
        'max_advertising_events': data[15],
        'rotation_count': struct.unpack('!I', data[16:20])[0],
    }


def _require_ble_uint(name, value, maximum, minimum=0):
    if not isinstance(value, int) or not minimum <= value <= maximum:
        raise ValueError(f"{name} must be {minimum}..{maximum}")


KEYBOARD_PROTOCOL_VERSION = 1
KEYBOARD_MAX_PROGRAM_BYTES = 4096
KEYBOARD_MAX_CHUNK_BYTES = 4089
KEYBOARD_OUTPUT_USB = 0x01
KEYBOARD_OUTPUT_BLE = 0x02
KEYBOARD_OUTPUT_BOTH = KEYBOARD_OUTPUT_USB | KEYBOARD_OUTPUT_BLE


def _require_keyboard_uint(name, value, maximum, minimum=0):
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError(f"{name} must be {minimum}..{maximum}")


def _parse_keyboard_response(resp, fmt, label, fields, versioned=False):
    expected = struct.calcsize(fmt)
    if len(resp.data) != expected:
        raise ValueError(
            f"malformed keyboard {label} response: expected {expected} byte(s), "
            f"got {len(resp.data)}")
    values = struct.unpack(fmt, resp.data)
    if versioned and values[0] != KEYBOARD_PROTOCOL_VERSION:
        raise ValueError(
            f"malformed keyboard {label} response: unsupported version {values[0]}")
    return dict(zip(fields, values))


class ChameleonCMD:
    """
        Chameleon cmd function
    """

    def __init__(self, chameleon: chameleon_com.ChameleonCom):
        """
        :param chameleon: chameleon instance, @see chameleon_device.Chameleon
        """
        self.device = chameleon

    @expect_response(Status.SUCCESS)
    def get_app_version(self):
        """
            Get firmware version number(application)
        """
        resp = self.device.send_cmd_sync(Command.GET_APP_VERSION)
        if resp.status == Status.SUCCESS:
            resp.parsed = struct.unpack('!BB', resp.data)
        # older protocol, must upgrade!
        if resp.status == 0 and resp.data == b'\x00\x01':
            print("Chameleon does not understand new protocol. Please update firmware")
            return chameleon_com.Response(cmd=Command.GET_APP_VERSION,
                                          status=Status.NOT_IMPLEMENTED)
        return resp

    @expect_response(Status.SUCCESS)
    def get_device_chip_id(self):
        """
            Get device chip id
        """
        resp = self.device.send_cmd_sync(Command.GET_DEVICE_CHIP_ID)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data.hex()
        return resp

    @expect_response(Status.SUCCESS)
    def get_device_address(self):
        """
            Get device address
        """
        resp = self.device.send_cmd_sync(Command.GET_DEVICE_ADDRESS)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data.hex()
        return resp

    @expect_response(Status.SUCCESS)
    def get_git_version(self):
        resp = self.device.send_cmd_sync(Command.GET_GIT_VERSION)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data.decode('utf-8')
        return resp

    @expect_response(Status.SUCCESS)
    def get_device_mode(self):
        resp = self.device.send_cmd_sync(Command.GET_DEVICE_MODE)
        if resp.status == Status.SUCCESS:
            resp.parsed, = struct.unpack('!?', resp.data)
        return resp

    @expect_response(Status.SUCCESS)
    def active_slot_snapshot_begin(self):
        """Freeze and identify the exact active MIFARE Classic slot."""
        payload = struct.pack(
            "!BB", ACTIVE_SLOT_SNAPSHOT_VERSION, ACTIVE_SLOT_SNAPSHOT_BEGIN)
        resp = self.device.send_cmd_sync(Command.ACTIVE_SLOT_SNAPSHOT, payload)
        if resp.status == Status.SUCCESS:
            try:
                if len(resp.data) < 13:
                    raise UnexpectedResponseError(
                        "malformed active-slot snapshot BEGIN response: revision unavailable")
                revision, = struct.unpack_from("!I", resp.data, 9)
                if revision == 0:
                    raise UnexpectedResponseError(
                        "malformed active-slot snapshot BEGIN response: zero revision")
                if len(resp.data) != 13:
                    raise UnexpectedResponseError(
                        "malformed active-slot snapshot BEGIN response: "
                        f"expected 13 bytes, got {len(resp.data)}")
                version, operation, slot, tag_type, owner_generation, _ = \
                    struct.unpack("!BBBHII", resp.data)
                mifare_types = {
                    TagSpecificType.MIFARE_Mini,
                    TagSpecificType.MIFARE_1024,
                    TagSpecificType.MIFARE_2048,
                    TagSpecificType.MIFARE_4096,
                }
                try:
                    parsed_type = TagSpecificType(tag_type)
                except ValueError as error:
                    raise UnexpectedResponseError(
                        "malformed active-slot snapshot BEGIN response: "
                        f"unknown tag type {tag_type}") from error
                if (version != ACTIVE_SLOT_SNAPSHOT_VERSION or
                        operation != ACTIVE_SLOT_SNAPSHOT_BEGIN or slot > 7 or
                        parsed_type not in mifare_types or owner_generation == 0):
                    raise UnexpectedResponseError(
                        "malformed active-slot snapshot BEGIN response")
            except UnexpectedResponseError:
                revision = struct.unpack_from("!I", resp.data, 9)[0] \
                    if len(resp.data) >= 13 else 0
                if revision != 0:
                    try:
                        _abort_snapshot_revision(self.device, revision)
                    except Exception:
                        _close_snapshot_device(self.device)
                else:
                    _close_snapshot_device(self.device)
                raise
            resp.parsed = {
                "version": version,
                "slot": slot,
                "tag_type": parsed_type,
                "owner_generation": owner_generation,
                "revision": revision,
            }
        return resp

    @expect_response(Status.SUCCESS)
    def active_slot_snapshot_save_release(self, revision):
        """Force-persist the frozen HF dump and restore field sensing."""
        _require_snapshot_revision(revision)
        payload = struct.pack(
            "!BBI", ACTIVE_SLOT_SNAPSHOT_VERSION,
            ACTIVE_SLOT_SNAPSHOT_SAVE_RELEASE, revision)
        try:
            resp = self.device.send_cmd_sync(
                Command.ACTIVE_SLOT_SNAPSHOT, payload,
                timeout=ACTIVE_SLOT_SNAPSHOT_SAVE_TIMEOUT_SECONDS)
        except TimeoutError:
            _close_snapshot_device(self.device)
            raise
        if resp.status == Status.SUCCESS:
            try:
                resp.parsed = _parse_snapshot_end(
                    resp, ACTIVE_SLOT_SNAPSHOT_SAVE_RELEASE, revision)
            except UnexpectedResponseError:
                _close_snapshot_device(self.device)
                raise
        return resp

    @expect_response(Status.SUCCESS)
    def active_slot_snapshot_abort(self, revision):
        """Discard the frozen transaction without writing flash."""
        _require_snapshot_revision(revision)
        payload = struct.pack(
            "!BBI", ACTIVE_SLOT_SNAPSHOT_VERSION,
            ACTIVE_SLOT_SNAPSHOT_ABORT, revision)
        resp = self.device.send_cmd_sync(
            Command.ACTIVE_SLOT_SNAPSHOT, payload, timeout=5)
        if resp.status == Status.SUCCESS:
            resp.parsed = _parse_snapshot_end(
                resp, ACTIVE_SLOT_SNAPSHOT_ABORT, revision)
        return resp

    def is_device_reader_mode(self) -> bool:
        """
            Get device mode, reader or tag.

        :return: True is reader mode, else tag mode
        """
        return self.get_device_mode()

    # Note: Will return NOT_IMPLEMENTED if one tries to set reader mode on Lite
    @expect_response(Status.SUCCESS)
    def change_device_mode(self, mode):
        data = struct.pack('!B', mode)
        return self.device.send_cmd_sync(Command.CHANGE_DEVICE_MODE, data)

    def set_device_reader_mode(self, reader_mode: bool = True):
        """
            Change device mode, reader or tag.

        :param reader_mode: True if reader mode, False if tag mode.
        :return:
        """
        self.change_device_mode(reader_mode)

    @expect_response(Status.SUCCESS)
    def keyboard_upload_begin(self, total_length: int, crc32: int):
        """Start a version-1 keyboard bytecode upload."""
        _require_keyboard_uint(
            "total_length", total_length, KEYBOARD_MAX_PROGRAM_BYTES, 1)
        _require_keyboard_uint("crc32", crc32, 0xFFFFFFFF)
        payload = struct.pack(
            "!BHI", KEYBOARD_PROTOCOL_VERSION, total_length, crc32)
        resp = self.device.send_cmd_sync(Command.KEYBOARD_UPLOAD_BEGIN, payload)
        if resp.status == Status.SUCCESS:
            resp.parsed = _parse_keyboard_response(
                resp, "!BIHH", "upload-begin",
                ("version", "upload_id", "next_offset", "max_chunk"),
                versioned=True)
        return resp

    @expect_response(Status.SUCCESS)
    def keyboard_upload_chunk(self, upload_id: int, offset: int, data: bytes):
        """Upload one chunk; the seven-byte header leaves 4089 data bytes."""
        _require_keyboard_uint("upload_id", upload_id, 0xFFFFFFFF)
        _require_keyboard_uint("offset", offset, KEYBOARD_MAX_PROGRAM_BYTES)
        if not isinstance(data, (bytes, bytearray, memoryview)):
            raise ValueError("data must be a byte value")
        data = bytes(data)
        if not 1 <= len(data) <= KEYBOARD_MAX_CHUNK_BYTES:
            raise ValueError(
                f"data length must be 1..{KEYBOARD_MAX_CHUNK_BYTES}")
        if offset + len(data) > KEYBOARD_MAX_PROGRAM_BYTES:
            raise ValueError("chunk extends beyond the 4096-byte program limit")
        payload = struct.pack(
            "!BIH", KEYBOARD_PROTOCOL_VERSION, upload_id, offset) + data
        resp = self.device.send_cmd_sync(Command.KEYBOARD_UPLOAD_CHUNK, payload)
        if resp.status == Status.SUCCESS:
            resp.parsed = _parse_keyboard_response(
                resp, "!IH", "upload-chunk", ("upload_id", "next_offset"))
        return resp

    @expect_response(Status.SUCCESS)
    def keyboard_upload_commit(self, upload_id: int):
        """Validate and atomically commit a completed upload."""
        _require_keyboard_uint("upload_id", upload_id, 0xFFFFFFFF)
        payload = struct.pack("!BI", KEYBOARD_PROTOCOL_VERSION, upload_id)
        resp = self.device.send_cmd_sync(Command.KEYBOARD_UPLOAD_COMMIT, payload)
        if resp.status == Status.SUCCESS:
            resp.parsed = _parse_keyboard_response(
                resp, "!IHI", "upload-commit",
                ("commit_id", "length", "crc32"))
        return resp

    @expect_response(Status.SUCCESS)
    def keyboard_run(self, commit_id: int, outputs: int):
        """Request execution over USB or an authenticated BLE connection."""
        _require_keyboard_uint("commit_id", commit_id, 0xFFFFFFFF, 1)
        _require_keyboard_uint("outputs", outputs, 3, 1)
        payload = struct.pack(
            "!BIB", KEYBOARD_PROTOCOL_VERSION, commit_id, outputs)
        resp = self.device.send_cmd_sync(Command.KEYBOARD_RUN, payload)
        if resp.status == Status.SUCCESS:
            resp.parsed = _parse_keyboard_response(
                resp, "!I", "run", ("run_id",))
        return resp

    @expect_response(Status.SUCCESS)
    def keyboard_set_temporary_ble_name(self, name: Optional[str] = None):
        """Set a volatile BLE name, or restore the board default with None."""
        if name is None:
            encoded = b""
        elif not isinstance(name, str):
            raise ValueError("name must be text or None")
        else:
            encoded = name.encode("utf-8")
        if len(encoded) > 26 or any(byte < 0x20 or byte == 0x7F for byte in encoded):
            raise ValueError(
                "BLE name must encode to at most 26 bytes without control characters")
        payload = bytes((KEYBOARD_PROTOCOL_VERSION, len(encoded))) + encoded
        resp = self.device.send_cmd_sync(
            Command.KEYBOARD_SET_TEMP_BLE_NAME, payload)
        if resp.status == Status.SUCCESS:
            if len(resp.data) < 2 or resp.data[0] != KEYBOARD_PROTOCOL_VERSION or \
                    resp.data[1] != len(resp.data) - 2:
                raise ValueError("malformed keyboard BLE-name response")
            try:
                resp.parsed = resp.data[2:].decode("utf-8")
            except UnicodeDecodeError as error:
                raise ValueError(
                    "malformed keyboard BLE-name response: invalid UTF-8") from error
        return resp

    @expect_response(Status.SUCCESS)
    def keyboard_arm_ble(self, commit_id: int):
        """Advertise and run once when an authenticated BLE HID host connects."""
        _require_keyboard_uint("commit_id", commit_id, 0xFFFFFFFF, 1)
        payload = struct.pack("!BI", KEYBOARD_PROTOCOL_VERSION, commit_id)
        resp = self.device.send_cmd_sync(Command.KEYBOARD_ARM_BLE, payload)
        if resp.status == Status.SUCCESS:
            resp.parsed = _parse_keyboard_response(
                resp, "!I", "arm", ("run_id",))
        return resp

    @expect_response(Status.SUCCESS)
    def keyboard_cancel(self):
        resp = self.device.send_cmd_sync(Command.KEYBOARD_CANCEL)
        if resp.status == Status.SUCCESS and resp.data:
            raise ValueError(
                "malformed keyboard cancel response: expected 0 byte(s), "
                f"got {len(resp.data)}")
        return resp

    @expect_response(Status.SUCCESS)
    def keyboard_get_status(self):
        resp = self.device.send_cmd_sync(Command.KEYBOARD_GET_STATUS)
        if resp.status == Status.SUCCESS:
            parsed = _parse_keyboard_response(
                resp, "!BBBBIIIHHHHI", "status",
                ("version", "state", "error", "outputs", "upload_id",
                 "commit_id", "run_id", "expected", "received", "pc",
                 "length", "crc32"), versioned=True)
            states = (
                "empty", "uploading", "ready", "running", "complete",
                "cancelled", "error", "armed")
            if parsed["state"] >= len(states):
                raise ValueError(
                    f"malformed keyboard status response: invalid state {parsed['state']}")
            if parsed["outputs"] & ~0x03:
                raise ValueError(
                    "malformed keyboard status response: invalid output mask "
                    f"{parsed['outputs']}")
            if parsed["error"] > 13:
                raise ValueError(
                    f"malformed keyboard status response: invalid error {parsed['error']}")
            if parsed["received"] > parsed["expected"]:
                raise ValueError(
                    "malformed keyboard status response: received exceeds expected")
            if parsed["pc"] > parsed["length"]:
                raise ValueError(
                    "malformed keyboard status response: pc exceeds length")
            if parsed["expected"] > KEYBOARD_MAX_PROGRAM_BYTES or \
                    parsed["length"] > KEYBOARD_MAX_PROGRAM_BYTES:
                raise ValueError(
                    "malformed keyboard status response: length exceeds limit")
            parsed["state_name"] = states[parsed["state"]]
            resp.parsed = parsed
        return resp

    @expect_response(Status.SUCCESS)
    def keyboard_clear(self):
        resp = self.device.send_cmd_sync(Command.KEYBOARD_CLEAR)
        if resp.status == Status.SUCCESS and resp.data:
            raise ValueError(
                "malformed keyboard clear response: expected 0 byte(s), "
                f"got {len(resp.data)}")
        return resp

    def keyboard_upload(self, program: bytes):
        """Upload and commit bytecode, checking every acknowledged field."""
        if not isinstance(program, (bytes, bytearray, memoryview)):
            raise ValueError("program must be a byte value")
        program = bytes(program)
        if not 1 <= len(program) <= KEYBOARD_MAX_PROGRAM_BYTES:
            raise ValueError("program length must be 1..4096")
        crc32 = zlib.crc32(program) & 0xFFFFFFFF

        begun = self.keyboard_upload_begin(len(program), crc32)
        if begun["next_offset"] != 0 or begun["max_chunk"] != KEYBOARD_MAX_CHUNK_BYTES:
            raise ValueError("keyboard upload-begin metadata mismatch")
        upload_id = begun["upload_id"]
        offset = 0
        while offset < len(program):
            chunk = program[offset:offset + KEYBOARD_MAX_CHUNK_BYTES]
            acknowledged = self.keyboard_upload_chunk(upload_id, offset, chunk)
            next_offset = offset + len(chunk)
            if (acknowledged["upload_id"] != upload_id or
                    acknowledged["next_offset"] != next_offset):
                raise ValueError("keyboard upload-chunk metadata mismatch")
            offset = next_offset

        committed = self.keyboard_upload_commit(upload_id)
        if (committed["length"] != len(program) or
                committed["crc32"] != crc32):
            raise ValueError("keyboard upload-commit metadata mismatch")
        return committed

    @expect_response(Status.HF_TAG_OK)
    def hf14a_scan(self):
        """
        14a tags in the scanning field.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.HF14A_SCAN)
        if resp.status == Status.HF_TAG_OK:
            # uidlen[1]|uid[uidlen]|atqa[2]|sak[1]|atslen[1]|ats[atslen]
            offset = 0
            data = []
            while offset < len(resp.data):
                uidlen, = struct.unpack_from('!B', resp.data, offset)
                offset += struct.calcsize('!B')
                uid, atqa, sak, atslen = struct.unpack_from(f'!{uidlen}s2s1sB', resp.data, offset)
                offset += struct.calcsize(f'!{uidlen}s2s1sB')
                ats, = struct.unpack_from(f'!{atslen}s', resp.data, offset)
                offset += struct.calcsize(f'!{atslen}s')
                data.append({'uid': uid, 'atqa': atqa, 'sak': sak, 'ats': ats})
            resp.parsed = data
        return resp

    def ble_scan_start(self, active: bool = False):
        """
        Start a BLE scan. Passive (default) is listen-only — the device transmits
        nothing. Active (active=True) also sends scan requests to collect scan
        responses (e.g. the full device name); this is the standard BLE discovery
        exchange, not disruption.
        """
        return self.device.send_cmd_sync(Command.BLE_SCAN_START,
                                         struct.pack('!B', 1 if active else 0))

    def ble_scan_stop(self):
        """
        Stop the passive BLE scan.
        """
        return self.device.send_cmd_sync(Command.BLE_SCAN_STOP)

    @expect_response(Status.SUCCESS)
    def ble_scan_get_count(self):
        """
        Number of distinct BLE devices seen so far in the current/last scan.
        """
        resp = self.device.send_cmd_sync(Command.BLE_SCAN_GET_COUNT)
        if resp.status == Status.SUCCESS:
            _require_ble_length(resp, 1, "BLE scan count")
            resp.parsed = resp.data[0]
        return resp

    @expect_response(Status.SUCCESS)
    def ble_scan_get_results(self, start_index: int = 0):
        """
        Fetch discovered BLE device records, starting at start_index.

        Wire format per record:
            addr[6] | addr_type[1] | rssi[1, signed] | adv_len[1] | adv[adv_len]

        :return: list of dicts {addr(bytes, LE), addr_type(int), rssi(int), adv(bytes)}
        """
        _require_ble_uint("start_index", start_index, 0xFF)
        data = struct.pack('!B', start_index)
        resp = self.device.send_cmd_sync(Command.BLE_SCAN_GET_RESULTS, data)
        if resp.status == Status.SUCCESS:
            offset = 0
            devices = []
            while offset < len(resp.data):
                if len(resp.data) - offset < 9:
                    raise ValueError("malformed BLE scan response: truncated record header")
                addr = resp.data[offset:offset + 6]
                offset += 6
                addr_type, rssi, adv_len = struct.unpack_from('!BbB', resp.data, offset)
                offset += 3
                if len(resp.data) - offset < adv_len:
                    raise ValueError("malformed BLE scan response: truncated advertising data")
                adv = resp.data[offset:offset + adv_len]
                offset += adv_len
                devices.append({'addr': addr, 'addr_type': addr_type, 'rssi': rssi, 'adv': adv})
            resp.parsed = devices
        return resp

    @expect_response(Status.SUCCESS)
    def ble_advertising_get(self):
        """Query whether the device is currently advertising."""
        resp = self.device.send_cmd_sync(Command.BLE_ADVERTISING_GET)
        if resp.status == Status.SUCCESS:
            _require_ble_length(resp, 1, "BLE advertising state")
            resp.parsed = bool(resp.data[0])
        return resp

    @expect_response(Status.SUCCESS)
    def ble_advertising_set(self, enabled: bool, erase_bonds: bool = False):
        """
        Enable or disable local advertising.

        :param enabled: True to start advertising, False to stop it.
        :param erase_bonds: When enabling, optionally clear bonds first.
        """
        data = struct.pack('!BB', 1 if enabled else 0, 1 if erase_bonds else 0)
        resp = self.device.send_cmd_sync(Command.BLE_ADVERTISING_SET, data)
        if resp.status == Status.SUCCESS:
            _require_ble_length(resp, 1, "BLE advertising state")
            resp.parsed = bool(resp.data[0])
        return resp

    @expect_response(Status.SUCCESS)
    def ble_link_probe(self, global_mode: bool = False):
        """Start a native firmware-side BLE link probe, targeted or global."""
        if global_mode:
            return self.device.send_cmd_sync(Command.BLE_LINK_PROBE, b"\x01")
        return self.device.send_cmd_sync(Command.BLE_LINK_PROBE)

    # --- Own-radio identity / radio power (cybersecurity fork) -------------
    # These mutate only OUR radio (local settings — no scope selector). The
# environment-wide / scan-buffer-wide tools live further down in this file.

    @expect_response(Status.SUCCESS)
    def ble_set_addr(self, mode: int, addr: bytes = None):
        """
        Change the device's own BLE GAP address.

        :param mode:
            0 = restore the FICR-derived original address
            1 = static-random from host (6 bytes LE, addr required)
            2 = firmware-generated random private resolvable
            3 = firmware-generated random private non-resolvable
        :param addr: 6-byte address (LE) — required when mode == 1.
        :raises SerialProtocolError: if a link is active (DEVICE_MODE_ERROR).
        """
        if mode == 1:
            if not isinstance(addr, (bytes, bytearray, memoryview)):
                raise ValueError("mode 1 (static-random) requires a 6-byte addr")
            addr = bytes(addr)
            if len(addr) != 6:
                raise ValueError("mode 1 (static-random) requires a 6-byte addr")
            data = struct.pack('!B', mode) + addr
        elif mode in (0, 2, 3):
            data = struct.pack('!B', mode)
        else:
            raise ValueError(f"unknown ble addr mode: {mode}")
        return self.device.send_cmd_sync(Command.BLE_SET_ADDR, data)

    @expect_response(Status.SUCCESS)
    def ble_get_addr(self):
        """
        Read the device's currently-active BLE GAP address.

        :returns: dict {'addr_type': int, 'addr': bytes (6, LE)}.
        """
        resp = self.device.send_cmd_sync(Command.BLE_GET_ADDR)
        if resp.status == Status.SUCCESS:
            _require_ble_length(resp, 7, "BLE address")
            resp.parsed = {
                'addr_type': resp.data[0],
                'addr': resp.data[1:7],
            }
        return resp

    @expect_response(Status.SUCCESS)
    def ble_radio_set(self, on: bool):
        """
        Turn the device's own BLE radio on/off. Off = stealth: stops
        advertising + scan + drops any active central link. On resumes.
        """
        return self.device.send_cmd_sync(
            Command.BLE_RADIO_SET, struct.pack('!B', 1 if on else 0))

    @expect_response(Status.SUCCESS)
    def ble_radio_get(self):
        """
        Snapshot of the device's own radio state.

        :returns: dict {'on': bool, 'advertising': bool, 'scanning': bool,
                        'central_link': bool}.
        """
        resp = self.device.send_cmd_sync(Command.BLE_RADIO_GET)
        if resp.status == Status.SUCCESS:
            _require_ble_length(resp, 4, "BLE radio state")
            resp.parsed = {
                'on':           bool(resp.data[0]),
                'advertising':  bool(resp.data[1]),
                'scanning':     bool(resp.data[2]),
                'central_link': bool(resp.data[3]),
            }
        return resp

    # --- Stress / broadcast (cybersecurity fork, operator-authorised) ---------
    # Per-call scope selectable:
    #   0 = single target (already-connected central link / host-picked addr)
    #   1 = scan-buffer-wide (every address cached by the passive scanner)
    #   2 = full environment-wide broadcast on the 2.4 GHz BLE spectrum

    @expect_response(Status.SUCCESS)
    def ble_flood_start(self, value_handle: int, payload_size: int,
                        max_iterations: int = 0, interval_ms: int = 5,
                        scope: int = 0):
        """
        Rapid WRITE_CMD spam. Per-call scope selectable (see CLAUDE.md).

        :param value_handle: characteristic value handle (used by scope 0/1).
        :param payload_size: 1..BLE_FUZZ_PAYLOAD_MAX bytes per write.
        :param max_iterations: 0 = until ble_flood_stop() (ignored on scope 2).
        :param interval_ms: ms between ticks (floor 1; legacy adv regulatory
                            floor of 100ms is enforced by the firmware on
                            scope 2).
        :param scope: 0=single target, 1=scan-buffer-wide, 2=environment-wide
                      broadcast (non-connectable adv spam).
        """
        if scope not in (0, 1, 2):
            raise ValueError("scope must be 0/1/2")
        if scope in (0, 1):
            if not 1 <= value_handle <= 0xFFFF:
                raise ValueError("value_handle must be 1..0xffff")
            if not 1 <= payload_size <= 20:
                raise ValueError("payload_size must be 1..20")
        else:
            if not 0 <= value_handle <= 0xFF:
                raise ValueError("broadcast fill byte must be 0..255")
            if not 0 <= payload_size <= 0xFF:
                raise ValueError("payload_size must be 0..255 for scope=2")
        if not 0 <= max_iterations <= 0xFFFF:
            raise ValueError("max_iterations must be 0..65535")
        if not 1 <= interval_ms <= 0xFFFF:
            raise ValueError("interval_ms must be 1..65535")
        data = struct.pack('!BHBHH', scope,
                           value_handle,
                           payload_size,
                           max_iterations,
                           interval_ms)
        return self.device.send_cmd_sync(Command.BLE_FLOOD_START, data)

    @expect_response(Status.SUCCESS)
    def ble_flood_stop(self):
        # Stops both the WRITE_CMD flood AND the environment-wide adv flood.
        return self.device.send_cmd_sync(Command.BLE_FLOOD_STOP)

    @expect_response(Status.SUCCESS)
    def ble_flood_count(self):
        resp = self.device.send_cmd_sync(Command.BLE_FLOOD_COUNT)
        if resp.status == Status.SUCCESS:
            _require_ble_length(resp, 4, "BLE flood count")
            resp.parsed, = struct.unpack('!I', resp.data)
        return resp

    @expect_response(Status.SUCCESS)
    def ble_kick(self, cycles: int = 1, scope: int = 0):
        """
        Force-disconnect the selected link(s). Single-target scope supports one
        disconnect; scan-buffer scope supports 1..10 cycles per peer.

        :param cycles: 1..10 disconnect cycles.
        :param scope: 0=single target (current central link),
                      1=scan-buffer-wide (connect → kick → next).
        """
        if not 1 <= cycles <= 10:
            raise ValueError("cycles must be 1..10")
        if scope not in (0, 1):
            raise ValueError("scope must be 0 or 1")
        if scope == 0 and cycles != 1:
            raise ValueError("scope=single supports exactly one disconnect cycle")
        return self.device.send_cmd_sync(Command.BLE_KICK,
                                        struct.pack('!BB', scope, cycles))

    @expect_response(Status.SUCCESS)
    def ble_adv_flood_start(self, fill_byte: int = 0x00, interval_units: int = 1):
        """
        Full environment-wide broadcast on the 2.4 GHz BLE spectrum —
        non-connectable advertising spam, max payload, regulatory-minimum
        interval (100ms). Every scanner / peer in range sees it.

        :param fill_byte: byte that fills the 26 manufacturer-data bytes.
        :param interval_units: 100ms multiples, 1..102.
        """
        if not 0 <= fill_byte <= 0xFF:
            raise ValueError("fill_byte must be 0..255")
        if not 1 <= interval_units <= 102:
            raise ValueError("interval_units must be 1..102")
        return self.device.send_cmd_sync(
            Command.BLE_ADV_FLOOD_START,
            struct.pack('!BBB', 2, fill_byte, interval_units))

    @expect_response(Status.SUCCESS)
    def ble_adv_flood_stop(self):
        return self.device.send_cmd_sync(Command.BLE_ADV_FLOOD_STOP)

    @expect_response(Status.SUCCESS)
    def ble_adv_lab_start(self, advertising_data=b'', scan_response_data=b'', *,
                          names=(), name_target=0, interval_ms=250,
                          rotation_ms=0, duration_ms=0, max_advertising_events=0,
                          mode=1, profile=None):
        advertising_data = bytes(advertising_data)
        scan_response_data = bytes(scan_response_data)
        adv_has_name = validate_ble_advertising_data(advertising_data)
        scan_has_name = validate_ble_advertising_data(
            scan_response_data, scan_response=True)
        if adv_has_name and scan_has_name:
            raise ValueError("local names cannot appear in both packets")
        names = tuple(names)
        if profile is None:
            profile = 2 if len(names) > 1 else 1
        if profile not in (1, 2, 3) or mode not in (0, 1, 2):
            raise ValueError("profile and mode must be supported advertising-lab values")
        if name_target not in (0, 1, 2):
            raise ValueError("name_target must be 0, 1, or 2")
        if not 20 <= interval_ms <= 10240 or (mode != 0 and interval_ms < 100):
            raise ValueError("interval_ms is outside the selected mode's range")
        if duration_ms and (duration_ms % 10 or not 10 <= duration_ms <= 655350):
            raise ValueError("duration_ms must be 0 or a 10ms multiple up to 655350")
        if not 0 <= max_advertising_events <= 255:
            raise ValueError("max_advertising_events must be 0..255")
        if len(names) > 32:
            raise ValueError("at most 32 names may be rotated")
        if ((profile == 1 and len(names) > 1) or
                (profile == 2 and not names) or
                (profile == 3 and names)):
            raise ValueError("profile does not match the supplied name list")
        if not names and (name_target != 0 or rotation_ms != 0):
            raise ValueError("name_target and rotation_ms require at least one name")
        if names and name_target == 0:
            raise ValueError("names require name_target 1 (advertisement) or 2 (scan response)")
        if names and (adv_has_name or scan_has_name):
            raise ValueError("base data must not contain a local name when names are supplied")
        if mode == 2 and (scan_response_data or name_target == 2):
            raise ValueError("non-scannable mode cannot use scan-response data")
        interval_units = (interval_ms * 1000 + 624) // 625
        effective_interval_ms = (interval_units * 625 + 999) // 1000
        if len(names) > 1 and not max(100, effective_interval_ms) <= rotation_ms <= 65535:
            raise ValueError("rotation_ms must be at least 100 and the advertising interval")
        if len(names) <= 1 and rotation_ms != 0:
            raise ValueError("rotation_ms must be 0 unless multiple names are supplied")
        encoded_names = bytearray()
        target_length = len(advertising_data if name_target == 1 else scan_response_data)
        for name in names:
            try:
                encoded = name.encode('utf-8')
            except UnicodeEncodeError as error:
                raise ValueError("names must contain valid UTF-8") from error
            if not encoded or len(encoded) > 26 or any(byte < 0x20 or byte == 0x7F for byte in encoded):
                raise ValueError("names must be 1..26 UTF-8 bytes without control characters")
            if target_length + len(encoded) + 2 > 31:
                raise ValueError("a name does not fit in its selected legacy packet")
            encoded_names.append(len(encoded))
            encoded_names.extend(encoded)
        payload = struct.pack(
            '!BBBBHHHBBBB', 1, profile, mode, name_target, interval_units,
            rotation_ms, duration_ms // 10, max_advertising_events,
            len(advertising_data), len(scan_response_data), len(names))
        payload += advertising_data + scan_response_data + encoded_names
        resp = self.device.send_cmd_sync(Command.BLE_ADV_LAB_START, payload)
        if resp.status == Status.SUCCESS:
            resp.parsed = _parse_ble_adv_lab_status(resp)
        return resp

    @expect_response(Status.SUCCESS)
    def ble_adv_lab_status(self):
        resp = self.device.send_cmd_sync(Command.BLE_ADV_LAB_STATUS)
        if resp.status == Status.SUCCESS:
            resp.parsed = _parse_ble_adv_lab_status(resp)
        return resp

    @expect_response(Status.SUCCESS)
    def ble_adv_lab_stop(self):
        resp = self.device.send_cmd_sync(Command.BLE_ADV_LAB_STOP)
        if resp.status == Status.SUCCESS:
            resp.parsed = _parse_ble_adv_lab_status(resp)
        return resp

    # --- Directed BLE GATT fuzzing harness (central role) -------------------
    # Point-to-point against ONE target the operator specifies by address or broadcast if selected.

    def ble_connect(self, addr: bytes, addr_type: int = 0):
        """
        Connect to a single BLE target.

        :param addr: 6-byte target address, little-endian (as the scanner reports)
        :param addr_type: BLE GAP address type (0=public, 1=random-static,
                          2=random-private-resolvable,
                          3=random-private-non-resolvable). Default 0.
        """
        _require_ble_uint("addr_type", addr_type, 3)
        if not isinstance(addr, (bytes, bytearray, memoryview)):
            raise ValueError("addr must be a 6-byte value")
        addr = bytes(addr)
        if len(addr) != 6:
            raise ValueError("addr must be a 6-byte value")
        data = struct.pack('!B', addr_type) + addr
        return self.device.send_cmd_sync(Command.BLE_CONNECT, data)

    def ble_disconnect(self):
        """Disconnect from the target, freeing it to reconnect normally."""
        return self.device.send_cmd_sync(Command.BLE_DISCONNECT)

    @expect_response(Status.SUCCESS)
    def ble_central_state(self):
        """
        Poll harness state. Returns a dict with conn_state, disc_state,
        char_count, fuzz_state, fuzz_sent, target_alive, last_reason,
        probe_state, probe_result.
        """
        resp = self.device.send_cmd_sync(Command.BLE_CENTRAL_STATE)
        if resp.status == Status.SUCCESS:
            # Ten/twelve-byte prefixes are retained for older firmware. Current
            # firmware appends operation state without changing that prefix.
            if len(resp.data) not in (10, 12) and len(resp.data) < 21:
                raise ValueError(
                    "malformed BLE central state response: expected 10, 12, or at least 21 "
                    f"bytes, got {len(resp.data)}")
            conn, disc, chars, fuzz, sent_hi, sent_lo, alive, reason, probe_state, probe_result = \
                struct.unpack_from('!10B', resp.data, 0)
            state = {
                'conn_state': conn, 'disc_state': disc, 'char_count': chars,
                'fuzz_state': fuzz, 'fuzz_sent': (sent_hi << 8) | sent_lo,
                'target_alive': bool(alive), 'last_reason': reason,
                'probe_state': probe_state, 'probe_result': probe_result,
            }
            if len(resp.data) >= 12:
                probe_index, probe_total = struct.unpack_from('!2B', resp.data, 10)
                state['probe_index'] = probe_index
                state['probe_total'] = probe_total
            if len(resp.data) >= 21:
                state.update({
                    'flood_state': resp.data[12],
                    'flood_sent': struct.unpack_from('!I', resp.data, 13)[0],
                    'read_state': resp.data[17],
                    'write_state': resp.data[18],
                    'notification_count': struct.unpack_from('!H', resp.data, 19)[0],
                })
            resp.parsed = state
        return resp

    def ble_gatt_discover(self):
        """Start enumerating the connected target's GATT characteristics (async)."""
        return self.device.send_cmd_sync(Command.BLE_GATT_DISCOVER)

    @expect_response(Status.SUCCESS)
    def ble_gatt_get_chars(self, start_index: int = 0):
        """
        Fetch discovered characteristics. Wire per char:
        value_handle[2] | props[1] | uuid_type[1] | uuid[2] (big-endian).
        """
        _require_ble_uint("start_index", start_index, 0xFF)
        data = struct.pack('!B', start_index)
        resp = self.device.send_cmd_sync(Command.BLE_GATT_GET_CHARS, data)
        if resp.status == Status.SUCCESS:
            if len(resp.data) % 6:
                raise ValueError("malformed BLE characteristic response: partial 6-byte record")
            offset = 0
            chars = []
            while offset + 6 <= len(resp.data):
                value_handle, props, uuid_type, uuid = struct.unpack_from('!HBBH', resp.data, offset)
                offset += 6
                chars.append({'handle': value_handle, 'props': props,
                              'uuid_type': uuid_type, 'uuid': uuid})
            resp.parsed = chars
        return resp

    def ble_desc_discover(self):
        """Start enumerating all descriptors of the connected target (async)."""
        return self.device.send_cmd_sync(Command.BLE_DESC_DISCOVER)

    @expect_response(Status.SUCCESS)
    def ble_get_descs(self, start_index: int = 0):
        """
        Fetch discovered descriptors: dict {state, items:[{handle, uuid_type, uuid}]}.
        state: 0 idle, 1 discovering, 2 done, 3 error. Wire: state[1] then per
        descriptor handle[2] | uuid_type[1] | uuid[2] (big-endian).
        """
        _require_ble_uint("start_index", start_index, 0xFF)
        resp = self.device.send_cmd_sync(Command.BLE_DESC_GET, struct.pack('!B', start_index))
        if resp.status == Status.SUCCESS:
            if len(resp.data) < 1 or (len(resp.data) - 1) % 5:
                raise ValueError("malformed BLE descriptor response: invalid record length")
            out = {'state': 0, 'items': []}
            out['state'] = resp.data[0]
            off = 1
            while off + 5 <= len(resp.data):
                handle, uuid_type, uuid = struct.unpack_from('!HBH', resp.data, off)
                off += 5
                out['items'].append({'handle': handle, 'uuid_type': uuid_type, 'uuid': uuid})
            resp.parsed = out
        return resp

    def ble_svc_discover(self):
        """Start discovering the connected target's primary services (async)."""
        return self.device.send_cmd_sync(Command.BLE_SVC_DISCOVER)

    @expect_response(Status.SUCCESS)
    def ble_get_svcs(self, start_index: int = 0):
        """
        Fetch discovered primary services: dict {state, items:[{uuid_type, uuid,
        start, end}]}. Wire: state[1] then per service uuid_type[1] | uuid[2] |
        start_handle[2] | end_handle[2] (big-endian).
        """
        _require_ble_uint("start_index", start_index, 0xFF)
        resp = self.device.send_cmd_sync(Command.BLE_SVC_GET, struct.pack('!B', start_index))
        if resp.status == Status.SUCCESS:
            if len(resp.data) < 1 or (len(resp.data) - 1) % 7:
                raise ValueError("malformed BLE service response: invalid record length")
            out = {'state': 0, 'items': []}
            out['state'] = resp.data[0]
            off = 1
            while off + 7 <= len(resp.data):
                uuid_type, uuid, start, end = struct.unpack_from('!BHHH', resp.data, off)
                off += 7
                out['items'].append({'uuid_type': uuid_type, 'uuid': uuid,
                                     'start': start, 'end': end})
            resp.parsed = out
        return resp

    def ble_devinfo_start(self):
        """
        Start reading the connected target's standard information characteristics
        (GAP name/appearance, Device Information Service, battery). Read-only;
        requires 'discover' to have run first. Async — poll ble_get_devinfo().
        """
        return self.device.send_cmd_sync(Command.BLE_DEVICE_INFO)

    @expect_response(Status.SUCCESS)
    def ble_get_devinfo(self):
        """
        Fetch collected device-info: dict {state, items:[{uuid, status, data}]}.
        state: 0 idle, 1 running, 2 done, 3 error. Wire: state[1] | count[1] then
        per field uuid[2] | status[1] | len[1] | data[len] (big-endian). status:
        0xFF = characteristic absent, else the ATT read status (0 = ok).
        """
        resp = self.device.send_cmd_sync(Command.BLE_GET_DEVICE_INFO)
        if resp.status == Status.SUCCESS:
            if len(resp.data) < 2:
                raise ValueError("malformed BLE device-info response: missing header")
            out = {'state': resp.data[0], 'items': []}
            count = resp.data[1]
            out['state'] = resp.data[0]
            off = 2  # skip state + count
            for _ in range(count):
                if len(resp.data) - off < 4:
                    raise ValueError("malformed BLE device-info response: truncated record header")
                uuid, st, ln = struct.unpack_from('!HBB', resp.data, off)
                off += 4
                if len(resp.data) - off < ln:
                    raise ValueError("malformed BLE device-info response: truncated value")
                val = bytes(resp.data[off:off + ln])
                off += ln
                out['items'].append({'uuid': uuid, 'status': st, 'data': val})
            if off != len(resp.data):
                raise ValueError("malformed BLE device-info response: trailing bytes")
            resp.parsed = out
        return resp

    def ble_fuzz_start(self, value_handle: int, max_iterations: int = 0, interval_ms: int = 50):
        """
        Start fuzzing: write mutated payloads to value_handle on the connected
        target, every interval_ms, up to max_iterations (0 = until stopped).
        """
        if not 1 <= value_handle <= 0xFFFF:
            raise ValueError("value_handle must be 1..0xffff")
        if not 0 <= max_iterations <= 0xFFFF:
            raise ValueError("max_iterations must be 0..65535")
        if not 1 <= interval_ms <= 0xFFFF:
            raise ValueError("interval_ms must be 1..65535")
        data = struct.pack('!HHH', value_handle, max_iterations, interval_ms)
        return self.device.send_cmd_sync(Command.BLE_FUZZ_START, data)

    def ble_fuzz_stop(self):
        """Stop fuzzing."""
        return self.device.send_cmd_sync(Command.BLE_FUZZ_STOP)

    def ble_gatt_read_start(self, value_handle: int):
        """Initiate a GATT read of value_handle on the connected target (async)."""
        _require_ble_uint("value_handle", value_handle, 0xFFFF, 1)
        return self.device.send_cmd_sync(Command.BLE_GATT_READ,
                                         struct.pack('!H', value_handle))

    @expect_response(Status.SUCCESS)
    def ble_gatt_read_result(self):
        """
        Fetch the last GATT read result: dict {state, gatt_status, data}.
        state: 0 idle, 1 pending, 2 ready.
        """
        resp = self.device.send_cmd_sync(Command.BLE_GATT_GET_READ)
        if resp.status == Status.SUCCESS:
            if len(resp.data) < 3:
                raise ValueError("malformed BLE read response: missing header")
            ln = resp.data[2]
            if len(resp.data) != 3 + ln:
                raise ValueError("malformed BLE read response: value length mismatch")
            resp.parsed = {'state': resp.data[0], 'gatt_status': resp.data[1],
                           'data': bytes(resp.data[3:])}
        return resp

    def ble_gatt_write_start(self, value_handle: int, data: bytes):
        """Write a value to a characteristic on the connected target (async)."""
        _require_ble_uint("value_handle", value_handle, 0xFFFF, 1)
        if not isinstance(data, (bytes, bytearray, memoryview)):
            raise ValueError("data must be a byte value")
        data = bytes(data)
        if not 1 <= len(data) <= 244:
            raise ValueError("data length must be 1..244")
        return self.device.send_cmd_sync(Command.BLE_GATT_WRITE,
                                          struct.pack('!H', value_handle) + data)

    @expect_response(Status.SUCCESS)
    def ble_gatt_write_result(self):
        """
        Fetch the last GATT write result: dict {state, gatt_status}.
        state: 0 idle, 1 pending, 2 done.
        """
        resp = self.device.send_cmd_sync(Command.BLE_GET_WRITE)
        if resp.status == Status.SUCCESS:
            _require_ble_length(resp, 2, "BLE write result")
            resp.parsed = {'state': resp.data[0], 'gatt_status': resp.data[1]}
        return resp

    @expect_response(Status.SUCCESS)
    def ble_get_mtu(self):
        """Effective ATT MTU of the connected target link (23 if not negotiated)."""
        resp = self.device.send_cmd_sync(Command.BLE_GET_MTU)
        if resp.status == Status.SUCCESS:
            _require_ble_length(resp, 2, "BLE MTU")
            resp.parsed, = struct.unpack('!H', resp.data)
        return resp

    def ble_subscribe(self, cccd_handle: int, mode: int = 1):
        """
        Subscribe to notifications/indications on the connected target by writing
        its CCCD. mode: 0=off, 1=notifications, 2=indications.
        """
        _require_ble_uint("cccd_handle", cccd_handle, 0xFFFF, 1)
        _require_ble_uint("mode", mode, 2)
        data = struct.pack('!HB', cccd_handle, mode)
        return self.device.send_cmd_sync(Command.BLE_SUBSCRIBE, data)

    def ble_find_cccd_start(self, value_handle: int):
        """Start discovering the CCCD descriptor of a characteristic (async)."""
        _require_ble_uint("value_handle", value_handle, 0xFFFE, 1)
        return self.device.send_cmd_sync(Command.BLE_FIND_CCCD,
                                         struct.pack('!H', value_handle))

    @expect_response(Status.SUCCESS)
    def ble_get_cccd(self):
        """
        Fetch the CCCD lookup result: dict {state, handle}.
        state: 0 idle, 1 searching, 2 found, 3 not-found.
        """
        resp = self.device.send_cmd_sync(Command.BLE_GET_CCCD)
        if resp.status == Status.SUCCESS:
            _require_ble_length(resp, 3, "BLE CCCD result")
            resp.parsed = {'state': resp.data[0],
                           'handle': (resp.data[1] << 8) | resp.data[2]}
        return resp

    @expect_response(Status.SUCCESS)
    def ble_get_notifications(self, start_index: int = 0):
        """
        Fetch received notifications. Wire per entry: handle[2] | len[1] | data[len].

        :return: list of dicts {handle, data}
        """
        _require_ble_uint("start_index", start_index, 0xFFFF)
        data = struct.pack('!H', start_index)
        resp = self.device.send_cmd_sync(Command.BLE_GET_NOTIFICATIONS, data)
        if resp.status == Status.SUCCESS:
            offset = 0
            out = []
            while offset < len(resp.data):
                if len(resp.data) - offset < 3:
                    raise ValueError("malformed BLE notification response: truncated record header")
                handle, ln = struct.unpack_from('!HB', resp.data, offset)
                offset += 3
                if len(resp.data) - offset < ln:
                    raise ValueError("malformed BLE notification response: truncated value")
                out.append({'handle': handle, 'data': bytes(resp.data[offset:offset + ln])})
                offset += ln
            resp.parsed = out
        return resp

    @expect_response(Status.SUCCESS)
    def ble_fuzz_get_log(self, start_index: int = 0):
        """
        Fetch the fuzz log. Wire per entry:
        index[2] | payload_len[1] | write_status[1] | data[min(payload_len, 16)].
        """
        _require_ble_uint("start_index", start_index, 0xFFFF)
        data = struct.pack('!H', start_index)
        resp = self.device.send_cmd_sync(Command.BLE_FUZZ_GET_LOG, data)
        if resp.status == Status.SUCCESS:
            offset = 0
            entries = []
            while offset < len(resp.data):
                if len(resp.data) - offset < 4:
                    raise ValueError("malformed BLE fuzz-log response: truncated record header")
                index, plen, wstatus = struct.unpack_from('!HBB', resp.data, offset)
                offset += 4
                dlen = min(plen, 16)
                if len(resp.data) - offset < dlen:
                    raise ValueError("malformed BLE fuzz-log response: truncated payload")
                payload = resp.data[offset:offset + dlen]
                offset += dlen
                entries.append({'index': index, 'len': plen, 'status': wstatus, 'data': payload})
            resp.parsed = entries
        return resp

    def mf1_detect_support(self):
        """
        Detect whether it is mifare classic tag.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.MF1_DETECT_SUPPORT)
        return resp.status == Status.HF_TAG_OK

    @expect_response(Status.HF_TAG_OK)
    def mf1_detect_prng(self):
        """
        Detect mifare Class of classic nt vulnerabilities.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.MF1_DETECT_PRNG)
        if resp.status == Status.HF_TAG_OK:
            resp.parsed = resp.data[0]
        return resp

    @expect_response(Status.HF_TAG_OK)
    def mf1_detect_nt_dist(self, block_known, type_known, key_known):
        """
        Detect the random number distance of the card.

        :return:
        """
        data = struct.pack('!BB6s', type_known, block_known, key_known)
        resp = self.device.send_cmd_sync(Command.MF1_DETECT_NT_DIST, data)
        if resp.status == Status.HF_TAG_OK:
            uid, dist = struct.unpack('!II', resp.data)
            resp.parsed = {'uid': uid, 'dist': dist}
        return resp

    @expect_response(Status.HF_TAG_OK)
    def mf1_nested_acquire(self, block_known, type_known, key_known, block_target, type_target):
        """
        Collect the key NT parameters needed for Nested decryption
        :return:
        """
        data = struct.pack('!BB6sBB', type_known, block_known, key_known, type_target, block_target)
        resp = self.device.send_cmd_sync(Command.MF1_NESTED_ACQUIRE, data)
        if resp.status == Status.HF_TAG_OK:
            resp.parsed = [{'nt': nt, 'nt_enc': nt_enc, 'par': par}
                           for nt, nt_enc, par in struct.iter_unpack('!IIB', resp.data)]
        return resp

    @expect_response(Status.HF_TAG_OK)
    def mf1_darkside_acquire(self, block_target, type_target, first_recover: Union[int, bool], sync_max):
        """
        Collect the key parameters needed for Darkside decryption.

        :param block_target:
        :param type_target:
        :param first_recover:
        :param sync_max:
        :return:
        """
        data = struct.pack('!BBBB', type_target, block_target, first_recover, sync_max)
        resp = self.device.send_cmd_sync(Command.MF1_DARKSIDE_ACQUIRE, data, timeout=sync_max * 10)
        if resp.status == Status.HF_TAG_OK:
            if resp.data[0] == MifareClassicDarksideStatus.OK:
                darkside_status, uid, nt1, par, ks1, nr, ar = struct.unpack('!BIIQQII', resp.data)
                resp.parsed = (darkside_status, {'uid': uid, 'nt1': nt1, 'par': par, 'ks1': ks1, 'nr': nr, 'ar': ar})
            else:
                resp.parsed = (resp.data[0],)
        return resp

    @expect_response([Status.HF_TAG_OK, Status.MF_ERR_AUTH])
    def mf1_auth_one_key_block(self, block, type_value: MfcKeyType, key):
        """
        Verify the mf1 key, only verify the specified type of key for a single sector.

        :param block:
        :param type_value:
        :param key:
        :return:
        """
        data = struct.pack('!BB6s', type_value, block, key)
        resp = self.device.send_cmd_sync(Command.MF1_AUTH_ONE_KEY_BLOCK, data)
        resp.parsed = resp.status == Status.HF_TAG_OK
        return resp

    @expect_response(Status.HF_TAG_OK)
    def mf1_read_one_block(self, block, type_value: MfcKeyType, key):
        """
        Read one mf1 block.

        :param block:
        :param type_value:
        :param key:
        :return:
        """
        data = struct.pack('!BB6s', type_value, block, key)
        resp = self.device.send_cmd_sync(Command.MF1_READ_ONE_BLOCK, data)
        resp.parsed = resp.data
        return resp

    def mf1_read_blocks(self, block, count, type_value: MfcKeyType, key):
        """
        Authenticate once to a sector, then read `count` consecutive blocks from
        it (all in the sector `block` belongs to) — far fewer auths than
        read-one-block per block when dumping.

        :return: response; resp.parsed is a list of the 16-byte blocks actually
                 read (may be shorter than `count` if a read failed part-way).
        """
        data = struct.pack('!BBB6s', type_value, block, count, key)
        resp = self.device.send_cmd_sync(Command.MF1_READ_BLOCKS, data)
        if resp.status == Status.HF_TAG_OK:
            resp.parsed = [resp.data[i:i + 16] for i in range(0, len(resp.data), 16)]
        return resp

    @expect_response(Status.HF_TAG_OK)
    def mf1_write_one_block(self, block, type_value: MfcKeyType, key, block_data):
        """
        Write mf1 single block.

        :param block:
        :param type_value:
        :param key:
        :param block_data:
        :return:
        """
        data = struct.pack('!BB6s16s', type_value, block, key, block_data)
        resp = self.device.send_cmd_sync(Command.MF1_WRITE_ONE_BLOCK, data)
        resp.parsed = resp.status == Status.HF_TAG_OK
        return resp

    @expect_response(Status.HF_TAG_OK)
    def hf14a_scan_keep(self):
        """
        Scan ISO14443-A tag with full select + RATS, keeping field alive.

        Identical to hf14a_scan but does NOT tear down the RF field afterward.
        The card remains powered and in ISO14443-4 T=CL state so subsequent
        hf14a_raw calls can exchange APDUs without re-selecting.
        """
        resp = self.device.send_cmd_sync(Command.HF14A_SCAN_KEEP)
        if resp.status == Status.HF_TAG_OK:
            offset = 0
            data = []
            while offset < len(resp.data):
                uidlen, = struct.unpack_from('!B', resp.data, offset)
                offset += 1
                uid, atqa, sak, atslen = struct.unpack_from(
                    f'!{uidlen}s2s1sB', resp.data, offset)
                offset += struct.calcsize(f'!{uidlen}s2s1sB')
                ats, = struct.unpack_from(f'!{atslen}s', resp.data, offset)
                offset += atslen
                data.append({'uid': uid, 'atqa': atqa, 'sak': sak, 'ats': ats})
            resp.parsed = data
        return resp

    def hf14a_4_set_anti_coll(self, uid: bytes, atqa: bytes, sak: int, ats: bytes):
        """
        Set UID / ATQA / SAK / ATS for the active HF14A_4 slot.

        :param uid:  UID bytes (4 or 7 bytes)
        :param atqa: ATQA 2 bytes (wire order, e.g. b'\x04\x00' for ATQA 00 04)
        :param sak:  SAK byte value (int), use 0x20 for ISO14443-4
        :param ats:  ATS bytes (without CRC)
        """
        uid_size = len(uid)
        payload = (bytes([uid_size]) + bytes(uid) + bytes(atqa) +
                   bytes([sak]) + bytes([len(ats)]) + bytes(ats))
        return self.device.send_cmd_sync(Command.HF14A_4_SET_ANTI_COLL, payload)

    def hf14a_4_apdu_recv(self):
        """
        Non-blocking poll for a pending APDU from the ISO14443-4 T=CL stack.

        Returns immediately: STATUS_SUCCESS + APDU bytes if one is pending,
        STATUS_HF_TAG_NO if no APDU is waiting.  Call in a tight loop from
        the host side for relay/capture use cases.
        """
        return self.device.send_cmd_sync(Command.HF14A_4_APDU_RECV, b'', timeout=2)

    def hf14a_4_apdu_send(self, resp: bytes):
        """Send an APDU response to the ISO14443-4 T=CL stack."""
        payload = bytes([(len(resp) >> 8) & 0xFF, len(resp) & 0xFF]) + bytes(resp)
        return self.device.send_cmd_sync(Command.HF14A_4_APDU_SEND, payload)

    def hf14a_4_add_static_response(self, cmd: bytes, resp: bytes):
        """
        Add a static APDU command→response pair to the HF14A_4 slot.

        The firmware will automatically reply with resp whenever it receives
        an APDU whose first len(cmd) bytes match cmd, without USB involvement.
        Must be called before hw mode -e.
        """
        rlen = len(resp)
        payload = bytes([len(cmd)]) + bytes(cmd) + bytes([(rlen >> 8) & 0xFF, rlen & 0xFF]) + bytes(resp)
        return self.device.send_cmd_sync(Command.HF14A_4_STATIC_RESP, payload)

    def hf14a_4_reader_apdu(self, apdu: bytes):
        """
        Select card (with RATS) and send one ISO14443-4 T=CL APDU in a single
        firmware call — avoiding the USB round-trip gap that would depower the card.

        :param apdu: raw APDU bytes (no PCB wrapping needed)
        :return: response object with resp.data = APDU response bytes (no PCB/CRC)
        """
        return self.device.send_cmd_sync(
            Command.HF14A_4_READER_APDU, bytes(apdu), timeout=3)

    def _send_iso_dep_session_command(self, command, data, timeout):
        try:
            return self.device.send_cmd_sync(command, data, timeout=timeout)
        except TimeoutError:
            # No transaction ID exists, so reconnect before any same-ID retry.
            try:
                self.device.close()
            except Exception:
                pass
            raise

    def _hf14a_4_reader_session_start(self, command):
        resp = self._send_iso_dep_session_command(command, b'', 6)
        if resp.status == Status.HF_TAG_OK:
            resp.parsed = _parse_iso_dep_session_start(resp)
        return resp

    @expect_response(Status.HF_TAG_OK)
    def hf14a_4_reader_session_start(self):
        """Select one real ISO-DEP target and open a stateful APDU session."""
        return self._hf14a_4_reader_session_start(
            Command.HF14A_4_READER_SESSION_START)

    @expect_response(Status.HF_TAG_OK)
    def hf14a_4_reader_session_start_apple_transit(self):
        """Open a session using the Apple Transit polling annotation."""
        return self._hf14a_4_reader_session_start(
            Command.HF14A_4_READER_SESSION_START_APPLE_TRANSIT)

    @expect_response(Status.HF_TAG_OK)
    def hf14a_4_reader_session_exchange(self, session_id: int, apdu: bytes):
        """Exchange one raw APDU without reselecting or resetting the target."""
        _require_iso_dep_session_id(session_id)
        apdu = _require_iso_dep_apdu(apdu)
        resp = self._send_iso_dep_session_command(
            Command.HF14A_4_READER_SESSION_EXCHANGE,
            struct.pack("!I", session_id) + apdu,
            10)
        if resp.status == Status.HF_TAG_OK:
            if not 2 <= len(resp.data) <= 512:
                raise ValueError(
                    "malformed ISO-DEP session EXCHANGE response: "
                    f"expected 2..512 bytes, got {len(resp.data)}")
            resp.parsed = bytes(resp.data)
        return resp

    @expect_response(Status.SUCCESS)
    def hf14a_4_reader_session_stop(self, session_id: int):
        """Deselect and close the matching real-card ISO-DEP session."""
        _require_iso_dep_session_id(session_id)
        resp = self._send_iso_dep_session_command(
            Command.HF14A_4_READER_SESSION_STOP,
            struct.pack("!I", session_id), 6)
        if resp.status == Status.SUCCESS:
            if resp.data:
                raise ValueError(
                    "malformed ISO-DEP session STOP response: expected no data")
            resp.parsed = True
        return resp

    def hf14a_4_emv_scan(self, amount: bytes = b''):
        """
        Full EMV card scan in a single firmware call.

        The firmware performs the complete sequence (field cycle, select, RATS,
        PPSE, SELECT AID, GPO, READ RECORDs) without returning to the host
        between APDUs, avoiding the field-drop issue with separate calls.

        Response format:
            uid_len(1) uid(n) atqa(2) sak(1) ats_len(1) ats(m)
            num_apdus(1)
            for each APDU pair:
                cmd_len(1) cmd(n) resp_len_le(2) resp(m)
        """
        if amount and len(amount) != 6:
            raise ValueError("EMV amount must be 6-byte n12 BCD")
        resp = self.device.send_cmd_sync(Command.HF14A_4_EMV_SCAN, bytes(amount), timeout=12)
        return resp

    @expect_response([Status.HF_TAG_OK, Status.HF_TAG_NO])
    def hf14a_4_emv_trace_start(self, request: EmvTraceRequest):
        """Start a retained, versioned EMV trace session."""
        payload = encode_start_request(request)
        timeout = max(12, ((request.budget_ms or 12000) + 999) // 1000 + 5)
        resp = self.device.send_cmd_sync(
            Command.HF14A_4_EMV_TRACE_START, payload, timeout=timeout)
        if resp.status in (Status.HF_TAG_OK, Status.HF_TAG_NO):
            resp.parsed = parse_start_response(resp.data)
        return resp

    @expect_response(Status.SUCCESS)
    def hf14a_4_emv_trace_meta(self, scan_id: int):
        """Read and validate metadata for a retained EMV trace session."""
        resp = self.device.send_cmd_sync(
            Command.HF14A_4_EMV_TRACE_META, encode_meta_request(scan_id))
        if resp.status == Status.SUCCESS:
            resp.parsed = parse_meta_response(resp.data, scan_id)
        return resp

    @expect_response(Status.SUCCESS)
    def hf14a_4_emv_trace_get(self, scan_id: int, start_record: int,
                             max_payload: int = 4096):
        """Read and validate one atomic-record page from a retained trace."""
        resp = self.device.send_cmd_sync(
            Command.HF14A_4_EMV_TRACE_GET,
            encode_get_request(scan_id, start_record, max_payload))
        if resp.status == Status.SUCCESS:
            resp.parsed = parse_get_response(resp.data, scan_id, start_record)
        return resp

    def hf14a_4_emv_trace_download(self, request: EmvTraceRequest,
                                   max_payload: int = 4096):
        """Start, page, and integrity-check a complete retained EMV trace."""
        start = self.hf14a_4_emv_trace_start(request)
        return download_emv_trace(
            start.scan_id,
            self.hf14a_4_emv_trace_meta,
            self.hf14a_4_emv_trace_get,
            max_payload=max_payload,
            start=start,
        )

    def hf14a_4_desfire_scan(self):
        """
        Full DESFire enumeration in a single firmware call. Same packed response
        layout as hf14a_4_emv_scan: tag info + num_apdus + (cmd, resp) pairs
        (GetVersion, GetApplicationIDs, per-app SelectApplication + GetFileIDs).
        """
        return self.device.send_cmd_sync(Command.HF14A_4_DESFIRE_SCAN, b'', timeout=10)

    def hf14a_set_field_on(self):
        """Reset the HF reader and turn the antenna field on (reader mode)."""
        return self.device.send_cmd_sync(Command.HF14A_SET_FIELD_ON)

    def hf14a_set_field_off(self):
        """Turn the HF reader antenna field off (reader mode)."""
        return self.device.send_cmd_sync(Command.HF14A_SET_FIELD_OFF)

    def hf14a_4_clear_static_responses(self):
        """Clear all static APDU responses from the active HF14A_4 slot."""
        return self.device.send_cmd_sync(Command.HF14A_4_STATIC_RESP, b'\x00')

    def hf14a_raw(self, options, resp_timeout_ms=100, data=[], bitlen=None):
        """
        Send raw cmd to 14a tag.

        :param options:
        :param resp_timeout_ms:
        :param data:
        :param bit_owned_by_the_last_byte:
        :return:
        """

        class CStruct(ctypes.BigEndianStructure):
            _fields_ = [
                ("activate_rf_field", ctypes.c_uint8, 1),
                ("wait_response", ctypes.c_uint8, 1),
                ("append_crc", ctypes.c_uint8, 1),
                ("auto_select", ctypes.c_uint8, 1),
                ("keep_rf_field", ctypes.c_uint8, 1),
                ("check_response_crc", ctypes.c_uint8, 1),
                ("reserved", ctypes.c_uint8, 2),
            ]

        cs = CStruct()
        cs.activate_rf_field = options['activate_rf_field']
        cs.wait_response = options['wait_response']
        cs.append_crc = options['append_crc']
        cs.auto_select = options['auto_select']
        cs.keep_rf_field = options['keep_rf_field']
        cs.check_response_crc = options['check_response_crc']

        if bitlen is None:
            bitlen = len(data) * 8  # bits = bytes * 8(bit)
        else:
            if len(data) == 0:
                raise ValueError(f'bitlen={bitlen} but missing data')
            if not ((len(data) - 1) * 8 < bitlen <= len(data) * 8):
                raise ValueError(f'bitlen={bitlen} incompatible with provided data ({len(data)} bytes), '
                                 f'must be between {((len(data) - 1) * 8)+1} and {len(data) * 8} included')

        data = bytes(cs)+struct.pack(f'!HH{len(data)}s', resp_timeout_ms, bitlen, bytearray(data))
        resp = self.device.send_cmd_sync(Command.HF14A_RAW, data, timeout=(resp_timeout_ms // 1000) + 1)
        return resp.data

    @expect_response(Status.HF_TAG_OK)
    def mf1_manipulate_value_block(self, src_block, src_type: MfcKeyType, src_key, operator: MfcValueBlockOperator, operand, dst_block, dst_type: MfcKeyType, dst_key):
        """
        1. Increment: increments value from source block and write to dest block
        2. Decrement: decrements value from source block and write to dest block
        3. Restore: copy value from source block and write to dest block


        :param src_block:
        :param src_type:
        :param src_key:
        :param operator:
        :param operand:
        :param dst_block:
        :param dst_type:
        :param dst_key:
        :return:
        """
        data = struct.pack('!BB6sBiBB6s', src_type, src_block, src_key, operator, operand, dst_type, dst_block, dst_key)
        resp = self.device.send_cmd_sync(Command.MF1_MANIPULATE_VALUE_BLOCK, data)
        resp.parsed = resp.status == Status.HF_TAG_OK
        return resp

    @expect_response([Status.HF_TAG_OK, Status.HF_TAG_NO])
    def mf1_check_keys_of_sectors(self, mask: bytes, keys: list[bytes]):
        """
        Check keys of sectors.
        :return:
        """
        if len(mask) != 10:
            raise ValueError("len(mask) should be 10")
        if len(keys) < 1 or len(keys) > 83:
            raise ValueError("Invalid len(keys)")
        data = struct.pack(f'!10s{6*len(keys)}s', mask, b''.join(keys))

        bitsCnt = 80  # maximum sectorKey_to_be_checked
        for b in mask:
            while b > 0:
                [bitsCnt, b] = [bitsCnt - (b & 0b1), b >> 1]
        if bitsCnt < 1:
            # All sectorKey is masked
            return chameleon_com.Response(
                cmd=Command.MF1_CHECK_KEYS_OF_SECTORS,
                status=Status.HF_TAG_OK,
                parsed={'status': Status.HF_TAG_OK},
            )
        # base timeout: 1s
        # auth: len(keys) * sectorKey_to_be_checked * 0.1s
        # read keyB from trailer block: 0.1s
        timeout = 1 + (bitsCnt + 1) * len(keys) * 0.1

        resp = self.device.send_cmd_sync(Command.MF1_CHECK_KEYS_OF_SECTORS, data, timeout=timeout)
        resp.parsed = {'status': resp.status}
        if len(resp.data) == 490:
            found = ''.join([format(i, '08b') for i in resp.data[0:10]])
            # print(f'{found = }')
            resp.parsed.update({
                'found': resp.data[0:10],
                'sectorKeys': {k: resp.data[6 * k + 10:6 * k + 16] for k, v in enumerate(found) if v == '1'}
            })
        return resp

    @expect_response([Status.HF_TAG_OK, Status.HF_TAG_NO, Status.MF_ERR_AUTH])
    def mf1_check_keys_on_block(self, block: int, key_type: int, keys: list[bytes]):
        if key_type not in [0x60, 0x61]:
            raise ValueError("Wrong key type")
        if len(keys) < 1 or len(keys) > 83:
            raise ValueError("Invalid len(keys)")
        data = struct.pack(f'!BBB{6*len(keys)}s', block, key_type, len(keys), b''.join(keys))

        resp = self.device.send_cmd_sync(Command.MF1_CHECK_KEYS_ON_BLOCK, data, timeout=10)

        if resp.status == Status.HF_TAG_OK and len(resp.data) == 7:
            found, key = struct.unpack('!B6s', resp.data)
            if found:
                resp.parsed = key

        return resp

    @expect_response(Status.HF_TAG_OK)
    def mf1_static_nested_acquire(self, block_known, type_known, key_known, block_target, type_target):
        """
        Collect the key NT parameters needed for StaticNested decryption
        :return:
        """
        data = struct.pack('!BB6sBB', type_known, block_known, key_known, type_target, block_target)
        resp = self.device.send_cmd_sync(Command.MF1_STATIC_NESTED_ACQUIRE, data)
        if resp.status == Status.HF_TAG_OK:
            resp.parsed = {
                'uid': struct.unpack('!I', resp.data[0:4])[0],
                'nts': [
                    {
                        'nt': nt,
                        'nt_enc': nt_enc
                    } for nt, nt_enc in struct.iter_unpack('!II', resp.data[4:])
                ]
            }
        return resp

    @expect_response(Status.HF_TAG_OK)
    def mf1_hard_nested_acquire(self, slow, block_known, type_known, key_known, block_target, type_target):
        """
        Collect the NT_ENC list for HardNested decryption
        :return:
        """
        data = struct.pack('!BBB6sBB', slow, type_known, block_known, key_known, type_target, block_target)
        resp = self.device.send_cmd_sync(Command.MF1_HARDNESTED_ACQUIRE, data, timeout=30)
        if resp.status == Status.HF_TAG_OK:
            resp.parsed = resp.data  # we can return the raw nonces bytes
        return resp

    @expect_response([Status.HF_TAG_OK, Status.HF_TAG_NO])
    def mf1_static_encrypted_nested_acquire(self, backdoor_key, sector_count, starting_sector):
        data = struct.pack('!6sBB', backdoor_key, sector_count, starting_sector)
        resp = self.device.send_cmd_sync(Command.MF1_ENC_NESTED_ACQUIRE, data, timeout=30)
        if resp.status == Status.HF_TAG_OK:
            resp.parsed = {
                'uid': struct.unpack('!I', resp.data[0:4])[0],
                'nts': {
                    'a': [],
                    'b': []
                }
            }

            i = 4

            while i < len(resp.data):
                resp.parsed['nts']['a'].append(
                    {
                        'nt': reconstruct_full_nt(resp.data, i),
                        'nt_enc': int.from_bytes(resp.data[i + 3: i + 7], byteorder='big'),
                        'parity': parity_to_str(resp.data[i + 2])
                    }
                )

                resp.parsed['nts']['b'].append(
                    {
                        'nt': reconstruct_full_nt(resp.data, i + 7),
                        'nt_enc': int.from_bytes(resp.data[i + 10: i + 14], byteorder='big'),
                        'parity': parity_to_str(resp.data[i + 9])
                    }
                )

                i += 14
        return resp

    def hf14a_sniff(self, timeout_ms: int = 5000):
        """
        Capture ISO14443A reader frames while CU acts as a tag emulator.

        The firmware installs a sniff callback into the HF14A stack for the
        requested duration, then returns all captured frames packed as:
          [2 bytes: bit count, big-endian] [N bytes: frame data, ceil(bits/8)] ...

        :param timeout_ms: Listen duration in ms (1-30000, default 5000)
        :return: Raw response — check .status and .data
        """
        timeout_ms = max(1, min(30000, timeout_ms))
        payload = bytes([(timeout_ms >> 8) & 0xFF, timeout_ms & 0xFF])
        timeout_s = (timeout_ms // 1000) + 5
        return self.device.send_cmd_sync(Command.HF14A_SNIFF, payload, timeout=timeout_s)

    def hf_capture_start(self, mode: int = 0, start_token: int = None):
        """Start a continuous HF capture session.

        mode: 0=tag emulation, 1=passive reader-command monitor, 2=active reader trace.
        """
        if mode not in (0, 1, 2):
            raise ValueError("mode must be 0 (emulation), 1 (passive), or 2 (reader)")
        if start_token is None:
            raise ValueError("start_token is required so a timed-out START can be recovered")
        if start_token <= 0 or start_token > 0xFFFFFFFF:
            raise ValueError("start_token must fit in a non-zero unsigned 32-bit integer")
        return self.device.send_cmd_sync(
            Command.HF_CAPTURE_START,
            bytes([2, mode]) + start_token.to_bytes(4, "big"),
        )

    def hf_capture_status(self, session_id: int, start_token: int):
        """Read metadata and rebind a retained capture to this transport."""
        if session_id < 0 or session_id > 0xFFFFFFFF:
            raise ValueError("session_id must fit in an unsigned 32-bit integer")
        if start_token <= 0 or start_token > 0xFFFFFFFF:
            raise ValueError("start_token must fit in a non-zero unsigned 32-bit integer")
        payload = (bytes([2]) + session_id.to_bytes(4, "big") +
                   start_token.to_bytes(4, "big"))
        return self.device.send_cmd_sync(Command.HF_CAPTURE_STATUS, payload)

    def hf_capture_get(self, session_id: int, ack_sequence=None,
                       ack_delivery_token=None,
                       requested_bytes: int = 4096):
        """Get an idempotent capture page and acknowledge a persisted page."""
        if session_id <= 0 or session_id > 0xFFFFFFFF:
            raise ValueError("session_id must fit in an unsigned 32-bit integer")
        if (ack_sequence is None) != (ack_delivery_token is None):
            raise ValueError("ack_sequence and ack_delivery_token must be supplied together")
        ack = 0 if ack_sequence is None else ack_sequence
        if ack < 0 or ack > 0xFFFFFFFF:
            raise ValueError("ack_sequence must fit in an unsigned 32-bit integer")
        delivery_token = 0 if ack_delivery_token is None else ack_delivery_token
        if ack_delivery_token is not None and not 1 <= delivery_token <= 0x7FFFFFFFFFFFFFFF:
            raise ValueError("ack_delivery_token must fit in a positive signed 64-bit integer")
        requested_bytes = max(605, min(4096, int(requested_bytes)))
        payload = (bytes([2]) + session_id.to_bytes(4, "big") +
                    bytes([ack_sequence is not None]) + ack.to_bytes(4, "big") +
                    delivery_token.to_bytes(8, "big") +
                    requested_bytes.to_bytes(2, "big"))
        return self.device.send_cmd_sync(Command.HF_CAPTURE_GET, payload)

    def hf_capture_stop(self, session_id: int):
        """Stop RF acquisition while retaining unread capture records."""
        if session_id <= 0 or session_id > 0xFFFFFFFF:
            raise ValueError("session_id must fit in an unsigned 32-bit integer")
        payload = bytes([2]) + session_id.to_bytes(4, "big")
        return self.device.send_cmd_sync(Command.HF_CAPTURE_STOP, payload)

    def hf14a_auth_trace(self, block: int, key_type: int, key: bytes, timeout_ms: int = 5000):
        """
        Run a full reader-side ISO14443A + MIFARE Classic Crypto1 auth flow
        against a real card and return every wire frame for inspection.

        The firmware polls for a tag in the field for up to `timeout_ms`
        milliseconds, then performs anticoll + SELECT + (optional RATS) +
        AUTH and packs all frames — synthesized anticoll plus the live
        AUTH/NT/NR||AR/AT — into the same buffer format used by hf14a_sniff:
          [2 bytes: bit count, big-endian] [N bytes: frame data, ceil(bits/8)] ...
        Bit 15 of the bit-count header: 0 = reader→card, 1 = card→reader.

        :param block:      target block number (0-255)
        :param key_type:   0x60 (Key A) or 0x61 (Key B)
        :param key:        6-byte sector key
        :param timeout_ms: tag-presence polling timeout in ms (1-30000)
        :return: Raw response — check .status and .data
        """
        if key_type not in (0x60, 0x61):
            raise ValueError("key_type must be 0x60 (Key A) or 0x61 (Key B)")
        if len(key) != 6:
            raise ValueError("key must be exactly 6 bytes")
        timeout_ms = max(1, min(30000, int(timeout_ms)))
        payload = (
            bytes([key_type, block & 0xFF])
            + bytes(key)
            + bytes([(timeout_ms >> 8) & 0xFF, timeout_ms & 0xFF])
        )
        # Add a couple of seconds of slack on top of the device-side polling
        # window so the USB/BLE round-trip doesn't time out before firmware
        # gives up on its own.
        return self.device.send_cmd_sync(Command.HF14A_AUTH_TRACE, payload, timeout=(timeout_ms // 1000) + 3)

    @expect_response(Status.SUCCESS)
    def hf14a_get_config(self):
        """
        Get hf 14a config

        :return:
        """
        resp = self.device.send_cmd_sync(Command.HF14A_GET_CONFIG)
        if resp.status == Status.SUCCESS:
            bcc, cl2, cl3, rats = struct.unpack('!bbbb', resp.data)
            resp.parsed = {'bcc': bcc,
                           'cl2': cl2,
                           'cl3': cl3,
                           'rats': rats}
        return resp

    @expect_response(Status.SUCCESS)
    def hf14a_set_config(self, data):
        """
        Set hf 14a config

        :return:
        """
        data = struct.pack('!bbbb', data['bcc'], data['cl2'], data['cl3'], data['rats'])
        return self.device.send_cmd_sync(Command.HF14A_SET_CONFIG, data)

    @expect_response(Status.LF_TAG_OK)
    def em410x_scan(self):
        """
        Read the card number of EM410X.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.EM410X_SCAN)
        if resp.status == Status.LF_TAG_OK:
            tag_type = struct.unpack('!H', resp.data[:2])[0]
            if tag_type == TagSpecificType.EM410X_ELECTRA:
                fmt = '!H13s'
            else:
                fmt = '!H5s'
            resp.parsed = struct.unpack(fmt, resp.data[:struct.calcsize(fmt)])  # tag type + uid
        return resp

    @expect_response(Status.LF_TAG_OK)
    def em410x_write_to_t55xx(self, id_bytes: bytes):
        """
        Write EM410X card number into T55XX.

        :param id_bytes: ID card number
        :return:
        """
        if len(id_bytes) == 5:
            data = struct.pack(f'!5s4s{4*len(old_keys)}s', id_bytes, new_key, b''.join(old_keys))
            return self.device.send_cmd_sync(Command.EM410X_WRITE_TO_T55XX, data)
        if len(id_bytes) == 13:
            data = struct.pack(f'!13s4s{4*len(old_keys)}s', id_bytes, new_key, b''.join(old_keys))
            return self.device.send_cmd_sync(Command.EM410X_ELECTRA_WRITE_TO_T55XX, data)
        raise ValueError("The id bytes length must equal 5 (EM410X) or 13 (Electra)")

    @expect_response(Status.LF_TAG_OK)
    def hidprox_scan(self, format: int):
        """
        Read the length, facility code and card number of HID Prox.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.HIDPROX_SCAN, struct.pack('!B', format))
        if resp.status == Status.LF_TAG_OK:
            resp.parsed = struct.unpack('>BIBIBH', resp.data[:13])
        return resp

    @expect_response(Status.LF_TAG_OK)
    def hidprox_write_to_t55xx(self, id_bytes: bytes):
        """
        Write HID Prox card number into T55XX.

        :param id_bytes: ID card number
        :return:
        """
        if len(id_bytes) != 13:
            raise ValueError("The id bytes length must equal 13")
        data = struct.pack(f'!13s4s{4*len(old_keys)}s', id_bytes, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.HIDPROX_WRITE_TO_T55XX, data)

    @expect_response(Status.LF_TAG_OK)
    def ioprox_scan(self):
        """
        Read ioProx (XSF): version, facility, number, raw.
        """
        resp = self.device.send_cmd_sync(Command.IOPROX_SCAN)
        if resp.status == Status.LF_TAG_OK:
            resp.parsed = struct.unpack(">BBH8sBBBB", resp.data[:16])
        return resp

    @expect_response(Status.LF_TAG_OK)
    def ioprox_write_to_t55xx(self, id_bytes: bytes):
        """
        Write ioProx card data to a T55XX tag.
        """
        if len(id_bytes) != 16:
            raise ValueError("The ioProx id bytes length must equal 16")

        # Pack id_bytes (16), new_key (4), and all old_keys (4 each) into one buffer
        fmt = f'!16s4s{4 * len(old_keys)}s'
        data = struct.pack(fmt, id_bytes, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.IOPROX_WRITE_TO_T55XX, data)

    @expect_response(Status.SUCCESS)
    def ioprox_decode_raw(self, raw8_bytes):
        """
        Send 8 raw card bytes to firmware and return 16-byte card data structure.
        Response layout: [0]=ver, [1]=fc, [2..3]=cn, [4..11]=raw8, [12..15]=padding.
        """
        resp = self.device.send_cmd_sync(Command.IOPROX_DECODE_RAW, data=raw8_bytes)
        if resp.status == Status.SUCCESS:
            resp.parsed = struct.unpack(">BBH8sBBBB", resp.data[:16])
        return resp

    @expect_response(Status.SUCCESS)
    def ioprox_compose_id(self, ver, fc, cn):
        """
        Encode ioProx parameters into a 16-byte card data structure via firmware.
        Response layout: [0]=ver, [1]=fc, [2..3]=cn, [4..11]=raw8, [12..15]=padding.
        """
        payload = struct.pack(">BBH", ver, fc, cn)
        resp = self.device.send_cmd_sync(Command.IOPROX_COMPOSE_ID, data=payload)
        if resp.status == Status.SUCCESS:
            resp.parsed = struct.unpack(">BBH8sBBBB", resp.data[:16])
        return resp

    def lf_sniff(self, timeout_ms: int = 2000):
        """
        Capture raw LF field ADC samples.

        The ChameleonUltra samples the LF antenna at 125kHz (8µs/sample).
        Each byte is an 8-bit ADC value: ~0x80 = field on, lower = gap/no field.

        :param timeout_ms: Capture duration in ms (1-10000, default 2000)
        :return: Raw response object — check .status and .data
        """
        timeout_ms = max(1, min(10000, timeout_ms))
        payload = bytes([(timeout_ms >> 8) & 0xFF, timeout_ms & 0xFF])
        timeout_s = (timeout_ms // 1000) + 2
        return self.device.send_cmd_sync(Command.LF_SNIFF, payload, timeout=timeout_s)

    @expect_response(Status.LF_TAG_OK)
    def em4x05_scan(self, pwd: int = 0):
        """
        Read an EM4x05 or EM4x69 tag (reader-talk-first).

        Response payload (14 bytes, big-endian):
          config    4 bytes  — block 0 configuration word
          uid       4 bytes  — EM4x05 UID
          uid_hi    4 bytes  — EM4x69 uid_hi (zero for plain EM4x05)
          is_em4x69 1 byte   — 1 if a 64-bit EM4x69 UID was read
          uid_block 1 byte   — block number UID was read from

        :param pwd: 32-bit password for LOGIN (default 0x00000000)
        :return: parsed tuple (config, uid, uid_hi, is_em4x69, uid_block)
        """
        pwd_bytes = struct.pack('!I', pwd & 0xFFFFFFFF)
        resp = self.device.send_cmd_sync(Command.EM4X05_SCAN, pwd_bytes)
        if resp.status == Status.LF_TAG_OK:
            resp.parsed = struct.unpack('!IIIBB', resp.data[:14])
        return resp

    @expect_response(Status.LF_TAG_OK)
    def viking_scan(self):
        """
        Read the card number of Viking.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.VIKING_SCAN)
        if resp.status == Status.LF_TAG_OK:
            resp.parsed = resp.data  # uid
        return resp

    @expect_response(Status.LF_TAG_OK)
    def viking_write_to_t55xx(self, id_bytes: bytes):
        """
        Write Viking card number into T55XX.

        :param id_bytes: ID card number
        :return:
        """
        if len(id_bytes) != 4:
            raise ValueError("The id bytes length must equal 4")
        data = struct.pack(f'!4s4s{4*len(old_keys)}s', id_bytes, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.VIKING_WRITE_TO_T55XX, data)

    @expect_response(Status.LF_TAG_OK)
    def pac_scan(self):
        """
        Read the card ID of PAC/Stanley.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.PAC_SCAN)
        if resp.status == Status.LF_TAG_OK:
            resp.parsed = resp.data[:8]
        return resp

    @expect_response(Status.LF_TAG_OK)
    def pac_write_to_t55xx(self, id_bytes: bytes):
        """
        Write PAC/Stanley card data to a T55XX tag.

        :param id_bytes: 8-byte ASCII card ID
        :return:
        """
        if len(id_bytes) != 8:
            raise ValueError("The id bytes length must equal 8")
        data = struct.pack(f'!8s4s{4*len(old_keys)}s', id_bytes, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.PAC_WRITE_TO_T55XX, data)

    @expect_response(Status.LF_TAG_OK)
    def jablotron_scan(self):
        """
        Read the card number of Jablotron.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.JABLOTRON_SCAN)
        if resp.status == Status.LF_TAG_OK:
            resp.parsed = resp.data[:5]
        return resp

    @expect_response(Status.LF_TAG_OK)
    def jablotron_write_to_t55xx(self, id_bytes: bytes):
        """
        Write Jablotron card number into T55XX.

        :param id_bytes: 5-byte Jablotron card ID
        :return:
        """
        if len(id_bytes) != 5:
            raise ValueError("The id bytes length must equal 5")
        data = struct.pack(f'!5s4s{4*len(old_keys)}s', id_bytes, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.JABLOTRON_WRITE_TO_T55XX, data)

    @expect_response(Status.LF_TAG_OK)
    def idteck_write_to_t55xx(self, id_bytes: bytes):
        """
        Write an IDTECK 64-bit PSK1 frame onto a T55xx tag.

        :param id_bytes: 8 bytes = full 64-bit frame (preamble 4 bytes + data 4 bytes)
        :return:
        """
        if len(id_bytes) != 8:
            raise ValueError("The id bytes length must equal 8")
        data = struct.pack(f'!8s4s{4*len(old_keys)}s', id_bytes, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.IDTECK_WRITE_TO_T55XX, data)

    @expect_response(Status.LF_TAG_OK)
    def adc_generic_read(self):
        """
        Read the ADC when the field is on.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.ADC_GENERIC_READ, None)
        if resp.status == Status.LF_TAG_OK:
            resp.parsed = resp.data
        return resp

    @expect_response(Status.SUCCESS)
    def get_slot_info(self):
        """
        Get slots info.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.GET_SLOT_INFO)
        if resp.status == Status.SUCCESS:
            resp.parsed = [{'hf': hf, 'lf': lf}
                           for hf, lf in struct.iter_unpack('!HH', resp.data)]
        return resp

    @expect_response(Status.SUCCESS)
    def get_active_slot(self):
        """
        Get selected slot.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.GET_ACTIVE_SLOT)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[0]
        return resp

    @expect_response(Status.SUCCESS)
    def set_active_slot(self, slot_index: SlotNumber):
        """
        Set the card slot currently active for use.

        :param slot_index: Card slot index
        :return:
        """
        # SlotNumber() will raise error for us if slot_index not in slot range
        data = struct.pack('!B', SlotNumber.to_fw(slot_index))
        return self.device.send_cmd_sync(Command.SET_ACTIVE_SLOT, data)

    @expect_response(Status.SUCCESS)
    def set_slot_tag_type(self, slot_index: SlotNumber, tag_type: TagSpecificType):
        """
        Set the label type of the emulated card of the current card slot
        Note: This operation will not change the data in the flash,
        and the change of the data in the flash will only be updated at the next save.

        :param slot_index:  Card slot number
        :param tag_type:  label type
        :return:
        """
        # SlotNumber() will raise error for us if slot_index not in slot range
        data = struct.pack('!BH', SlotNumber.to_fw(slot_index), tag_type)
        return self.device.send_cmd_sync(Command.SET_SLOT_TAG_TYPE, data)

    @expect_response(Status.SUCCESS)
    def delete_slot_sense_type(self, slot_index: SlotNumber, sense_type: TagSenseType):
        """
        Delete a sense type for a specific slot.

        :param slot_index: Slot index
        :param sense_type: Sense type to disable
        :return:
        """
        data = struct.pack('!BB', SlotNumber.to_fw(slot_index), sense_type)
        return self.device.send_cmd_sync(Command.DELETE_SLOT_SENSE_TYPE, data)

    @expect_response(Status.SUCCESS)
    def set_slot_data_default(self, slot_index: SlotNumber, tag_type: TagSpecificType):
        """
        Set the data of the emulated card in the specified card slot as the default data
        Note: This API will set the data in the flash together.

        :param slot_index: Card slot number
        :param tag_type:  The default label type to set
        :return:
        """
        # SlotNumber() will raise error for us if slot_index not in slot range
        data = struct.pack('!BH', SlotNumber.to_fw(slot_index), tag_type)
        return self.device.send_cmd_sync(Command.SET_SLOT_DATA_DEFAULT, data)

    @expect_response(Status.SUCCESS)
    def set_slot_enable(self, slot_index: SlotNumber, sense_type: TagSenseType, enabled: bool):
        """
        Set whether the specified card slot is enabled.

        :param slot_index: Card slot number
        :param enable: Whether to enable
        :return:
        """
        # SlotNumber() will raise error for us if slot_index not in slot range
        data = struct.pack('!BBB', SlotNumber.to_fw(slot_index), sense_type, enabled)
        return self.device.send_cmd_sync(Command.SET_SLOT_ENABLE, data)

    def _get_active_lf_tag_type(self) -> TagSpecificType:
        slotinfo = self.get_slot_info()
        active_slot = SlotNumber.from_fw(self.get_active_slot())
        lf_tag_value = slotinfo[active_slot - 1]['lf']
        return TagSpecificType(lf_tag_value)

    @expect_response(Status.SUCCESS)
    def em410x_set_emu_id(self, id: bytes):
        """
        Set the card number emulated by EM410x.

        :param id_bytes: byte of the card number
        :return:
        """
        lf_tag_type = self._get_active_lf_tag_type()
        if lf_tag_type == TagSpecificType.EM410X_ELECTRA:
            expected_len = 13
        elif lf_tag_type == TagSpecificType.EM410X:
            expected_len = 5
        else:
            raise ValueError(f"Active LF slot type {lf_tag_type} is not EM410X")

        if len(id) != expected_len:
            raise ValueError(f"The id bytes length must equal {expected_len}")

        data = struct.pack(f'!{expected_len}s', id)
        return self.device.send_cmd_sync(Command.EM410X_SET_EMU_ID, data)

    @expect_response(Status.SUCCESS)
    def em410x_get_emu_id(self):
        """
        Get the emulated EM410x card id
        """
        resp = self.device.send_cmd_sync(Command.EM410X_GET_EMU_ID)
        if resp.status == Status.SUCCESS:
            data = resp.data
            id_bytes = data
            tag_type = None

            if len(data) >= 2:
                try:
                    candidate = TagSpecificType(int.from_bytes(data[:2], byteorder='big'))
                except ValueError:
                    candidate = None

                if candidate in (TagSpecificType.EM410X, TagSpecificType.EM410X_ELECTRA):
                    expected_len = 13 if candidate == TagSpecificType.EM410X_ELECTRA else 5
                    if len(data) == expected_len + 2:
                        tag_type = candidate
                        id_bytes = data[2:2 + expected_len]

            if tag_type is None:
                lf_tag_type = self._get_active_lf_tag_type()
                if lf_tag_type == TagSpecificType.EM410X_ELECTRA:
                    expected_len = 13
                elif lf_tag_type == TagSpecificType.EM410X:
                    expected_len = 5
                else:
                    expected_len = len(data)
                id_bytes = data[:expected_len]

            resp.parsed = id_bytes
        return resp

    @expect_response(Status.SUCCESS)
    def hidprox_set_emu_id(self, id: bytes):
        """
        Set the card number emulated by HID Prox.

        :param id_bytes: byte of the card number
        :return:
        """
        if len(id) != 13:
            raise ValueError("The id bytes length must equal 13")
        return self.device.send_cmd_sync(Command.HIDPROX_SET_EMU_ID, id)

    @expect_response(Status.SUCCESS)
    def hidprox_get_emu_id(self):
        """
        Get the emulated HID Prox card id
        """
        resp = self.device.send_cmd_sync(Command.HIDPROX_GET_EMU_ID)
        if resp.status == Status.SUCCESS:
            resp.parsed = struct.unpack('>BIBIBH', resp.data[:13])
        return resp

    @expect_response(Status.SUCCESS)
    def ioprox_set_emu_id(self, id: bytes):
        """
        Set the card number emulated by ioProx.

        :param id_bytes: byte of the card number
        :return:
        """
        if len(id) != 16:
            raise ValueError("The id bytes length must equal 16")
        return self.device.send_cmd_sync(Command.IOPROX_SET_EMU_ID, id)

    @expect_response(Status.SUCCESS)
    def ioprox_get_emu_id(self):
        """
        Get the emulated ioProx card id
        """
        resp = self.device.send_cmd_sync(Command.IOPROX_GET_EMU_ID)
        if resp.status == Status.SUCCESS:
            resp.parsed = struct.unpack(">BBH8sBBBB", resp.data[:16])
        return resp

    @expect_response(Status.SUCCESS)
    def idteck_set_emu_id(self, id: bytes):
        """
        Set the 64-bit IDTECK frame emulated on the active slot.

        :param id: 8 bytes (preamble + card data, big-endian)
        """
        if len(id) != 8:
            raise ValueError("The id bytes length must equal 8")
        return self.device.send_cmd_sync(Command.IDTECK_SET_EMU_ID, id)

    @expect_response(Status.SUCCESS)
    def idteck_get_emu_id(self):
        """
        Get the emulated IDTECK 64-bit frame.
        """
        resp = self.device.send_cmd_sync(Command.IDTECK_GET_EMU_ID)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[:8]
        return resp

    @expect_response(Status.SUCCESS)
    def viking_set_emu_id(self, id: bytes):
        """
        Set the card number emulated by Viking.

        :param id_bytes: byte of the card number
        :return:
        """
        if len(id) != 4:
            raise ValueError("The id bytes length must equal 4")
        data = struct.pack('4s', id)
        return self.device.send_cmd_sync(Command.VIKING_SET_EMU_ID, data)

    @expect_response(Status.SUCCESS)
    def viking_get_emu_id(self):
        """
        Get the emulated Viking card id
        """
        resp = self.device.send_cmd_sync(Command.VIKING_GET_EMU_ID)
        resp.parsed = resp.data
        return resp

    @expect_response(Status.SUCCESS)
    def pac_set_emu_id(self, id: bytes):
        """
        Set the card ID emulated by PAC/Stanley.

        :param id: 8-byte ASCII card ID
        :return:
        """
        if len(id) != 8:
            raise ValueError("The id bytes length must equal 8")
        data = struct.pack('8s', id)
        return self.device.send_cmd_sync(Command.PAC_SET_EMU_ID, data)

    @expect_response(Status.SUCCESS)
    def pac_get_emu_id(self):
        """
        Get the emulated PAC/Stanley card ID
        """
        resp = self.device.send_cmd_sync(Command.PAC_GET_EMU_ID)
        resp.parsed = resp.data[:8]
        return resp

    @expect_response(Status.SUCCESS)
    def jablotron_set_emu_id(self, id: bytes):
        """
        Set the card number emulated by Jablotron.

        :param id: 5-byte Jablotron card ID
        :return:
        """
        if len(id) != 5:
            raise ValueError("The id bytes length must equal 5")
        data = struct.pack('5s', id)
        return self.device.send_cmd_sync(Command.JABLOTRON_SET_EMU_ID, data)

    @expect_response(Status.SUCCESS)
    def jablotron_get_emu_id(self):
        """
        Get the emulated Jablotron card id
        """
        resp = self.device.send_cmd_sync(Command.JABLOTRON_GET_EMU_ID)
        resp.parsed = resp.data[:5]
        return resp

    @expect_response(Status.SUCCESS)
    def mf1_set_detection_enable(self, enabled: bool):
        """
        Set whether to enable the detection of the current card slot.

        :param enable: Whether to enable
        :return:
        """
        data = struct.pack('!B', enabled)
        return self.device.send_cmd_sync(Command.MF1_SET_DETECTION_ENABLE, data)

    @expect_response(Status.SUCCESS)
    def mf1_set_random_uid_mode(self, enabled: bool):
        """
        Set whether the emulator presents a new random UID on each reader activation.

        Note: random UID fragments MFKey32 recovery (which needs two auths sharing
        the same UID); use a fixed UID when maximizing key capture.

        :param enabled: Whether to enable random-UID mode
        :return:
        """
        data = struct.pack('!B', enabled)
        return self.device.send_cmd_sync(Command.MF1_SET_RANDOM_UID_MODE, data)

    @expect_response(Status.SUCCESS)
    def mf1_get_random_uid_mode(self):
        """
        Get whether random-UID mode is enabled for the current card slot.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.MF1_GET_RANDOM_UID_MODE)
        if resp.status == Status.SUCCESS:
            resp.parsed = struct.unpack('!B', resp.data)[0] == 1
        return resp

    @expect_response(Status.SUCCESS)
    def mf1_set_reader_keys_anim(self, enabled: bool):
        """
        Enable/disable the reader-key capture LED animation (rainbow radiating
        from the center of the LED bar outward).

        :param enabled: Whether to run the animation
        :return:
        """
        data = struct.pack('!B', enabled)
        return self.device.send_cmd_sync(Command.MF1_SET_READER_KEYS_ANIM, data)

    @expect_response(Status.SUCCESS)
    def mf1_get_detection_count(self):
        """
        Get the statistics of the current detection records.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.MF1_GET_DETECTION_COUNT)
        if resp.status == Status.SUCCESS:
            resp.parsed, = struct.unpack('!I', resp.data)
        return resp

    @expect_response(Status.SUCCESS)
    def mf1_get_detection_log(self, index: int):
        """
        Get detection logs from the specified index position.

        :param index: start index
        :return:
        """
        data = struct.pack('!I', index)
        resp = self.device.send_cmd_sync(Command.MF1_GET_DETECTION_LOG, data)
        if resp.status == Status.SUCCESS:
            # convert
            result_list = []
            pos = 0
            while pos < len(resp.data):
                block, bitfield, uid, nt, nr, ar = struct.unpack_from('!BB4s4s4s4s', resp.data, pos)
                result_list.append({
                    'block': block,
                    'type': ['A', 'B'][bitfield & 0x01],
                    'is_nested': bool(bitfield & 0x02),
                    'uid': uid.hex(),
                    'nt': nt.hex(),
                    'nr': nr.hex(),
                    'ar': ar.hex()
                })
                pos += struct.calcsize('!BB4s4s4s4s')
            resp.parsed = result_list
        return resp

    @expect_response(Status.SUCCESS)
    def mf0_ntag_get_detection_enable(self):
        """
        Get whether NTAG password detection is enabled.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_GET_DETECTION_ENABLE)
        if resp.status == Status.SUCCESS:
            resp.parsed = struct.unpack('!B', resp.data)[0] == 1
        return resp

    @expect_response(Status.SUCCESS)
    def mf0_ntag_set_detection_enable(self, enabled: bool):
        """
        Set whether to enable NTAG password detection.

        :param enable: Whether to enable
        :return:
        """
        data = struct.pack('!B', enabled)
        return self.device.send_cmd_sync(Command.MF0_NTAG_SET_DETECTION_ENABLE, data)

    @expect_response(Status.SUCCESS)
    def mf0_ntag_get_detection_count(self):
        """
        Get the statistics of the current NTAG password detection records.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_GET_DETECTION_COUNT)
        if resp.status == Status.SUCCESS:
            resp.parsed = struct.unpack('!I', resp.data)[0]
        return resp

    @expect_response(Status.SUCCESS)
    def mf0_ntag_get_detection_log(self, index: int):
        """
        Get NTAG password detection logs from the specified index position.

        :param index: start index
        :return:
        """
        data = struct.pack('!I', index)
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_GET_DETECTION_LOG, data)
        if resp.status == Status.SUCCESS:
            # convert - each log entry is just a 4-byte password
            result_list = []
            pos = 0
            while pos < len(resp.data):
                password = resp.data[pos:pos+4]
                result_list.append({
                    'password': password.hex()
                })
                pos += 4
            resp.parsed = result_list
        return resp

    @expect_response(Status.SUCCESS)
    def mf1_write_emu_block_data(self, block_start: int, block_data: bytes):
        """
        Set the block data of the analog card of MF1.

        :param block_start:  Start setting the location of block data, including this location
        :param block_data:  The byte buffer of the block data to be set can contain multiple block data,
                            automatically from block_start  increment
        :return:
        """
        data = struct.pack(f'!B{len(block_data)}s', block_start, block_data)
        return self.device.send_cmd_sync(Command.MF1_WRITE_EMU_BLOCK_DATA, data)

    @expect_response(Status.SUCCESS)
    def mf1_read_emu_block_data(self, block_start: int, block_count: int):
        """
            Gets data for selected block range
        """
        data = struct.pack('!BB', block_start, block_count)
        resp = self.device.send_cmd_sync(Command.MF1_READ_EMU_BLOCK_DATA, data)
        resp.parsed = resp.data
        return resp

    @expect_response(Status.SUCCESS)
    def mfu_get_emu_pages_count(self):
        """
            Gets the number of pages available in the current MF0 / NTAG slot
        """
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_GET_PAGE_COUNT)
        resp.parsed = resp.data[0]
        return resp

    @expect_response(Status.SUCCESS)
    def mfu_read_emu_page_data(self, page_start: int, page_count: int):
        """
            Gets data for selected block range
        """
        data = struct.pack('!BB', page_start, page_count)
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_READ_EMU_PAGE_DATA, data)
        resp.parsed = resp.data
        return resp

    @expect_response(Status.SUCCESS)
    def mfu_write_emu_page_data(self, page_start: int, data: bytes):
        """
            Gets data for selected block range
        """
        count = len(data) >> 2

        assert (len(data) % 4) == 0
        assert (page_start >= 0) and (count + page_start) <= 256

        data = struct.pack('!BB', page_start, count) + data
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_WRITE_EMU_PAGE_DATA, data)
        return resp

    @expect_response(Status.SUCCESS)
    def mfu_read_emu_counter_data(self, index: int) -> tuple[int, bool]:
        """
            Gets data for selected counter
        """
        data = struct.pack('!B', index)
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_GET_COUNTER_DATA, data)
        if resp.status == Status.SUCCESS:
            resp.parsed = (((resp.data[2] << 16) | (resp.data[1] << 8) | resp.data[0]), resp.data[3] == 0xBD)
        return resp

    @expect_response(Status.SUCCESS)
    def mfu_write_emu_counter_data(self, index: int, value: int, reset_tearing: bool):
        """
            Sets data for selected counter
        """
        data = struct.pack('!BBBB', index | (int(reset_tearing) << 7),
                           value & 0xFF, (value >> 8) & 0xFF, (value >> 16) & 0xFF)
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_SET_COUNTER_DATA, data)
        return resp

    @expect_response(Status.SUCCESS)
    def mfu_reset_auth_cnt(self):
        """
            Resets authentication counter
        """
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_RESET_AUTH_CNT, bytes())
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[0]
        return resp

    @expect_response(Status.SUCCESS)
    def hf14a_set_anti_coll_data(self, uid: bytes, atqa: bytes, sak: bytes, ats: bytes = b''):
        """
        Set anti-collision data of current HF slot (UID/SAK/ATQA/ATS).

        :param uid:  uid bytes
        :param atqa: atqa bytes
        :param sak:  sak bytes
        :param ats:  ats bytes (optional)
        :return:
        """
        data = struct.pack(f'!B{len(uid)}s2s1sB{len(ats)}s', len(uid), uid, atqa, sak, len(ats), ats)
        return self.device.send_cmd_sync(Command.HF14A_SET_ANTI_COLL_DATA, data)

    @expect_response(Status.SUCCESS)
    def set_slot_tag_nick(self, slot: SlotNumber, sense_type: TagSenseType, name: str):
        """
        Set the nick name of the slot.

        :param slot:  Card slot number
        :param sense_type:  field type
        :param name:  Card slot nickname
        :return:
        """
        encoded_name = name.encode(encoding="utf8")
        if len(encoded_name) > 32:
            raise ValueError("Your tag nick name too long.")
        # SlotNumber() will raise error for us if slot not in slot range
        data = struct.pack(f'!BB{len(encoded_name)}s', SlotNumber.to_fw(slot), sense_type, encoded_name)
        return self.device.send_cmd_sync(Command.SET_SLOT_TAG_NICK, data)

    @expect_response(Status.SUCCESS)
    def get_slot_tag_nick(self, slot: SlotNumber, sense_type: TagSenseType):
        """
        Get the nick name of the slot.

        :param slot:  Card slot number
        :param sense_type:  field type
        :return:
        """
        # SlotNumber() will raise error for us if slot not in slot range
        data = struct.pack('!BB', SlotNumber.to_fw(slot), sense_type)
        resp = self.device.send_cmd_sync(Command.GET_SLOT_TAG_NICK, data)
        resp.parsed = resp.data.decode(encoding="utf8")
        return resp

    @expect_response(Status.SUCCESS)
    def get_all_slot_nicks(self):
        resp = self.device.send_cmd_sync(Command.GET_ALL_SLOT_NICKS, b'')

        slots = []
        i = 0
        slot_index = 0

        while i < len(resp.data) and slot_index < 8:
            slot_names = {'hf': '', 'lf': ''}

            if i < len(resp.data):
                hf_len = resp.data[i]
                i += 1
                if hf_len > 0 and i + hf_len <= len(resp.data):
                    slot_names['hf'] = resp.data[i:i + hf_len].decode(encoding="utf8", errors="ignore")
                    i += hf_len
                else:
                    i += hf_len

            if i < len(resp.data):
                lf_len = resp.data[i]
                i += 1
                if lf_len > 0 and i + lf_len <= len(resp.data):
                    slot_names['lf'] = resp.data[i:i + lf_len].decode(encoding="utf8", errors="ignore")
                    i += lf_len
                else:
                    i += lf_len

            slots.append(slot_names)
            slot_index += 1

        resp.parsed = slots
        return resp

    @expect_response(Status.SUCCESS)
    def delete_slot_tag_nick(self, slot: SlotNumber, sense_type: TagSenseType):
        """
        Delete the nick name of the slot.

        :param slot:  Card slot number
        :param sense_type:  field type
        :return:
        """
        # SlotNumber() will raise error for us if slot not in slot range
        data = struct.pack('!BB', SlotNumber.to_fw(slot), sense_type)
        return self.device.send_cmd_sync(Command.DELETE_SLOT_TAG_NICK, data)

    @expect_response(Status.SUCCESS)
    def mf1_get_emulator_config(self):
        """
            Get array of Mifare Classic emulators settings:
            [0] - mf1_is_detection_enable (mfkey32)
            [1] - mf1_is_gen1a_magic_mode
            [2] - mf1_is_gen2_magic_mode
            [3] - mf1_is_use_mf1_coll_res (use UID/BCC/SAK/ATQA from 0 block)
            [4] - mf1_get_write_mode

        :return:
        """
        resp = self.device.send_cmd_sync(Command.MF1_GET_EMULATOR_CONFIG)
        if resp.status == Status.SUCCESS:
            b1, b2, b3, b4, b5 = struct.unpack('!????B', resp.data)
            resp.parsed = {'detection': b1,
                           'gen1a_mode': b2,
                           'gen2_mode': b3,
                           'block_anti_coll_mode': b4,
                           'write_mode': b5}
        return resp

    @expect_response(Status.SUCCESS)
    def mf1_set_gen1a_mode(self, enabled: bool):
        """
        Set gen1a magic mode
        """
        data = struct.pack('!B', enabled)
        return self.device.send_cmd_sync(Command.MF1_SET_GEN1A_MODE, data)

    @expect_response(Status.SUCCESS)
    def mf1_set_gen2_mode(self, enabled: bool):
        """
        Set gen2 magic mode
        """
        data = struct.pack('!B', enabled)
        return self.device.send_cmd_sync(Command.MF1_SET_GEN2_MODE, data)

    @expect_response(Status.SUCCESS)
    def mf1_set_block_anti_coll_mode(self, enabled: bool):
        """
        Set 0 block anti-collision data
        """
        data = struct.pack('!B', enabled)
        return self.device.send_cmd_sync(Command.MF1_SET_BLOCK_ANTI_COLL_MODE, data)

    @expect_response(Status.SUCCESS)
    def mf1_set_write_mode(self, mode: int):
        """
        Set write mode
        """
        data = struct.pack('!B', mode)
        return self.device.send_cmd_sync(Command.MF1_SET_WRITE_MODE, data)

    def mf1_get_prng_type(self):
        """
        Get PRNG type used for MF1 auth nonce:
          0 = Static  (fixed nonce)
          1 = Weak    (LFSR-based, predictable)
          2 = Hard    (unpredictable)
        """
        resp = self.device.send_cmd_sync(Command.MF1_GET_PRNG_TYPE)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[0]
        return resp

    @expect_response(Status.SUCCESS)
    def mf1_set_prng_type(self, prng_type: int):
        """
        Set PRNG type (0=Static, 1=Weak, 2=Hard)
        """
        data = struct.pack('!B', prng_type)
        return self.device.send_cmd_sync(Command.MF1_SET_PRNG_TYPE, data)

    @expect_response(Status.SUCCESS)
    def slot_data_config_save(self):
        """
        Update the configuration and data of the card slot to flash.
        :return:
        """
        return self.device.send_cmd_sync(Command.SLOT_DATA_CONFIG_SAVE)

    def enter_bootloader(self):
        """
        Reboot into DFU mode (bootloader)
        :return:
        """
        self.device.send_cmd_auto(Command.ENTER_BOOTLOADER, close=True)

    @expect_response(Status.SUCCESS)
    def get_animation_mode(self):
        """
        Get animation mode value
        """
        resp = self.device.send_cmd_sync(Command.GET_ANIMATION_MODE)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[0]
        return resp

    @expect_response(Status.SUCCESS)
    def get_enabled_slots(self):
        """
        Get enabled slots
        """
        resp = self.device.send_cmd_sync(Command.GET_ENABLED_SLOTS)
        if resp.status == Status.SUCCESS:
            resp.parsed = [{'hf': hf, 'lf': lf} for hf, lf in struct.iter_unpack('!BB', resp.data)]
        return resp

    @expect_response(Status.SUCCESS)
    def set_animation_mode(self, value: int):
        """
        Set animation mode value
        """
        data = struct.pack('!B', value)
        return self.device.send_cmd_sync(Command.SET_ANIMATION_MODE, data)

    @expect_response(Status.SUCCESS)
    def set_runtime_undercover_mode(self, enabled: bool):
        """Set the BLE-session-only LED suppression mode."""
        if not isinstance(enabled, bool):
            raise ValueError("enabled must be a boolean")
        resp = self.device.send_cmd_sync(
            Command.SET_RUNTIME_UNDERCOVER_MODE,
            struct.pack('!?', enabled),
        )
        resp.parsed = True
        return resp

    @expect_response(Status.SUCCESS)
    def get_sleep_timeout(self):
        """
        Get the wake timeout (in seconds) after a button wakeup
        """
        resp = self.device.send_cmd_sync(Command.GET_SLEEP_TIMEOUT)
        if resp.status == Status.SUCCESS:
            resp.parsed = struct.unpack('!B', resp.data)[0]
        return resp

    @expect_response(Status.SUCCESS)
    def set_sleep_timeout(self, seconds: int):
        """
        Set the wake timeout (in seconds) after a button wakeup
        """
        data = struct.pack('!B', seconds)
        return self.device.send_cmd_sync(Command.SET_SLEEP_TIMEOUT, data)

    @expect_response(Status.SUCCESS)
    def reset_settings(self):
        """
        Reset settings stored in flash memory
        """
        resp = self.device.send_cmd_sync(Command.RESET_SETTINGS)
        resp.parsed = resp.status == Status.SUCCESS
        return resp

    @expect_response(Status.SUCCESS)
    def save_settings(self):
        """
        Store settings to flash memory
        """
        resp = self.device.send_cmd_sync(Command.SAVE_SETTINGS)
        resp.parsed = resp.status == Status.SUCCESS
        return resp

    @expect_response(Status.SUCCESS)
    def wipe_fds(self):
        """
        Reset to factory settings
        """
        resp = self.device.send_cmd_sync(Command.WIPE_FDS)
        resp.parsed = resp.status == Status.SUCCESS
        self.device.close()
        return resp

    @expect_response(Status.SUCCESS)
    def get_battery_info(self):
        """
        Get battery info
        """
        resp = self.device.send_cmd_sync(Command.GET_BATTERY_INFO)
        if resp.status == Status.SUCCESS:
            resp.parsed = struct.unpack('!HB', resp.data)
        return resp

    @expect_response(Status.SUCCESS)
    def get_button_press_config(self, button: ButtonType):
        """
        Get config of button press function
        """
        data = struct.pack('!B', button)
        resp = self.device.send_cmd_sync(Command.GET_BUTTON_PRESS_CONFIG, data)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[0]
        return resp

    @expect_response(Status.SUCCESS)
    def set_button_press_config(self, button: ButtonType, function: ButtonPressFunction):
        """
        Set config of button press function
        """
        data = struct.pack('!BB', button, function)
        return self.device.send_cmd_sync(Command.SET_BUTTON_PRESS_CONFIG, data)

    @expect_response(Status.SUCCESS)
    def get_long_button_press_config(self, button: ButtonType):
        """
        Get config of long button press function
        """
        data = struct.pack('!B', button)
        resp = self.device.send_cmd_sync(Command.GET_LONG_BUTTON_PRESS_CONFIG, data)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[0]
        return resp

    @expect_response(Status.SUCCESS)
    def set_long_button_press_config(self, button: ButtonType, function: ButtonPressFunction):
        """
        Set config of long button press function
        """
        data = struct.pack('!BB', button, function)
        return self.device.send_cmd_sync(Command.SET_LONG_BUTTON_PRESS_CONFIG, data)

    @expect_response(Status.SUCCESS)
    def set_ble_connect_key(self, key: str):
        """
        Set config of ble connect key
        """
        data_bytes = key.encode(encoding='ascii')

        # check key length
        if len(data_bytes) != 6:
            raise ValueError("The ble connect key length must be 6")

        data = struct.pack('6s', data_bytes)
        return self.device.send_cmd_sync(Command.SET_BLE_PAIRING_KEY, data)

    @expect_response(Status.SUCCESS)
    def get_ble_pairing_key(self):
        """
        Get config of ble connect key
        """
        resp = self.device.send_cmd_sync(Command.GET_BLE_PAIRING_KEY)
        resp.parsed = resp.data.decode(encoding='ascii')
        return resp

    @expect_response(Status.SUCCESS)
    def delete_all_ble_bonds(self):
        """
        From peer manager delete all bonds.
        """
        return self.device.send_cmd_sync(Command.DELETE_ALL_BLE_BONDS)

    @expect_response(Status.SUCCESS)
    def get_device_capabilities(self):
        """
        Get list of commands that client understands
        """
        try:
            resp = self.device.send_cmd_sync(Command.GET_DEVICE_CAPABILITIES)
        except chameleon_com.CMDInvalidException:
            # Single, clear message; the caller's connect handler surfaces it.
            raise UnexpectedResponseError(
                "device does not understand GET_DEVICE_CAPABILITIES; please update firmware")
        if resp.status == Status.SUCCESS:
            resp.parsed = [x[0] for x in struct.iter_unpack('!H', resp.data)]
        return resp

    @expect_response(Status.SUCCESS)
    def get_device_model(self):
        """
        Get device model
        0 - Chameleon Ultra
        1 - Chameleon Lite
        """

        resp = self.device.send_cmd_sync(Command.GET_DEVICE_MODEL)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[0]
        return resp

    @expect_response(Status.SUCCESS)
    def get_device_settings(self):
        """
        Get all possible settings
        For version 6:
        settings[0] = SETTINGS_CURRENT_VERSION; // current version
        settings[1] = settings_get_animation_config(); // animation mode
        settings[2] = settings_get_button_press_config('A'); // short A button press mode
        settings[3] = settings_get_button_press_config('B'); // short B button press mode
        settings[4] = settings_get_long_button_press_config('A'); // long A button press mode
        settings[5] = settings_get_long_button_press_config('B'); // long B button press mode
        settings[6] = settings_get_ble_pairing_enable(); // does device require pairing
        settings[7:13] = settings_get_ble_pairing_key(); // BLE pairing key
        settings[13] = sleep_timeout in seconds; // wake timeout after button wakeup
        """
        resp = self.device.send_cmd_sync(Command.GET_DEVICE_SETTINGS)
        if resp.status == Status.SUCCESS:
            expected_size = struct.calcsize('!BBBBBBB6sB')
            if len(resp.data) != expected_size:
                raise UnexpectedResponseError(
                    f"GET_DEVICE_SETTINGS v6 expected {expected_size} bytes, "
                    f"got {len(resp.data)}")
            if resp.data[0] > CURRENT_VERSION_SETTINGS:
                raise ValueError("Settings version in app older than Chameleon. "
                                 "Please upgrade client")
            if resp.data[0] < CURRENT_VERSION_SETTINGS:
                raise ValueError("Settings version in app newer than Chameleon. "
                                 "Please upgrade Chameleon firmware")
            settings_version, animation_mode, btn_press_A, btn_press_B, btn_long_press_A, \
                btn_long_press_B, ble_pairing_enable, ble_pairing_key, sleep_timeout = \
                struct.unpack('!BBBBBBB6sB', resp.data)
            resp.parsed = {'settings_version': settings_version,
                           'animation_mode': animation_mode,
                           'btn_press_A': btn_press_A,
                           'btn_press_B': btn_press_B,
                           'btn_long_press_A': btn_long_press_A,
                           'btn_long_press_B': btn_long_press_B,
                           'ble_pairing_enable': ble_pairing_enable,
                           'ble_pairing_key': ble_pairing_key,
                           'sleep_timeout': sleep_timeout}
        return resp

    @expect_response(Status.SUCCESS)
    def hf14a_get_anti_coll_data(self):
        """
        Get anti-collision data from current HF slot (UID/SAK/ATQA/ATS)

        :return:
        """
        resp = self.device.send_cmd_sync(Command.HF14A_GET_ANTI_COLL_DATA)
        if resp.status == Status.SUCCESS and len(resp.data) > 0:
            # uidlen[1]|uid[uidlen]|atqa[2]|sak[1]|atslen[1]|ats[atslen]
            offset = 0
            uidlen, = struct.unpack_from('!B', resp.data, offset)
            offset += struct.calcsize('!B')
            uid, atqa, sak, atslen = struct.unpack_from(f'!{uidlen}s2s1sB', resp.data, offset)
            offset += struct.calcsize(f'!{uidlen}s2s1sB')
            ats, = struct.unpack_from(f'!{atslen}s', resp.data, offset)
            offset += struct.calcsize(f'!{atslen}s')
            resp.parsed = {'uid': uid, 'atqa': atqa, 'sak': sak, 'ats': ats}
        return resp

    @expect_response(Status.SUCCESS)
    def mf0_ntag_get_uid_magic_mode(self):
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_GET_UID_MAGIC_MODE)
        if resp.status == Status.SUCCESS:
            resp.parsed, = struct.unpack('!?', resp.data)
        return resp

    @expect_response(Status.SUCCESS)
    def mf0_ntag_set_uid_magic_mode(self, enabled: bool):
        return self.device.send_cmd_sync(Command.MF0_NTAG_SET_UID_MAGIC_MODE, struct.pack('?', enabled))

    @expect_response(Status.SUCCESS)
    def mf0_ntag_get_version_data(self):
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_GET_VERSION_DATA)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[:8]
        return resp

    @expect_response(Status.SUCCESS)
    def mf0_ntag_set_version_data(self, data: bytes):
        assert len(data) == 8
        return self.device.send_cmd_sync(Command.MF0_NTAG_SET_VERSION_DATA, data)

    @expect_response(Status.SUCCESS)
    def mf0_ntag_get_signature_data(self):
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_GET_SIGNATURE_DATA)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[:32]
        return resp

    @expect_response(Status.SUCCESS)
    def mf0_ntag_set_signature_data(self, data: bytes):
        assert len(data) == 32
        return self.device.send_cmd_sync(Command.MF0_NTAG_SET_SIGNATURE_DATA, data)

    @expect_response(Status.SUCCESS)
    def mf0_ntag_get_write_mode(self):
        """
        Get write mode for MF0/NTAG
        """
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_GET_WRITE_MODE)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[0]
        return resp

    @expect_response(Status.SUCCESS)
    def mf0_ntag_set_write_mode(self, mode: int):
        """
        Set write mode for MF0/NTAG
        """
        data = struct.pack('!B', mode)
        return self.device.send_cmd_sync(Command.MF0_NTAG_SET_WRITE_MODE, data)

    @expect_response(Status.SUCCESS)
    def get_ble_pairing_enable(self):
        """
        Is ble pairing enable?

        :return: True if pairing is enable, False if pairing disabled
        """
        resp = self.device.send_cmd_sync(Command.GET_BLE_PAIRING_ENABLE)
        if resp.status == Status.SUCCESS:
            resp.parsed, = struct.unpack('!?', resp.data)
        return resp

    @expect_response(Status.SUCCESS)
    def set_ble_pairing_enable(self, enabled: bool):
        data = struct.pack('!B', enabled)
        return self.device.send_cmd_sync(Command.SET_BLE_PAIRING_ENABLE, data)

    @expect_response(Status.SUCCESS)
    def get_keyboard_hid_enable(self):
        """
        Is the keyboard HID feature (USB + BLE) enabled?

        :return: True if enabled, False otherwise
        """
        resp = self.device.send_cmd_sync(Command.GET_KEYBOARD_HID_ENABLE)
        if resp.status == Status.SUCCESS:
            resp.parsed, = struct.unpack('!?', resp.data)
        return resp

    @expect_response(Status.SUCCESS)
    def set_keyboard_hid_enable(self, enabled: bool):
        """
        Enable/disable the opt-in keyboard HID feature. Persisted by
        save_settings(); requires a device reboot to (un)expose the USB HID
        interface and register/tear down the BLE HID service.
        """
        data = struct.pack('!B', enabled)
        return self.device.send_cmd_sync(Command.SET_KEYBOARD_HID_ENABLE, data)

    @expect_response(Status.SUCCESS)
    def mf1_get_field_off_do_reset(self):
        resp = self.device.send_cmd_sync(Command.MF1_GET_FIELD_OFF_DO_RESET)
        if resp.status == Status.SUCCESS:
            resp.parsed = struct.unpack('!B', resp.data)[0] == 1
        return resp

    @expect_response(Status.SUCCESS)
    def mf1_set_field_off_do_reset(self, enabled: bool):
        data = struct.pack('!B', enabled)
        return self.device.send_cmd_sync(Command.MF1_SET_FIELD_OFF_DO_RESET, data)


def test_fn():
    # connect to chameleon
    dev = chameleon_com.ChameleonCom()
    try:
        dev.open('com19')
    except chameleon_com.OpenFailException:
        dev.open('/dev/ttyACM0')

    cml = ChameleonCMD(dev)
    ver = cml.get_app_version()
    print(f"Firmware number of application: {ver[0]}.{ver[1]}")
    chip = cml.get_device_chip_id()
    print(f"Device chip id: {chip}")

    # change to reader mode
    cml.set_device_reader_mode()

    options = {
        'activate_rf_field': 1,
        'wait_response': 1,
        'append_crc': 0,
        'auto_select': 0,
        'keep_rf_field': 1,
        'check_response_crc': 0,
    }

    try:
        # unlock 1
        resp = cml.hf14a_raw(options=options, resp_timeout_ms=1000, data=[0x40], bitlen=7)

        if resp[0] == 0x0a:
            print("Gen1A unlock 1 success")
            # unlock 2
            resp = cml.hf14a_raw(options=options, resp_timeout_ms=1000, data=[0x43])
            if resp[0] == 0x0a:
                print("Gen1A unlock 2 success")
                print("Start dump gen1a memory...")
                # Transfer with crc
                options['append_crc'] = 1
                options['check_response_crc'] = 1
                block = 0
                while block < 64:
                    # Tag read block cmd
                    cmd_read_gen1a_block = [0x30, block]
                    if block == 63:
                        options['keep_rf_field'] = 0
                    resp = cml.hf14a_raw(options=options, resp_timeout_ms=100, data=cmd_read_gen1a_block)

                    print(f"Block {block} : {resp.hex()}")
                    block += 1

            else:
                print("Gen1A unlock 2 fail")
                raise
        else:
            print("Gen1A unlock 1 fail")
            raise
    except Exception:
        options['keep_rf_field'] = 0
        options['wait_response'] = 0
        cml.hf14a_raw(options=options)

    # disconnect
    dev.close()


if __name__ == '__main__':
    test_fn()
