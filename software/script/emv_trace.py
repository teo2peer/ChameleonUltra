"""Strict, transport-neutral codec for the retained EMV trace protocol."""

from dataclasses import dataclass
import struct
from typing import Any, Callable, Dict, Optional, Tuple, Union
import zlib


PROTOCOL_VERSION = 1

OPT_MAXIMUM_PROCESSING = 0x01
OPT_INCLUDE_RF = 0x02
OPT_TIMING = 0x04
OPT_RECORD_GRID = 0x08
OPT_TRANSACTION_LOG = 0x10
OPT_PDOL_FALLBACK = 0x20
OPT_ALL = 0x3F

TRACE_COMPLETE = 0x00000001
TRACE_TIMEOUT = 0x00000002
TRACE_LOG_TRUNCATED = 0x00000004
TRACE_RF_TRUNCATED = 0x00000008
TRACE_RESPONSE_TRUNCATED = 0x00000010
TRACE_APP_LIMIT = 0x00000020
TRACE_TIMING_VALID = 0x00000040
TRACE_MAXIMUM_PROCESSING = 0x00000080
TRACE_TRANSPORT_ERROR = 0x00000100
TRACE_ALL = 0x000001FF

VALID_STAGES = frozenset(range(10)) | {0xFF}

RECORD_RF = 1
RECORD_APDU = 2
RECORD_APP = 3
RECORD_SUMMARY = 4


class EmvTraceError(ValueError):
    """The trace stream violates the version-1 wire format."""


def _uint(name: str, value: int, maximum: int) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or not 0 <= value <= maximum:
        raise EmvTraceError(f"{name} must be 0..{maximum}")
    return value


def _fixed_bytes(name: str, value: bytes, length: int) -> bytes:
    if not isinstance(value, (bytes, bytearray, memoryview)):
        raise EmvTraceError(f"{name} must be bytes")
    result = bytes(value)
    if len(result) != length:
        raise EmvTraceError(f"{name} must be exactly {length} bytes")
    return result


@dataclass(frozen=True)
class EmvTraceRequest:
    flags: int = OPT_TIMING | OPT_PDOL_FALLBACK
    max_aids: int = 0
    max_records: int = 0
    max_apdus: int = 0
    budget_ms: int = 0
    amount: bytes = b"\x00" * 6
    country: bytes = b"\x00" * 2
    currency: bytes = b"\x00" * 2
    date: bytes = b"\x00" * 3
    transaction_type: int = 0
    cryptogram_type: int = 0xFF


@dataclass(frozen=True)
class EmvTraceStart:
    state: int
    scan_id: int
    flags: int
    raw: bytes


@dataclass(frozen=True)
class EmvTraceMeta:
    state: int
    result_status: int
    flags: int
    scan_id: int
    stored_records: int
    observed_records: int
    stored_bytes: int
    required_bytes: int
    first_dropped: int
    crc32: int
    app_count: int
    elapsed_ms: int
    uid: bytes
    atqa: bytes
    sak: int
    ats: bytes
    raw: bytes


@dataclass(frozen=True)
class RfPayload:
    direction: int
    bit_length: int
    data: bytes


@dataclass(frozen=True)
class ApduPayload:
    sw: int
    command: bytes
    response: bytes


@dataclass(frozen=True)
class AppPayload:
    aid: bytes
    priority: int


@dataclass(frozen=True)
class SummaryPayload:
    stored_records: int
    observed_records: int
    flags: int


RecordPayload = Union[RfPayload, ApduPayload, AppPayload, SummaryPayload]


@dataclass(frozen=True)
class EmvTraceRecord:
    record_type: int
    sequence: int
    stage: int
    app_index: int
    attempt: int
    flags: int
    status: int
    timestamp_ms: int
    payload: RecordPayload
    raw: bytes


@dataclass(frozen=True)
class EmvTracePage:
    flags: int
    scan_id: int
    start_record: int
    next_record: int
    returned_count: int
    records_bytes: int
    records: Tuple[EmvTraceRecord, ...]
    raw_records: bytes
    raw: bytes


