# HF reader additions

Recent additions to the high-frequency (13.56 MHz) reader tooling and Python
command layer in `software/script/`. Sections with a CLI command show its syntax.

## Continuous ISO14443-A capture (commands 2021-2025)

Protocol v2 adds a retained 8 KiB firmware ring for long-running capture in tag
emulation, passive monitor, and active-reader trace modes. Every record carries
a sequence number, accumulated `app_timer` timestamp, direction, exact bit
length, representation/error flags, and payload. Metadata exposes observed,
buffered, and dropped counts plus an overflow flag; sequence numbers advance for
dropped records so loss cannot be mistaken for a clean trace.

The host retrieves versioned pages with an IEEE CRC32. `HF_CAPTURE_GET` applies
the previous page's acknowledgement before returning the next page, so clients
must validate and durably persist a page before acknowledging its final
sequence and 64-bit delivery token. The token makes duplicate ACKs idempotent
without confusing a reused 32-bit sequence after wrap. `HF_CAPTURE_STOP` stops RF acquisition without deleting unread data,
and `HF_CAPTURE_STATUS` rebinds a retained session to a reconnected USB or BLE
transport. Command 2025 is an unsolicited metadata notification used only to
wake a draining host; periodic GET remains the correctness fallback.

Metadata includes a random per-boot identifier and the non-zero 32-bit token supplied
by START. An exact START retry with the same mode, transport, and token returns
the existing session instead of creating another one. Flutter also binds each active
manifest to the device chip ID, persists complete wire pages with an additional
full-file CRC, validates every page before its first reconnect ACK, and drains
stopped sessions after reconnect. GET uses a separate ACK-present byte so all
32-bit sequence values remain acknowledgeable after wrap. A new START cannot
discard unread records, and storage failures stop acquisition before any unsafe
ACK. Before START, the GUI writes the token and transport type to a pending
manifest; if the response is lost, capability-checked `STATUS(0)` recovers only
the matching session on the same USB/BLE transport type. A link loss invalidates
the owner and suppresses events until STATUS presents that token. On iOS,
`bluetooth-central` permits eligible BLE background wakeups but
does not guarantee timer execution or process survival; retained firmware data
and explicit overflow counters remain the reconnect fallback.

Passive mode never transmits, but one Ultra can guarantee only reader-to-card
traffic because its NFCT front end cannot simultaneously receive an external
card's load-modulated response. Reader mode traces RF work initiated by reader
commands; START does not autonomously poll a card. DESFire/ISO-DEP traffic can be
captured and decoded, but ordinary traces do not reveal AES or 3DES keys.

## MIFARE Classic — fast sector read (`hf mf rdsc`)

`MF1_READ_BLOCKS` (command 2018) authenticates **once** to a sector and then
reads N consecutive blocks from it, instead of one authentication per block.
On a full 1K dump this cuts the block-read authentications from 64 to 16.

```
hf mf rdsc --blk 4 -a -k FFFFFFFFFFFF -c 4    # read the 4 blocks of sector 1
```
- `--blk` start block, `-a`/`-b` key A/B, `-k` key (12 hex), `-c` count (1–16).
- All requested blocks must be inside the sector the start block belongs to; if a
  read fails part-way the blocks read so far are returned.

`hf mf dump` and the `hf mf autopwn` dump path use this fast read internally,
falling back to per-block reads on any hiccup, so existing behaviour is preserved.

## MIFARE Classic — antenna field control (`hf 14a field`)

Turn the reader antenna field on or off directly (reader mode):
```
hf 14a field on
hf 14a field off
```
(`HF14A_SET_FIELD_ON`/`OFF`, commands 2100/2101.)

## ISO-DEP - persistent reader session (`hf 14a session`)

Commands `6011`-`6014` select one real ISO-DEP card once and preserve RF field,
ISO-DEP block numbers, chaining state, and ATS-derived timing across host APDU
round trips:

```text
hf 14a session start
hf 14a session start --express-transit
hf 14a session exchange <session-id> 00A404000E325041592E5359532E444446303100
hf 14a session stop <session-id>
```

Normal START uses command 6011. `--express-transit` uses command 6014 to send the
TfL Apple ECP2 frame before WUPA/SELECT/RATS; both variants return the same
metadata and continue with 6012/6013. ECP2 success means that ISO-DEP activation
succeeded, not that Wallet policy changed or a transaction was approved.

The session ID is a nonzero 32-bit token for the current boot. The session has no
inactivity timeout and is bound to the USB or BLE command transport that opened it;
commands from the other transport are rejected while it is active. Explicit STOP
or reset, a replacement START from the owner transport, RF failure, reader-mode
exit or another owner-safe invalidation, and loss of the owner USB/BLE link close
the session and power down the field.

A timed-out or otherwise uncertain 6012 EXCHANGE must never be retried. On the same
still-valid owner connection, an ordered empty 6013 response with status `0x68`,
`0x60`, or `0x66` confirms the old session is closed and lets a host clear only its
6012 quarantine.
Reconnect only if that reset cannot be confirmed or framing/transport was itself
invalidated. This is the backend used by the Android authorized payment HCE relay.
See the
[complete relay and protocol guide](authorized-iso-dep-relay.md).

## DESFire — fast enumeration (`hf des enum`)

`HF14A_4_DESFIRE_SCAN` (command 6006) performs the whole DESFire enumeration in a
**single firmware call** — no per-APDU USB round-trips, so no field drops between
steps. It returns the tag info plus the GetVersion / GetApplicationIDs /
per-application GetFileIDs exchanges, which the CLI decodes:
```
hf des enum
```
prints UID, ATQA/SAK/ATS, HW/SW version + generation (EV1/EV2/EV3/…) + storage +
protocol, production batch/week/year, and each application AID with its file IDs.

