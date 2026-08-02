# Atomic active-slot snapshots

Command `ACTIVE_SLOT_SNAPSHOT` (`1050`) provides a transport-owned, versioned
transaction for reading and optionally persisting the exact active MIFARE Classic
HF dump. Ultra and Lite both advertise it through `GET_DEVICE_CAPABILITIES`
(`1035`). Clients must not infer support from a firmware version or optimistically
use it when `1035` is unavailable.

## Wire protocol

All multi-byte values are big-endian. Protocol version 2 defines three
operations:

| Operation | Request | Successful response |
|---|---|---|
| `BEGIN=0` | `version:u8=2, operation:u8=0` | `version=2, operation=0, slot:u8, tag_type:u16be, owner_generation:u32be, revision:u32be` |
| `SAVE_RELEASE=1` | `version=2, operation=1, revision:u32be` | Exact echo `version=2, operation=1, revision:u32be` |
| `ABORT=2` | `version=2, operation=2, revision:u32be` | Exact echo `version=2, operation=2, revision:u32be` |

The revision is nonzero and opaque. A client must return it unchanged and must
not derive ordering or age from its numeric value.

`owner_generation` is also nonzero. It identifies the currently loaded tag
owner and changes whenever firmware loads or reloads a tag buffer. It remains
stable across RF memory writes and volatile random-UID regeneration. Monitor
identity is `(slot, tag_type, owner_generation)`, never the anticollision UID.
Version 1 does not provide this identity and is unsupported; clients fail closed
rather than falling back to UID matching.

## Freeze semantics

`BEGIN` is accepted only in tag mode with an exact loaded active MIFARE Mini,
1K, 2K, or 4K HF owner. Firmware disables HF and LF field sensing before it
publishes the transaction. While frozen it blocks RF rearming, button actions,
sleep, slot/type/enable changes, factory/load operations, direct buffer writes,
mode changes, and ordinary saves.

The request transport is the owner. The other transport receives
`STATUS_DEVICE_MODE_ERROR` for every command while the transaction is active.
The owner may issue only:

- `1050` snapshot control;
- `4008` MIFARE dump reads with an exact two-byte request, count `1..32`, and a
  range inside the frozen type;
- `4009`, `4016`, and `4018` with empty requests.

Any other owner command receives `STATUS_DEVICE_MODE_ERROR`. A malformed allowed
read receives `STATUS_PAR_ERR`. A valid allowed read refreshes the 5-second idle
lease. The 120-second absolute lease never refreshes. Either lease expiring is an
implicit `ABORT`; sensing is restored without a flash write.

## Save and failure behavior

`SAVE_RELEASE` revalidates owner transport, revision, active slot, exact HF type,
loaded-buffer ownership, and Normal MIFARE write mode. It invokes only the active
HF dump FDS write, never slot configuration or LF persistence. The write is
forced even when the RAM CRC equals the persisted baseline. The baseline CRC is
updated only after FDS success.

Each synchronous FDS write, garbage collection, or retry waits at most 15
seconds. The bounded write/GC/write path is therefore at most 45 seconds, and the
snapshot commit reservation is 46 seconds. The absolute transaction lease is
120 seconds. Firmware rejects `SAVE_RELEASE` with `STATUS_CMD_ERR` before flash
work unless at least the full reservation remains. Once accepted, the transaction
is marked committing and ordinary lease expiry is suppressed until the bounded
call returns. GUI and Python clients use a dedicated 55-second response timeout.
A host timeout makes the save outcome uncertain, so clients disconnect instead
of retrying command 1050 on the same stream.

On FDS failure firmware returns `STATUS_FLASH_WRITE_FAIL`, keeps the transaction
frozen, and permits the same owner to retry `SAVE_RELEASE` with the same revision
or send `ABORT`. A successful save or abort releases the transaction and restores
the sensing state captured by `BEGIN`.

## GUI monitor

The automatic emulation-change monitor runs only when command `1050` is explicitly
present in the advertised capability list. Every poll performs `BEGIN`, reads the
returned slot/type and its exact dump while frozen, then:

- sends `ABORT` for a new baseline or an unchanged dump;
- sends `SAVE_RELEASE` for a changed dump;
- awaits and verifies durable local history storage after the release succeeds;
- publishes the new baseline and notification only after that storage succeeds;
- keeps a failed local write pending and retries it before any later device poll;
- attempts `ABORT` if a poll is cancelled or any read/save validation fails.

The monitor checks generation, communicator identity, connector identity, and
connection state before and after anticollision and every dump chunk. A cancelled
read cannot start a subsequent chunk. On a malformed successful `BEGIN`, clients
decode the fixed v2 revision first and attempt `ABORT` before rejecting later
metadata. If no revision is available, or cleanup cannot be confirmed, they
disconnect because firmware transaction state is uncertain.

The monitor never uses legacy `SLOT_DATA_CONFIG_SAVE` (`1009`). Manual legacy
slot operations retain their existing command behavior.

## Residual hardware validation

Host tests cover owner/revision/lease state, exact read allowlisting, FDS
failure/retry, forced writes, sensing restoration, strict host decoding, monitor
ordering, and cancellation. Release qualification still requires physical Ultra
and Lite tests for NFCT/LPCOMP quiescence, BLE/USB contention, field transitions,
FDS fault injection, disconnect lease expiry, and power-loss behavior during the
blocking FDS write.