@dataclass(frozen=True)
class EmvTraceDownload:
    scan_id: int
    meta: EmvTraceMeta
    pages: Tuple[EmvTracePage, ...]
    records: Tuple[EmvTraceRecord, ...]
    raw_records: bytes
    start: Optional[EmvTraceStart] = None


def encode_start_request(request: EmvTraceRequest) -> bytes:
    if not isinstance(request, EmvTraceRequest):
        raise EmvTraceError("request must be an EmvTraceRequest")
    flags = _uint("flags", request.flags, 0xFF)
    if flags & ~OPT_ALL:
        raise EmvTraceError("flags contain reserved bits")
    max_aids = _uint("max_aids", request.max_aids, 16)
    max_records = _uint("max_records", request.max_records, 64)
    max_apdus = _uint("max_apdus", request.max_apdus, 512)
    budget_ms = _uint("budget_ms", request.budget_ms, 30000)
    transaction_type = _uint("transaction_type", request.transaction_type, 0xFF)
    cryptogram_type = _uint("cryptogram_type", request.cryptogram_type, 0xFF)
    if cryptogram_type not in (0xFF, 0x00, 0x40, 0x80):
        raise EmvTraceError("cryptogram_type must be FF, 00, 40, or 80")
    payload = struct.pack(
        "!BBBBHI6s2s2s3sBB",
        PROTOCOL_VERSION,
        flags,
        max_aids,
        max_records,
        max_apdus,
        budget_ms,
        _fixed_bytes("amount", request.amount, 6),
        _fixed_bytes("country", request.country, 2),
        _fixed_bytes("currency", request.currency, 2),
        _fixed_bytes("date", request.date, 3),
        transaction_type,
        cryptogram_type,
    )
    if len(payload) != 25:
        raise AssertionError("EMV trace START encoder produced the wrong size")
    return payload


def encode_meta_request(scan_id: int) -> bytes:
    return struct.pack("!BI", PROTOCOL_VERSION, _uint("scan_id", scan_id, 0xFFFFFFFF))


def encode_get_request(scan_id: int, start_record: int, max_payload: int) -> bytes:
    _uint("scan_id", scan_id, 0xFFFFFFFF)
    _uint("start_record", start_record, 0xFFFFFFFF)
    _uint("max_payload", max_payload, 0xFFFF)
    if max_payload < 48:
        raise EmvTraceError("max_payload must be at least 48")
    return struct.pack("!BIIH", PROTOCOL_VERSION, scan_id, start_record, max_payload)


BytesLike = Union[bytes, bytearray, memoryview]


def parse_start_response(data: BytesLike) -> EmvTraceStart:
    raw = bytes(data)
    if len(raw) != 10:
        raise EmvTraceError(f"START response must be exactly 10 bytes, got {len(raw)}")
    version, state, scan_id, flags = struct.unpack("!BBII", raw)
    if version != PROTOCOL_VERSION:
        raise EmvTraceError(f"unsupported START version {version}")
    if state not in range(4):
        raise EmvTraceError(f"invalid START state {state}")
    if scan_id == 0:
        raise EmvTraceError("START returned scan_id zero")
    return EmvTraceStart(state, scan_id, flags, raw)


