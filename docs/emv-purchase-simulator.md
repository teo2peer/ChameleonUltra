# EMV purchase and transit simulator internals

This document explains the investigative purchase/transit workflow implemented by
the ChameleonUltra firmware and GUI. It is not an EMV-certified payment kernel and
does not contact an acquirer, payment network, or issuer.

For a detailed Spanish explanation of APDU structure, GPO, PDOL, ISO-DEP, and WTX,
see [Guia de APDU, GPO y PDOL](apdu-gpo-guide-es.md).

## Roles

During a purchase-simulator run:

- ChameleonUltra is the ISO/IEC 14443-A reader and ISO-DEP terminal.
- The phone Wallet or test card is the contactless card application.
- The GUI builds the terminal request, downloads the retained trace, and explains
  the result.
- Firmware performs timing-sensitive RF and APDU work locally so USB/BLE latency
  does not interrupt the card session.

The relevant implementation is:

- `firmware/application/src/rfid/reader/hf/emv_trace.c`
- `firmware/application/src/rfid/reader/hf/iso_dep_reader.c`
- `software/script/emv_trace.py`
- GUI `lib/helpers/emv_trace.dart`, `lib/helpers/emv.dart`, and
  `lib/helpers/transit_gate.dart`

## 1. START request

The GUI sends command `6007` with a versioned request containing limits and
terminal data: trace options, processing mode, application/record/APDU/time limits,
amount, country, currency, date, transaction type, cryptogram type, and optional
terminal/polling profiles.

Firmware validates every bound before enabling RF. Processing runs synchronously;
commands `6008` and `6009` retrieve the retained result afterward.

## 2. Express Transit polling

When Express Transit is enabled, firmware keeps the RF field active and alternates
WUPA polling with the Apple ECP2 transit frame:

```text
6A 02 C8 01 00 03 00 02 79 00 00 00 00 C2 D8
```

ECP itself has no direct response. It lets a configured mobile Wallet expose its
selected Express credential under the phone's policy. The reader then waits for an
ordinary ISO14443-A response. Fast, balanced, and patient profiles change only
bounded timing values; they do not bypass Wallet policy.

## 3. ISO14443-A and ISO-DEP activation

Firmware performs WUPA/ATQA, anti-collision/SELECT for UID and SAK, RATS for ATS,
then initializes ISO-DEP. The ISO-DEP reader handles I-block chaining, R-ACK/R-NAK,
card WTX, sequence numbers, CRC validation, response reassembly, and DESELECT.
Logical APDUs are recorded separately from underlying RF frames.

## 4. Payment directory selection

The terminal selects the contactless payment directory:

```text
00 A4 04 00 0E 32 50 41 59 2E 53 59 53 2E 44 44 46 30 31 00
```

The data is ASCII `2PAY.SYS.DDF01`. A successful `9000` response normally contains
application templates (`61`) with AID (`4F`) and priority (`87`). An explicitly
enabled fallback can try a fixed list of known scheme AIDs when PPSE is unavailable
or empty; it does not enumerate arbitrary AIDs.

## 5. Application selection and PDOL

Each discovered AID is selected with:

```text
00 A4 04 00 <AID length> <AID> 00
```

The application FCI may contain PDOL (`9F38`), a list of requested terminal tags
and lengths. Firmware fills it in card-declared order. Supported objects include
TTQ, amounts, country, TVR, currency, date, transaction type, unpredictable number,
terminal capabilities/type, and merchant/category/name values. Unknown objects are
zero-filled; malformed or oversized DOLs are rejected rather than partly sent.

## 6. GET PROCESSING OPTIONS

PDOL values are wrapped in command template `83`:

```text
80 A8 00 00 Lc 83 L <PDOL values> 00
```

GPO can return AIP/AFL or transaction/cryptogram data and may advance card/Wallet
state even though no issuer is contacted. Format 1 uses tag `80` for AIP followed
by AFL; format 2 uses template `77` with tags such as `82`, `94`, and cryptogram
objects.

The compatibility sweep changes bounded terminal profiles only after `6985`,
`6986`, or `6A80`, and stops on success, transport failure, or another status.

## 7. Records and cryptograms

Firmware validates AFL and reads bounded records:

```text
00 B2 <record> <(SFI << 3) | 04> 00
```

Records can contain CDOL1 (`8C`). If GPO did not already return an appropriate
cryptogram and a complete CDOL1 exists, maximum processing may send:

```text
80 AE <AAC/TC/ARQC request> 00 Lc <CDOL1 values> 00
```

This is state-changing. Returned `9F26`, `9F27`, `9F36`, and `9F10` are evidence,
but the tool does not validate the cryptogram or send it to an issuer.

## 8. Why iPhone can display “Done”

Wallet controls its own UI. It can show “Done” after its local contactless
application completes the terminal interaction and produces or hands off the
expected transaction result. The animation does **not** prove issuer authorization,
acquirer acceptance, settlement, transport-operator acceptance, or back-office
fare/purchase completion.

The simulator reports cryptogram evidence, GPO status, CVM/CTQ, and trace
completeness without labeling the result an approved payment.

## 9. Retained evidence

Firmware stores typed RF, APDU, application, and summary records. Metadata includes
sequence/stage/application/attempt, observed/stored counts, required/stored bytes,
truncation flags, first dropped record, session ID, timing, and CRC-32.

Clients page records with `6009`, validate cursors and record boundaries, recompute
CRC-32, and preserve raw bytes. `61xx`/GET RESPONSE chains are folded only for
logical assessment; physical APDUs remain in the exported trace.

## 10. Difference from the relay labs

The purchase simulator is a terminal talking directly to a Wallet/payment
application. The relay-resistance lab is isolated to private AID
`F0010203040506`, private CLA `F0`, and synthetic endpoints. Consequently it can
report protocol/timing success but cannot trigger Wallet's payment animation or
claim payment completion.

The [authorized ISO-DEP payment relay](authorized-iso-dep-relay.md) is a third,
separate topology. Android presents payment-category HCE with PPSE and the physical
card's discovered AIDs, while ChameleonUltra holds one persistent reader session to
that card through normal START 6011 or Apple Transit START 6014, followed by
6012/6013. Transparent mode forwards terminal APDUs unchanged. The explicit Apple
Transit mode rewrites only approved GPO terminal-profile values from the selected
application's exact PDOL; all card responses remain unchanged. It can exercise real
card application state, but it remains an offline authorized lab tool and does not
prove issuer approval or settlement.
