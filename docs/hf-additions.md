# HF reader additions

Recent additions to the high-frequency (13.56 MHz) reader tooling, reachable
from the Python CLI in `software/script/`.

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

The explicit maximum-processing option reactivates and processes every discovered
AID independently, builds the card's PDOL in its declared order, runs GPO, validates
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
Transaction-log records are read only when requested and a valid `9F4D` Log Entry
is present. This is investigative tooling, not a certified EMV Level-2 kernel and
does not perform CVM, issuer authorization, issuer scripts, or cryptogram validation.

`emv load --defaults` and the GUI EMV emulator "Test card" preset load a
dummy, readable Mastercard-shaped test card: PPSE, SELECT AID, GPO format 1
(AIP+AFL), and one READ RECORD response with test PAN data. It is intended for
authorised terminal/UI testing only and does not represent a live account.
