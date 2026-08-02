# ISO-DEP and APDU command reference

This reference covers commands surfaced by the APDU Terminal, EMV trace workflow,
EMV emulator, synthetic relay-resistance lab, and authorized real-card relay. See
[Authorized ISO-DEP payment relay lab](authorized-iso-dep-relay.md) for the complete
setup, wire protocol, Android HCE configuration, timing, and operator procedure.
The Spanish byte-level datagram and recovery walkthrough is available in
[authorized-relay-datagrams-es.md](authorized-relay-datagrams-es.md).
For a detailed Spanish explanation of APDU, GPO, PDOL, ISO-DEP, WTX, and their
relay encapsulation, see [Guia de APDU, GPO y PDOL](apdu-gpo-guide-es.md).

## APDU structure

```text
CLA INS P1 P2 [Lc] [Data] [Le]
```

- **CLA**: command class/namespace
- **INS**: instruction
- **P1/P2**: instruction parameters
- **Lc**: command-data length
- **Data**: command payload
- **Le**: expected response length; `00` commonly means maximum short response

Responses are `[response data] SW1 SW2`; the final two bytes are the status word.

## Common EMV APDUs

| Name | Template/example | Meaning | State impact |
|---|---|---|---|
| SELECT PPSE | `00 A4 04 00 0E 325041592E5359532E4444463031 00` | Select payment directory | Normally discovery only |
| SELECT AID | `00 A4 04 00 Lc <AID> 00` | Select advertised application | Establishes application context |
| GPO | `80 A8 00 00 Lc 83 L <PDOL values> 00` | Supply terminal profile/start processing | Can advance state/return cryptogram data |
| READ RECORD | `00 B2 <record> <SFI-control> 00` | Read AFL/log record | Usually read-only |
| GET DATA | `80 CA <tag high> <tag low> 00` | Request ATC/counters/objects | Usually read-only |
| GET RESPONSE | `00 C0 00 00 <Le>` | Retrieve bytes announced by `61xx` | Continues previous command |
| GENERATE AC | `80 AE <type> 00 Lc <CDOL1 values> 00` | Request AAC, TC, or ARQC | State-changing; can advance ATC |

GENERATE AC `P1` high bits are `00` AAC, `40` TC, and `80` ARQC.

## Synthetic relay-lab APDUs

| Name | Template | Meaning |
|---|---|---|
| SELECT private AID | `00 A4 04 00 07 F0010203040506 00` | Enter isolated synthetic application |
| Challenge | `F0 10 00 00 08 <8-byte nonce>` | Test nonce binding/replay behavior |
| Data example | `F0 20 P1 P2 Lc <data>` | Arbitrary private payload |
| Status example | `F0 30 00 00 00` | Synthetic status request |
| Any private command | `F0 INS P1 P2 ...` | Forwarded after successful AID selection, up to 64 bytes |

No non-`F0` command is forwarded after selection. Selection clears when NFC
deactivates.

## Common status words

| SW | Meaning |
|---|---|
| `9000` | Success |
| `61xx` | More response bytes available |
| `6283` | Selected file invalidated |
| `6300` | Authentication failed/warning |
| `6700` | Wrong length |
| `6982` | Security status not satisfied |
| `6985` | Conditions of use not satisfied |
| `6986` | Command not allowed/no current EF |
| `6A80` | Incorrect command data |
| `6A81` | Function not supported |
| `6A82` | File/application not found |
| `6A83` | Record not found |
| `6A86` | Incorrect P1/P2 |
| `6Cxx` | Wrong Le; SW2 gives exact length |
| `6D00` | INS not supported |
| `6E00` | CLA not supported |
| `6F00` | Unspecified processing failure |

## Chameleon command IDs

These are host/device commands, not card APDUs.

| ID | Name | Meaning |
|---:|---|---|
| 6000 | `HF14A_4_APDU_RECV` | Poll APDU received by tag emulation |
| 6001 | `HF14A_4_APDU_SEND` | Supply host-generated emulation response |
| 6002 | `HF14A_4_SET_ANTI_COLL` | Set UID, ATQA, SAK, and ATS |
| 6003 | `HF14A_4_STATIC_RESP` | Clear/add APDU prefix-response rules |
| 6004 | `HF14A_4_READER_APDU` | Activate target and exchange one APDU |
| 6005 | `HF14A_4_EMV_SCAN` | Legacy packed EMV scan |
| 6006 | `HF14A_4_DESFIRE_SCAN` | Bounded DESFire enumeration |
| 6007 | `HF14A_4_EMV_TRACE_START` | Execute/retain versioned EMV trace |
| 6008 | `HF14A_4_EMV_TRACE_META` | Read retained metadata |
| 6009 | `HF14A_4_EMV_TRACE_GET` | Page retained records |
| 6010 | `HF14A_4_DEBUG_COUNTERS` | Read ISO-DEP emulator counters |
| 6011 | `HF14A_4_READER_SESSION_START` | Select/RATS once and open a real-card session |
| 6012 | `HF14A_4_READER_SESSION_EXCHANGE` | Exchange an APDU in the matching session |
| 6013 | `HF14A_4_READER_SESSION_STOP` | Deselect/close the matching session; ordered reset after uncertain 6012 |
| 6014 | `HF14A_4_READER_SESSION_START_APPLE_TRANSIT` | Send bounded Apple ECP2 polling, then open the same real-card session |