`hf des info` still exists and does the same enumeration the slower per-APDU way
(one command at a time); `hf des enum` is the fast one-shot equivalent.

## EMV — richer `emv scan`

`emv scan` is a multi-application EMV data-discovery and trace command. Its normal
mode selects PPSE, parses every advertised application template, selects every AID,
requests standard GET DATA objects, and performs bounded record discovery. It does
not run GPO by default because Visa-style GPO can generate a cryptogram and advance
card state.

Firmware that advertises commands 6007-6009 uses the retained paged protocol. The
CLI and GUI validate the session ID, cursor, atomic record lengths/counts, and CRC32,
then preserve every retained APDU and RF frame. RF records include direction, exact
bit length, PCB/control traffic, CRC bytes, transport status, and relative timing.
Metadata explicitly reports timeouts, transport failures, dropped RF detail, and
logical trace/response truncation. Older firmware falls back to bounded command 6005.

The explicit maximum-processing option processes every discovered AID in the same
RF session, builds the card's PDOL in its declared order, runs GPO, validates
format-1/format-2 AIP+AFL, reads AFL records, and issues GENERATE AC only when a
complete CDOL1 exists. Visa applications that already return cryptogram data from
GPO do not receive a generic GENERATE AC. This mode can advance ATC or other card
state even though the tool performs no issuer/bank communication.

Beyond PAN (with Luhn check), expiry, cardholder name, issuer country and application
label/name, decoding includes:

- **Effective date** (tag 5F25)
- **PAN sequence number** (5F34)
- **Application currency** (9F42, shown with the ISO-4217 name)
- **Language preference** (5F2D)
- **Application version** (9F08)
- **Application Interchange Profile** (82, decoded to the supported capabilities:
  SDA / DDA / CDA / cardholder verification / terminal risk management / issuer
  authentication)
- **Service code** (from Track 2)

```
emv scan                    # print to terminal
emv scan -f /tmp/card.json  # save PM3-compatible JSON
emv scan -s 3               # also load the scanned card into slot 3 for emulation
emv scan --maximum-processing --amount 1.00  # explicit state-changing processing
emv scan --maximum-processing --grid --max-aids 16
emv scan --rf --logs --budget-ms 30000 -f /tmp/lossless-trace.json
```

The GPO/CDOL builder fills terminal country/currency/date/type, amount, TTQ and a
fresh unpredictable number in the card-declared order. Unknown DOL objects are
zero-filled and malformed or oversized DOLs are rejected rather than partially
sent. Record discovery uses valid SFI 1..30 and configurable smart/grid limits.
Express Transit requests use the Apple-compatible Visa TTQ `33804000`, including
the ODA-for-online capability required by this mobile profile. The investigative
reader does not validate ODA or an issuer response, so this remains protocol
evidence rather than an approval result. Express requests also skip the generic
empty-PDOL fallback.

Extended START requests can select a bounded terminal profile: automatic,
Apple-transit `33804000`, online-without-ODA `32804000`, broad-mobile
`3600C000`, qVSDC-online `26804000`, minimal-online `22804000`, MSD+qVSDC
`B600C000`, or a caller-supplied TTQ. The compatibility sweep tries those six
named profiles in that order, reselecting the application between attempts. It
continues only for GPO status `6985`, `6986`, or `6A80`, and stops on success,
transport failure, or any other response. Trace APDUs retain every attempted GPO.
The DOL builder also supplies bounded values for amount-other, TVR, terminal and
additional capabilities, merchant category/identifier/name, transaction sequence,
and transaction category when an application requests them; unknown tags remain
zero-filled.

The extended request can additionally select fast, balanced, or patient ECP
polling, override the bounded retry/delay/timeout values, request scheme-adaptive
profile ordering, and cycle RF/re-run ECP before each rejected profile. Visa,
Mastercard/Maestro, American Express, Discover, JCB, UnionPay, and Interac are
classified by RID/AID. When explicitly enabled and PPSE is unavailable or empty,
firmware probes one fixed application AID for each of those schemes; it does not
enumerate arbitrary AIDs. Adaptive sweeps use Visa-, Mastercard-, or other-scheme
orders, continue only after `6985`, `6986`, or `6A80`, and remain bounded by six
profiles plus the request APDU/time limits. GUI reports retain each GPO command,
PDOL payload, recognized TTQ, status word, scheme, order, and successful attempt
for side-by-side comparison.

The GUI can instead coordinate six completely separate ECP/RF sessions, one fixed
profile per session with a field-off delay, and stop at the first successful GPO.
This is distinct from firmware's in-session sweep and is useful when a wallet
enters a retry state after rejecting GPO. Exported reports include every session.
Copied reports can be replayed offline: raw record framing, status duplication,
record stream equality, and CRC-32 are revalidated before the assessment is
recomputed. ODA diagnostics decode SDA/DDA/CDA support and inventory the issuer/
ICC certificate, exponent, signed-static-data, and CAPK-index tags. They remain
explicitly `cryptographicallyVerified: false` until a matching trusted CAPK and
full EMV signed-data reconstruction are available; tag presence or TTQ never
counts as ODA verification. Terminal presets now also provide no-CVM results,
floor limit, and profile-specific attended/unattended terminal type when requested
through PDOL/CDOL.
Transaction-log records are read only when requested and a valid `9F4D` Log Entry
is present. This is investigative tooling, not a certified EMV Level-2 kernel and
does not perform CVM, issuer authorization, issuer scripts, or cryptogram validation.

`emv load --defaults` and the GUI EMV emulator "Test card" preset load a
dummy, readable Mastercard-shaped test card: PPSE, SELECT AID, GPO format 1
(AIP+AFL), and one READ RECORD response with test PAN data. It is intended for
authorised terminal/UI testing only and does not represent a live account.
