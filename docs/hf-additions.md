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

`emv scan` runs the full EMV sequence (PPSE → SELECT AID → GPO → READ RECORDs,
optionally GENERATE AC) in one firmware call and decodes the card's data
elements. The CLI and GUI now show a per-frame APDU trace (command name, raw
command/response, status word, and parsed response TLVs), including transaction
cryptogram fields when `GENERATE AC` succeeds. GPO format-1 responses (`80` =
AIP + AFL) are expanded so AIP/AFL decode the same way as format-2 `77`
responses. Beyond PAN (with Luhn check), expiry, cardholder name, issuer country
and application label/name, it now also gathers and displays:

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
emv scan --amount 1.00      # offline GENERATE AC simulation; no bank traffic
```

The firmware GPO builder follows the card's actual PDOL tag order (`9F38`) and
fills known terminal tags (`9F66`, `9F33`, `9F40`, `9F35`, `9F1A`, `5F2A`,
`9A`, `9C`, `95`, `9F37`, amount fields, and several Mastercard-specific
terminal fields) with a deterministic lab terminal profile. Fixed-length PDOL
guesses remain only as compatibility fallbacks. If the AFL does not expose PAN or
expiry data, the firmware falls back to a bounded nfc-frog-style sweep of SFI
1..31, records 1..16. This mirrors the approach used by public EMV
readers/kernels such as EMV-NFC-Paycard-Enrollment, nfc-frog, and OpenEMV
without vendoring their code.

`emv load --defaults` and the GUI EMV emulator "Test card" preset load a
dummy, readable Mastercard-shaped test card: PPSE, SELECT AID, GPO format 1
(AIP+AFL), and one READ RECORD response with test PAN data. It is intended for
authorised terminal/UI testing only and does not represent a live account.