Command `6004` cycles RF and performs SELECT/RATS for every invocation. It supports
the stateless synthetic test, not a transparent stateful multi-APDU relay. Commands
`6011` and `6014` are alternate START commands; both preserve one real card's
ISO-DEP block state across host round trips through `6012` and `6013`. Each START
returns a nonzero big-endian session ID followed by UID/ATQA/SAK/ATS metadata;
`EXCHANGE` and normal `STOP` require that exact ID and reject stale IDs. The session
has no inactivity timeout. It belongs to the USB or BLE command transport that sent
START, and firmware rejects commands from the non-owner transport while it is
active. Explicit STOP/reset, an owner-transport replacement START, RF/ISO-DEP
failure, reader-mode exit or another owner-safe invalidation, and owner-link loss
turn off the field and invalidate the session.

A timed-out or uncertain `6012` must never be retried. If framing and the connection
remain valid, send `6013` in order with the uncertain session ID. An empty `6013`
response with outer status `0x68`, `0x60`, or `0x66` confirms that session is closed
and forms a response-ordering barrier, allowing the host to clear only the `6012`
quarantine and reuse the same connection. Reconnect only when this reset cannot be
confirmed or framing/transport has already been invalidated.

The Android authorized relay uses these commands as one armed session with no
overall lease; its 50..5000 ms native deadline applies separately to each terminal
APDU. It first selects PPSE directly on the backend card to discover its `4F`
application AIDs,
registers PPSE plus those AIDs with a separate payment-category HCE service, and
requires the operator to select CU GUI explicitly in Android's contactless-payment
settings. Transparent mode forwards terminal APDUs in order. The explicit Apple
Transit mode starts with 6014 and rewrites only `9F66`, `9F35`, and `9F33` inside
GPO tag `83`, based on the exact PDOL `9F38` from the selected AID; every card
response remains unchanged. Cleanup retains the last dynamic card AIDs and keeps
the HCE component mounted so CU GUI stays visible in payment settings while
unarmed taps fail closed with `6400`. Android does not restore the previous Wallet
automatically. During preparation, a clean START no-card response is retried after
500 ms until a card appears or the operator cancels; other RF/protocol errors are
not hidden by that loop. The operator may press Arm without running Prepare first;
the GUI performs the same required PPSE/AID preflight automatically and then
continues into the armed rendezvous. An explicit mobile-Wallet mode permits a
rotating backend UID only after exact ATQA/SAK/ATS and full PPSE-response
revalidation; normal physical-card mode still requires the prepared UID.
Backend S(WTX) handling accepts up to 64 requests but enforces a 5000 ms cumulative
WTX budget per APDU, so mobile Wallet processing is bounded without the older
eight-request premature abort. A timeout after the final command block or immediately
after WTX permits at most two ISO-DEP `R(NAK)` response-block retransmission requests
within that budget; the command APDU itself is never replayed.
This workflow is intended for owned cards and offline lab terminals; forwarded
commands can advance card state or produce transaction cryptograms. The complete
reference is [authorized-iso-dep-relay.md](authorized-iso-dep-relay.md).

## Selected EMV tags

| Tag | Meaning |
|---|---|
| `4F` | Application identifier |
| `50` | Application label |
| `57` | Track 2 equivalent data |
| `82` | Application Interchange Profile |
| `8C` | CDOL1 |
| `8F` | CA public-key index |
| `94` | Application File Locator |
| `95` | Terminal Verification Results |
| `9F02` | Authorized amount |
| `9F10` | Issuer Application Data |
| `9F26` | Application cryptogram |
| `9F27` | Cryptogram Information Data |
| `9F34` | CVM results |
| `9F36` | Application Transaction Counter |
| `9F37` | Unpredictable number |
| `9F38` | PDOL |
| `9F66` | Terminal Transaction Qualifiers |
| `9F6C` | Card Transaction Qualifiers |

## Interpretation

- `9000` means that APDU succeeded, not that an issuer approved payment.
- Wallet “Done” does not prove settlement.
- AIP/ODA tag presence does not prove cryptographic verification.
- Trace truncation, transport errors, and logical status are separate evidence.