def parse_meta_response(data: BytesLike,
                        expected_scan_id: Optional[int] = None) -> EmvTraceMeta:
    raw = bytes(data)
    if len(raw) < 47:
        raise EmvTraceError(f"META response is too short: {len(raw)} bytes")
    version, state, result_status, flags, scan_id, stored_records, observed_records, \
        stored_bytes, required_bytes, first_dropped, crc32, app_count, elapsed_ms = \
        struct.unpack_from("!BBHIIIIIIIIHI", raw)
    if version != PROTOCOL_VERSION:
        raise EmvTraceError(f"unsupported META version {version}")
    if state not in range(4):
        raise EmvTraceError(f"invalid META state {state}")
    if expected_scan_id is not None and scan_id != expected_scan_id:
        raise EmvTraceError(
            f"META session mismatch: expected {expected_scan_id}, got {scan_id}")
    if scan_id == 0:
        raise EmvTraceError("META returned scan_id zero")
    if flags & ~TRACE_ALL:
        raise EmvTraceError(f"META contains unknown flags 0x{flags:08x}")
    if stored_records > observed_records:
        raise EmvTraceError("META stored_records exceeds observed_records")
    if stored_bytes > required_bytes:
        raise EmvTraceError("META stored_bytes exceeds required_bytes")
    if first_dropped != 0xFFFFFFFF and first_dropped >= observed_records:
        raise EmvTraceError("META first_dropped is outside observed_records")
    if bool(flags & TRACE_LOG_TRUNCATED) != (first_dropped != 0xFFFFFFFF):
        raise EmvTraceError("META first_dropped disagrees with LOG_TRUNCATED")
    offset = 42
    uid_len = raw[offset]
    offset += 1
    if offset + uid_len + 4 > len(raw):
        raise EmvTraceError("META UID overruns response")
    uid = raw[offset:offset + uid_len]
    offset += uid_len
    atqa = raw[offset:offset + 2]
    sak = raw[offset + 2]
    ats_len = raw[offset + 3]
    offset += 4
    if offset + ats_len != len(raw):
        raise EmvTraceError("META ATS length does not consume the response exactly")
    ats = raw[offset:]
    return EmvTraceMeta(
        state, result_status, flags, scan_id, stored_records, observed_records,
        stored_bytes, required_bytes, first_dropped, crc32, app_count, elapsed_ms,
        uid, atqa, sak, ats, raw,
    )


def parse_record(data: BytesLike) -> EmvTraceRecord:
    raw = bytes(data)
    if len(raw) < 18:
        raise EmvTraceError("record is shorter than its 18-byte minimum")
    body_len = struct.unpack_from("!H", raw)[0]
    if body_len < 16 or body_len + 2 != len(raw):
        raise EmvTraceError("record length prefix does not match the atomic record")
    version, record_type, sequence, stage, app_index, attempt, flags, status, timestamp_ms = \
        struct.unpack_from("!BBIBBBBHI", raw, 2)
    if version != PROTOCOL_VERSION:
        raise EmvTraceError(f"unsupported record version {version}")
    if stage not in VALID_STAGES or (record_type == RECORD_SUMMARY) != (stage == 0xFF):
        raise EmvTraceError(f"invalid stage {stage} for record type {record_type}")
    value = raw[18:]
    if record_type == RECORD_RF:
        if len(value) < 5:
            raise EmvTraceError("RF record payload is too short")
        direction, bit_length, data_len = struct.unpack_from("!BHH", value)
        if direction not in (0, 1):
            raise EmvTraceError(f"invalid RF direction {direction}")
        if data_len != len(value) - 5:
            raise EmvTraceError("RF data length does not match record payload")
        payload: RecordPayload = RfPayload(direction, bit_length, value[5:])
    elif record_type == RECORD_APDU:
        if len(value) < 6:
            raise EmvTraceError("APDU record payload is too short")
        sw, command_len, response_len = struct.unpack_from("!HHH", value)
        if command_len + response_len != len(value) - 6:
            raise EmvTraceError("APDU command/response lengths do not match record payload")
        command = value[6:6 + command_len]
        response = value[6 + command_len:]
        expected_sw = int.from_bytes(response[-2:], "big") if response_len >= 2 else 0xFFFF
        if sw != expected_sw:
            raise EmvTraceError("APDU status word disagrees with response")
        payload = ApduPayload(sw, command, response)
    elif record_type == RECORD_APP:
        if len(value) < 2:
            raise EmvTraceError("application record payload is too short")
        aid_len = value[0]
        if not 5 <= aid_len <= 16 or aid_len + 2 != len(value):
            raise EmvTraceError("application AID length does not match record payload")
        payload = AppPayload(value[1:1 + aid_len], value[-1])
    elif record_type == RECORD_SUMMARY:
        if len(value) != 12:
            raise EmvTraceError("summary record payload must be exactly 12 bytes")
        payload = SummaryPayload(*struct.unpack("!III", value))
    else:
        raise EmvTraceError(f"unknown record type {record_type}")
    return EmvTraceRecord(
        record_type, sequence, stage, app_index, attempt, flags, status,
        timestamp_ms, payload, raw,
    )


def parse_get_response(data: BytesLike, expected_scan_id: Optional[int] = None,
                       expected_start: Optional[int] = None) -> EmvTracePage:
    raw = bytes(data)
    if len(raw) < 18:
        raise EmvTraceError(f"GET response is too short: {len(raw)} bytes")
    version, flags, scan_id, start_record, next_record, returned_count, records_bytes = \
        struct.unpack_from("!BBIIIHH", raw)
    if version != PROTOCOL_VERSION:
        raise EmvTraceError(f"unsupported GET version {version}")
    if flags & ~0x07 or bool(flags & 0x01) == bool(flags & 0x02):
        raise EmvTraceError(f"invalid GET flags 0x{flags:02x}")
    if expected_scan_id is not None and scan_id != expected_scan_id:
        raise EmvTraceError(
            f"GET session mismatch: expected {expected_scan_id}, got {scan_id}")
    if expected_start is not None and start_record != expected_start:
        raise EmvTraceError(
            f"GET cursor mismatch: expected {expected_start}, got {start_record}")
    if next_record < start_record or next_record - start_record != returned_count:
        raise EmvTraceError("GET next cursor disagrees with returned_count")
    if records_bytes != len(raw) - 18:
        raise EmvTraceError("GET records_bytes does not match response length")
    records = []
    offset = 18
    for _ in range(returned_count):
        if offset + 2 > len(raw):
            raise EmvTraceError("GET ended in a record length prefix")
        body_len = struct.unpack_from("!H", raw, offset)[0]
        end = offset + 2 + body_len
        if end > len(raw):
            raise EmvTraceError("GET contains a partial atomic record")
        records.append(parse_record(raw[offset:end]))
        offset = end
    if offset != len(raw):
        raise EmvTraceError("GET returned_count does not consume records_bytes exactly")
    return EmvTracePage(
        flags, scan_id, start_record, next_record, returned_count, records_bytes,
        tuple(records), raw[18:], raw,
    )


MetaResult = Union[BytesLike, EmvTraceMeta]
PageResult = Union[BytesLike, EmvTracePage]


def download_emv_trace(
        scan_id: int,
        fetch_meta: Callable[[int], MetaResult],
        fetch_page: Callable[[int, int, int], PageResult],
        max_payload: int = 4096,
        start: Optional[EmvTraceStart] = None) -> EmvTraceDownload:
    """Download one retained session and verify its complete stored stream."""
    _uint("scan_id", scan_id, 0xFFFFFFFF)
    encode_get_request(scan_id, 0, max_payload)
    if start is not None and start.scan_id != scan_id:
        raise EmvTraceError("START session does not match requested download")
    meta_result = fetch_meta(scan_id)
    if isinstance(meta_result, EmvTraceMeta):
        meta = parse_meta_response(meta_result.raw, scan_id)
        if meta != meta_result:
            raise EmvTraceError("META callback object disagrees with its raw response")
    else:
        meta = parse_meta_response(meta_result, scan_id)
    if meta.state not in (2, 3):
        raise EmvTraceError(f"META session is not terminal (state {meta.state})")
    if start is not None and (start.state != meta.state or start.flags != meta.flags):
        raise EmvTraceError("START and META state/flags disagree")

    pages = []
    records = []
    cursor = 0
    while cursor < meta.stored_records:
        page_result = fetch_page(scan_id, cursor, max_payload)
        if isinstance(page_result, EmvTracePage):
            page = parse_get_response(page_result.raw, scan_id, cursor)
            if page != page_result:
                raise EmvTraceError("GET callback object disagrees with its raw response")
        else:
            page = parse_get_response(page_result, scan_id, cursor)
        if len(page.raw) > max_payload:
            raise EmvTraceError("GET response exceeds requested max_payload")
        if bool(page.flags & 0x04) != bool(meta.flags & 0x04):
            raise EmvTraceError("GET and META truncation flags disagree")
        if page.returned_count == 0 or page.next_record <= cursor:
            raise EmvTraceError(f"GET made no progress at record {cursor}")
        if page.next_record > meta.stored_records:
            raise EmvTraceError("GET cursor exceeds META stored_records")
        if page.flags & 0x02 and page.next_record != meta.stored_records:
            raise EmvTraceError("GET marked a non-final page as final")
        if page.flags & 0x01 and page.next_record == meta.stored_records:
            raise EmvTraceError("GET marked the final page as having more records")
        pages.append(page)
        records.extend(page.records)
        cursor = page.next_record

    raw_records = b"".join(record.raw for record in records)
    if len(records) != meta.stored_records:
        raise EmvTraceError("downloaded record count differs from META")
    if len(raw_records) != meta.stored_bytes:
        raise EmvTraceError("downloaded byte count differs from META")
    if zlib.crc32(raw_records) & 0xFFFFFFFF != meta.crc32:
        raise EmvTraceError("downloaded record CRC32 differs from META")
    for previous, current in zip(records, records[1:]):
        if current.sequence <= previous.sequence:
            raise EmvTraceError("record sequence numbers are not strictly increasing")
    return EmvTraceDownload(
        scan_id, meta, tuple(pages), tuple(records), raw_records, start)


def _payload_json(payload: RecordPayload) -> Dict[str, Any]:
    if isinstance(payload, RfPayload):
        return {"direction": payload.direction, "bit_length": payload.bit_length,
                "data": payload.data.hex()}
    if isinstance(payload, ApduPayload):
        return {"sw": payload.sw, "command": payload.command.hex(),
                "response": payload.response.hex()}
    if isinstance(payload, AppPayload):
        return {"aid": payload.aid.hex(), "priority": payload.priority}
    return {"stored_records": payload.stored_records,
            "observed_records": payload.observed_records, "flags": payload.flags}


def trace_to_json(download: EmvTraceDownload) -> Dict[str, Any]:
    """Return a JSON-safe, lossless representation of a verified download."""
    meta = download.meta
    result: Dict[str, Any] = {
        "protocol_version": PROTOCOL_VERSION,
        "scan_id": download.scan_id,
        "meta": {
            "state": meta.state, "result_status": meta.result_status,
            "flags": meta.flags, "scan_id": meta.scan_id,
            "stored_records": meta.stored_records,
            "observed_records": meta.observed_records,
            "stored_bytes": meta.stored_bytes,
            "required_bytes": meta.required_bytes,
            "first_dropped": meta.first_dropped, "crc32": meta.crc32,
            "app_count": meta.app_count, "elapsed_ms": meta.elapsed_ms,
            "uid": meta.uid.hex(), "atqa": meta.atqa.hex(), "sak": meta.sak,
            "ats": meta.ats.hex(), "raw": meta.raw.hex(),
        },
        "pages": [{
            "flags": page.flags, "scan_id": page.scan_id,
            "start_record": page.start_record, "next_record": page.next_record,
            "returned_count": page.returned_count,
            "records_bytes": page.records_bytes, "raw": page.raw.hex(),
        } for page in download.pages],
        "records": [{
            "type": record.record_type, "sequence": record.sequence,
            "stage": record.stage, "app_index": record.app_index,
            "attempt": record.attempt, "flags": record.flags,
            "status": record.status, "timestamp_ms": record.timestamp_ms,
            "payload": _payload_json(record.payload), "raw": record.raw.hex(),
        } for record in download.records],
        "raw_records": download.raw_records.hex(),
    }
    if download.start is not None:
        result["start"] = {
            "state": download.start.state, "scan_id": download.start.scan_id,
            "flags": download.start.flags, "raw": download.start.raw.hex(),
        }
    return result
