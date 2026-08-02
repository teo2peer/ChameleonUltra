# Authorized ISO-DEP payment relay lab

For a byte-level Spanish walkthrough of every NFC-A, ISO-DEP, Chameleon, HCE and
EMV datagram, including the complete safe-recovery runbook, see
[Relay ISO-DEP autorizado: procesos, datagramas y recuperación](authorized-relay-datagrams-es.md).
For a conceptual Spanish introduction to APDU, GPO, PDOL, ISO-DEP, and WTX, see
[Guia de APDU, GPO y PDOL](apdu-gpo-guide-es.md).

## Scope

This document is the complete operator and protocol reference for the Android
payment-category HCE relay backed by a real ISO-DEP card on ChameleonUltra.

Use it only with:

- a card owned by the operator or explicitly authorized for the test;
- a terminal owned by the operator or explicitly authorized for the test;
- an offline laboratory transaction that cannot reach an acquirer, payment
  network, issuer, or production merchant system.

The software displays an authorization prompt, but it cannot prove ownership or
enforce that a terminal is offline. Those are operator responsibilities.

Forwarded commands are real card commands. They can advance counters, change
application state, or produce transaction cryptograms even when no issuer is
contacted. A `9000` response, a terminal success screen, or a Wallet animation is
not proof of authorization, clearing, or settlement.

## What this feature does

The feature keeps one physical card selected on the ChameleonUltra while an
Android phone presents a payment HCE endpoint to a terminal. Logical APDUs travel
through this path:

```text
Offline EMV terminal
       |
       | NFC-A / ISO-DEP
       v
Android NFC controller
       |
       | HostApduService
       v
AuthorizedRelayHostApduService
       |
       | EventChannel / MethodChannel
       v
CU GUI authorized relay page
       |
       | Chameleon binary command 6012 over USB or BLE
       v
ChameleonUltra ISO-DEP reader session
       |
       | NFC-A / ISO-DEP
       v
Owned or authorized physical card
```

The terminal sees Android as the card. The physical card sees ChameleonUltra as
the terminal. CU GUI forwards the terminal command APDU to the physical card and
returns the physical card response APDU to Android unchanged.

The relay supports a sequence of APDUs in one NFC activation. It is not the older
one-shot command `6004`, which reselects a card for every APDU.

## What this feature does not do

- It does not copy or clone a card.
- It does not store a reusable payment credential.
- It does not implement an EMV Level 2 kernel.
- It does not validate application cryptograms or contact an issuer.
- It does not guarantee that every terminal will tolerate the relay latency.
- It does not bypass card, terminal, CVM, issuer, or risk-management policy.
- It does not restore the previous Android wallet automatically.
- It does not make BLE equivalent to USB for timing-sensitive tests.
- It does not make a terminal-side WTX request directly from application code.

## Difference from the other EMV tools

| Tool | Terminal-facing endpoint | Backend | Purpose |
|---|---|---|---|
| EMV purchase/transit simulator | ChameleonUltra reader | Phone Wallet or test card | Investigate a card or Wallet as a terminal |
| Synthetic relay-resistance lab | Android HCE private AID `F0010203040506` | Synthetic endpoint | Test relay timing and policy without payment AIDs |
| Authorized ISO-DEP relay | Android payment HCE with PPSE/card AIDs | Real card held on ChameleonUltra | Authorized real-card relay interoperability test |

The synthetic lab uses category `other`, private CLA `F0`, and a private AID. It
cannot exercise normal payment-directory routing. The authorized relay is a
separate service in Android category `payment`.

## Requirements

### Hardware

- ChameleonUltra. Commands `6011`-`6014` are Ultra-only and are not available on
  ChameleonLite.
- An Android device with NFC Host Card Emulation.
- A physical ISO14443-A target that sets SAK bit `0x20` and returns at least two
  ATS bytes. Firmware does not fully validate ATS syntax or TL consistency.
- An EMV terminal that is both owned or explicitly authorized for testing and
  isolated from every production acquirer, payment-network, issuer, and merchant
  path.
- USB is strongly recommended for the first validation. BLE adds another radio,
  scheduling, and transport round trip to every APDU.

### Firmware

For transparent mode the Ultra firmware must advertise:

- `6011 HF14A_4_READER_SESSION_START`
- `6012 HF14A_4_READER_SESSION_EXCHANGE`
- `6013 HF14A_4_READER_SESSION_STOP`

Apple Transit mode replaces 6011 with:

- `6014 HF14A_4_READER_SESSION_START_APPLE_TRANSIT`

CU GUI checks the advertised capability list during preparation and refuses to
continue if any command is absent.

At connection time, current firmware reports its command list with
`GET_DEVICE_CAPABILITIES` (`1035`) and the communicator caches it. The authorized
relay requires affirmative advertised support for the selected START plus 6012
and 6013. If an explicit list omits one, or capability support is legacy/unknown and
`supportsCommand()` returns no affirmative result, preparation stops before RF
work. Other generic communicator calls can permit legacy-unknown commands, but this
workflow does not.

### Explicit Apple Transit mode

The GUI defaults to transparent mode. Apple Transit is a confirmed opt-in stored
locally in GUI preferences; it remains selected across relay sessions, page
recreation, and app restarts until the operator disables it. It changes two
independent parts of the backend path:

1. START uses command 6014, which sends the fixed TfL ECP2 polling frame before
   ordinary NFC-A activation.
2. After a terminal SELECT AID succeeds, Flutter strictly parses exactly one
   matching primitive `84` and one PDOL `9F38` from that application's FCI. A
   subsequent short GPO is rebuilt in place with `9F66=33804000`, optional
   `9F35=14`, and optional `9F33=E00800` at their exact PDOL offsets.

`9F66` must be present with length four. If `9F35` or `9F33` is requested, its
length must respectively be one or three. Duplicate fields, malformed BER-TLV,
missing/stale PDOL, extended GPO, or a tag-83 length mismatch stop the relay and
return synthetic `6400` without sending that GPO to the backend. Amount, currency,
country, date, transaction type, unpredictable number, every other command byte,
and every backend response remain unchanged. Closing a relay clears its prepared
card/session state but retains the selected backend profile for the next preparation.
This preference is not transferred by GUI data sync and never restores prepared
AIDs, arm tokens, pending APDUs, or a firmware session.

Build the application firmware from the repository root with:

```bash
make -j -C firmware/application
```

The resulting application image is normally:

```text
firmware/objects/application.hex
```

For the first enlarged-bootloader migration, build and flash the full package over
USB DFU:

```bash
./flash_firmware.sh --full
```

After a successful migration, the helper records `~/.chameleon/migrated`, and a
bare `./flash_firmware.sh` performs an app-only update. App-only DFU must not be used
before that migration. `ALLOW_APP_ONLY_DFU=1` is an explicit override for a device
already known to be migrated when the local marker is absent.

To prepare the same signed app-only DFU without opening a device or changing USB/BLE
state, run `./flash_firmware.sh --package-only`. The package is written to
`firmware/objects/chameleon-dfu-app.zip` with the same monotonic version handling as
the normal flash path.

### Android application

Build and install a debug APK with:

```bash
cd ../ChameleonUltraGUI/chameleonultragui
flutter build apk --debug
adb install -r build/app/outputs/flutter-apk/app-debug.apk
```

The authorized HCE service is declared by:

```text
android/app/src/main/AndroidManifest.xml
android/app/src/main/res/xml/authorized_relay_apdu_service.xml
```

The application requests NFC permission and declares HCE as an optional hardware
feature. The relay page reports HCE as unavailable on a device without it.

## Android payment-service configuration

### Why Android requires a default payment service

PPSE is the standard payment directory AID:

```text
325041592E5359532E4444463031
```

Its ASCII value is:

```text
2PAY.SYS.DDF01
```

Google Pay, another Wallet, and CU GUI can all claim PPSE in category `payment`.
Android permits only the selected contactless payment service to receive this
traffic. CU GUI cannot silently make itself the default; the user must select it
in system settings.

### Permanent component and temporary card AIDs

`AuthorizedRelayHostApduService` is always mounted and enabled. It remains visible
in Android's contactless-payment service list after preparation, disarm, NFC
deactivation, a deadline, or application restart.

It has one static payment AID:

```text
PPSE = 325041592E5359532E4444463031
```

Preparation registers one dynamic payment group containing:

1. PPSE.
2. Every valid payment AID discovered in the physical card's PPSE response.

The dynamic group replaces the static group and is retained after disarm, NFC
deactivation, application restart, and process recreation. Therefore CU GUI and
the last discovered card AIDs remain selectable without reacquiring the card; the
unarmed service still fails closed with `6400`.

When CU GUI is the default but the relay is not armed, a payment tap reaches the
service and receives `6400`. Normal Wallet payments will not be routed to Google
Pay or another Wallet until the operator selects that Wallet again.

### Selecting CU GUI as the default

Use the in-app path first:

1. Open `Ethical Hacking`.
2. Open `Diagnostics`.
3. Open `Authorized ISO-DEP relay lab`.
4. Press `Payment settings`.
5. Select `CU GUI` or `Authorized payment card relay` as the default contactless
   payment service.
6. Return to CU GUI.
7. Confirm that `CU GUI is contactless default` has a green status icon.

Android menu names vary by vendor. Common manual paths include:

```text
Settings > Connected devices > Connection preferences > NFC > Contactless payments
Settings > Connections > NFC and contactless payments > Contactless payments
Settings > Connection & sharing > NFC > Contactless payments
```

Then open `Default payment app`, `Payment default`, or the equivalent vendor menu
and select CU GUI.

Because the component has a static PPSE and is always mounted, CU GUI can be
selected before preparing a card. The documented workflow prepares first so the
same screen can immediately verify both the dynamic AIDs and default-payment state.
If CU GUI is selected while unprepared or disarmed, payment APDUs fail closed with
`6400`.

The app tries `ACTION_NFC_PAYMENT_SETTINGS` first and falls back to generic NFC
settings. It does not use a hidden API, edit the secure setting, or restore the
previous component.

### Restoring the normal Wallet

After the laboratory run:

1. Open `Payment settings` from CU GUI or Android Settings.
2. Select Google Pay, Google Wallet, or the normal payment application.
3. Confirm the selected Wallet in Android Settings before using contactless
   payments outside the lab.

Disarming the relay does not perform this step automatically.

### ADB verification

The following commands are diagnostic only. They do not replace the user-facing
selection workflow.

List installed HCE services:

```bash
adb shell cmd package query-services \
  -a android.nfc.cardemulation.action.HOST_APDU_SERVICE --components
```

The output should include:

```text
io.chameleon.ultra/.AuthorizedRelayHostApduService
```

Inspect the component override state:

```bash
adb shell dumpsys package io.chameleon.ultra
```

The service should not be in `disabledComponents`. Builds containing the permanent
component explicitly place it in `enabledComponents` when Flutter starts.

On Android versions that expose the setting under this key:

```bash
adb shell settings get secure nfc_payment_default_component
```

Expected while CU GUI is selected:

```text
io.chameleon.ultra/io.chameleon.ultra.AuthorizedRelayHostApduService
```

Inspect NFC routing and the AID cache:

```bash
adb shell dumpsys nfc
```

While prepared and selected, the dump should show:

- the current preferred payment service as `AuthorizedRelayHostApduService`;
- PPSE routed to that service;
- the card-specific payment AIDs routed to that service;
- the service bound in the payment category during an activation.

If an old build disabled the component and Android retains a stale NFC cache,
install the updated APK, launch CU GUI once, and toggle NFC off/on from Android
Settings. A development shell can also restart NFC:

```bash
adb shell svc nfc disable
adb shell svc nfc enable
```

That command is vendor-dependent and should only be used during development.

## Complete operator procedure

### 1. Prepare the environment

1. Disconnect the terminal from Ethernet, Wi-Fi, cellular, or any production
   acquirer path.
2. Confirm that the card and terminal are owned or explicitly authorized.
3. Power the ChameleonUltra and connect it to CU GUI.
4. Prefer a USB connection for the first run.
5. Enable NFC on Android.
6. Unlock Android and keep it unlocked.
7. Keep CU GUI open on the authorized relay page.

The Flutter page owns the event listener. Navigating away disposes the page and
starts HCE disable and ordered firmware session reset while retaining the dynamic
AIDs. Page disposal cannot await completion, but the attached cleanup continuation
still resets a session returned by an in-flight START. Explicit disarm reports
native disable and reset failures instead of claiming successful cleanup.

Merely backgrounding the app, opening Android Settings, or turning the screen off
does not dispose the page or automatically clear AIDs. Lock, NFC, and default-
service checks fail closed if an APDU reaches the service. Readiness rows refresh
when the app resumes, but prepared and armed state does not automatically clear.
Those flags remain until explicit disarm, native deactivation/deadline, disconnect
while armed, or another cleanup path.

### 2. Place the backend card

The card can be absent when `Prepare backend card` is first pressed. Place it on
the ChameleonUltra HF antenna while CU GUI is waiting. Once detected, keep it
motionless through the rest of preparation, arming, and the entire terminal
interaction.

Only one ISO-DEP backend session exists globally. Other HF reader operations,
manual field changes, raw commands, or a reader-mode exit invalidate it.

### 3. Prepare the backend card manually (optional)

Press `Prepare backend card`.

This manual step is optional. If the backend has not been prepared when the user
enables `Arm one terminal-facing relay session`, CU GUI runs this complete preflight
automatically and continues into arming after it succeeds. The preflight cannot be
removed internally because Android must know PPSE and the card's application AIDs
before it can route terminal SELECT AID commands to the HCE service.

The first press on that page shows an authorization dialog. Confirm only after the
offline and authorization conditions are satisfied.

Preparation performs this exact sequence:

1. Verify that the Ultra advertises 6012, 6013, and the selected START command.
2. Switch the Ultra to reader mode if necessary.
3. Pause GUI emulated-tag change monitoring to prevent competing commands.
4. Send command 6011 in transparent mode or 6014 in Apple Transit mode.
5. If firmware returns `STATUS_HF_TAG_NO` (`0x01`), wait 500 ms and retry START.
   Continue until a card is detected, the operator presses `Cancel waiting`, the
   Chameleon disconnects, or a non-no-card error occurs.
6. Keep a successfully detected card selected and verify nonzero session ID, UID
   length, SAK ISO-DEP bit, and ATS length.
7. Send this PPSE APDU through command `6012`:

   ```text
   00 A4 04 00 0E 325041592E5359532E4444463031 00
   ```

8. Require the card response to end in `9000`.
9. Parse BER-TLV recursively and collect tag `4F` only when it is inside an
   application template `61`.
10. Reject malformed TLV, excessive nesting, truncated lengths, invalid AID length,
   or an empty AID set.
11. Send command `6013` to stop the temporary preflight session.
12. Register PPSE plus the discovered AIDs as Android dynamic payment AIDs.
13. Display the backend UID and discovered application AIDs.
14. Resume the emulation-change monitor.

Only a clean no-card result is retried. Collision, CRC, parity, ATS, mode,
capability, transport, and malformed-response errors stop preparation and remain
visible for diagnosis. The delay is added after each completed no-card response;
actual polling cadence also includes the firmware scan and transport time.

START uses a six-second host timeout. Firmware can perform two complete scans, and
a partially coupled card can consume several 200 ms RF waits across anticollision,
up to three UID cascade levels, SELECT, and RATS. The previous three-second host
timeout could expire before that bounded scan sequence returned.

`Cancel waiting` prevents another retry. If a START command is already in flight,
the GUI waits for that command to return and stops any session it opened before
finishing cancellation. Continuous waiting ends as soon as a card is detected; it
does not monitor for later removal and cannot preserve an active ISO-DEP transaction
across card removal.

Preparation is discovery only in the current GUI: it sends SELECT PPSE and does not
send GPO or GENERATE AC. Later terminal APDUs can be state-changing. They are
forwarded unchanged in transparent mode; Apple Transit mode has only the bounded
GPO exception described above. Backend R-APDUs are always forwarded unchanged.

The parser accepts AIDs from 5 through 16 bytes. This implementation permits at
most 32 unique entries in the dynamic group, including PPSE, so the card may
contribute at most 31 distinct application AIDs.

### 4. Select CU GUI for contactless payments

If readiness reports that CU GUI is not the contactless default:

1. Press `Payment settings`.
2. Select CU GUI.
3. Return to the relay page.
4. Wait for the readiness indicators to refresh.

Preparation remains valid while Android opens Settings as long as the application
process remains alive. The native bridge distinguishes `prepared` from `armed`, so
Android creating the HCE service during selection does not remove it or clear a
valid preparation.

If Android kills the application process while Settings is open, the in-memory
prepared state is lost. Return to CU GUI and prepare the card again.

### 5. Configure the terminal response deadline

The default is:

```text
1000 ms per terminal APDU
```

The accepted range is:

```text
50..5000 ms
```

This is a wall-clock watchdog for each terminal APDU. It begins when the Android
service accepts the APDU. It is not the total NFC-session duration and is not reset
by backend-card WTX.

Recommended starting points:

| Transport/terminal | Initial deadline |
|---|---:|
| USB, ordinary lab reader | 1000 ms |
| USB, phone/terminal pair empirically tolerant of delayed HCE responses | 1500-3000 ms |
| BLE exploratory test | 3000-5000 ms |

A larger value does not force the terminal to wait. The terminal or its EMV kernel
can impose a shorter frame, APDU, or transaction limit.
Increasing this watchdog does not cause Android to issue WTX. Terminal WTX support
alone does not prove that a particular Android NFC controller will generate WTX or
that the complete phone/terminal combination will wait.

### 6. Arm one terminal-facing session

Enable `Arm one terminal-facing relay session`.

The switch remains available when no backend card has been prepared. In that case,
it first enters the same cancellable continuous card wait described in section 3,
discovers/registers the payment AIDs, and then proceeds automatically. The separate
`Prepare backend card` button remains useful when the operator wants to inspect UID
and AIDs before arming.

If the backend presented to ChameleonUltra is a phone Wallet rather than a physical
card, enable `Mobile Wallet backend (rotating UID)` before arming. Mobile NFC stacks
commonly generate a new anticollision UID for every RF activation, so UID equality
alone would reject the same phone after the preparation field cycle. This option is
for the backend Wallet phone; it is unrelated to whether the terminal itself is a
mobile SoftPOS device.

Arming performs:

1. Run the preparation preflight first if no prepared backend exists.
2. Recheck HCE support, NFC enabled, Android unlocked, and CU GUI selected as the
   default payment service.
3. Pause emulation-change monitoring.
4. Recheck that the current communicator advertises 6012, 6013, and the selected
   START command (6011 transparent or 6014 Apple Transit), and
   switch it to reader mode if necessary.
5. Allocate a process-static positive arm token from the Android bridge and enable
   native HCE forwarding before requiring the backend card to be present.
6. Start cancellable selected-START polling in the background while HCE waits for
   the first terminal APDU.
7. Compare every acquired backend UID with the UID observed during preparation. If
   rotating-UID mode is explicitly enabled and UID differs, SELECT PPSE again and
   require exact ATQA, SAK, ATS, and complete PPSE-response equality.
8. Verify that Android still reports a dynamic group containing PPSE and at least
   one non-PPSE AID.
9. Enter the armed rendezvous state. The card or terminal may now arrive first.

Normal mode requires exact UID equality and does not add another APDU. Rotating-UID
mode never accepts UID difference by itself: it sends an additional SELECT PPSE in
the acquired session, validates the response as well-formed, and compares exact
ATQA, SAK, ATS, and full PPSE response bytes with preparation. Any mismatch closes
the session. If the terminal arrived first, CU GUI confirms the token-bound native
APDU is still pending before sending this extra PPSE and checks it again before the
terminal APDU exchange. This is a strict continuity fingerprint, not cryptographic
proof that the same Wallet credential is present; multiple identically configured
Wallets can still expose the same metadata. Keep control of the authorized backend
device.

The additional SELECT PPSE response belongs to the newly acquired live firmware
session. CU GUI retains it for exactly the first terminal APDU: if that APDU is the
same SELECT PPSE, the exact backend bytes are delivered without sending the command
twice. The entry is consumed even on a mismatch and is never carried into another
session. This leaves both terminal and backend in the state produced by one PPSE
selection while preserving the card response byte for byte.

If the card arrives first, CU GUI holds the same backend session while waiting for
the terminal. Neither firmware nor GUI/native arm has an inactivity lease or overall
expiry, so waiting alone does not trigger a replacement START. The GUI no longer
recycles card-first sessions. Keep the owner USB/BLE link and backend RF coupling
stable; explicit cleanup, RF/mode invalidation, or owner-link loss closes the
session.

If the terminal arrives first, native Android retains exactly one APDU under the
configured 50..5000 ms watchdog while START polling waits for the card. Acquisition,
identity validation, START, and EXCHANGE must all finish before the native and
terminal deadlines. Rotating-UID mode adds one backend SELECT PPSE before the
terminal APDU, so phone-first timing is tighter. Android HCE cannot force the
terminal to issue or honor WTX. Terminal arrival wakes an active inter-attempt card
poll delay immediately; it does not retry a timed-out, malformed, or otherwise
uncertain START.

### 7. Present Android to the terminal

Present the Android NFC antenna to the terminal, not the ChameleonUltra. Keep:

- Android unlocked;
- CU GUI running on the relay page;
- the Ultra connected;
- the backend card stable on the Ultra antenna.

For each terminal command:

1. Android receives a logical command APDU in `processCommandApdu()`.
2. Native code validates length, armed token, relay state, and pending state.
3. Native code allocates an APDU request ID and starts the per-APDU watchdog.
4. Native code emits the APDU to Flutter and returns `null` for asynchronous HCE
   completion.
5. Flutter asks native Android whether the matching arm token and request ID are
   still pending before sending `session_id || APDU` with command `6012`.
6. Ultra wraps the APDU in ISO-DEP I-blocks and communicates with the physical card.
7. Ultra reassembles the logical card response and returns it to Flutter.
8. Flutter returns the response to the matching native request ID.
9. Android sends the response APDU to the terminal with `sendResponseApdu()`.

Only one terminal APDU may be pending. A second APDU received before the first is
answered receives `6985`.

### 8. End the session

Normal NFC deactivation disarms the relay. Cleanup also occurs after a watchdog,
backend error, user disarm, or page disposal. A Chameleon disconnect automatically
starts cleanup while the relay is armed or prepared. Dynamic AIDs remain registered
independently of the live backend state.

Normal awaited cleanup performs:

1. Disable native forwarding.
2. Answer a pending terminal APDU with `6400`, if necessary.
3. Send command `6013` for ordered ISO-DEP session reset and field shutdown.
4. Clear the live backend capability and prepared card metadata.
5. Resume emulation-change monitoring.
6. Keep the dynamic payment AID group and last discovered AID list registered.
7. Keep `AuthorizedRelayHostApduService` mounted and visible.
8. Keep the user's Android default-payment selection unchanged.

If `6012` became uncertain while cleanup started, step 3 uses the ordered 6013
reset path rather than retrying the APDU. An empty 6013 response with status `0x68`,
`0x60`, or `0x66` confirms closure and lets CU GUI reuse the same connection after
clearing only the 6012 quarantine. If reset cannot be confirmed, or framing/write/
transport was invalidated, cleanup disconnects and a new connection is required.

After cleanup, prepare again before another run. Manually restore the normal Wallet
when the authorized laboratory session is complete.

## State machines

### GUI and Android state

| State | Backend session | Dynamic AIDs | HCE forwarding | Transition |
|---|---|---|---|---|
| Idle | None | Normally absent | Disabled | Initial state or cleanup complete |
| Preparing | Repeated START, then temporary session | Being discovered | Disabled | `Prepare backend card`; waits through no-card responses |
| Prepared | None | PPSE plus card AIDs | Disabled | PPSE parse and registration succeed |
| Arming | None | Present | Enabling with arm token | Revalidate firmware, mode, and Android readiness |
| Waiting for both | None | Present | Enabled | START polling and HCE wait concurrently |
| Waiting for terminal | Active | Present | Enabled | Card arrived first; same session waits without an overall lease |
| Waiting for backend | Starting | Present | One APDU pending | Terminal arrived first; native watchdog is running |
| Exchanging | Active | Present | One APDU pending | Both sides joined; exactly one `6012` |
| Cleanup | Stopping | Removing | Disabled | Deactivation, error, deadline, disarm, disposal |

Every arm, native event, pending APDU, response, and cleanup carries the same
positive arm token. Android allocates these tokens monotonically in the
process-static bridge, so recreating a Flutter engine does not restart the token
sequence. Events and cleanup from an older arm cannot consume an APDU or disable a
newer arm. The backend state also retains the exact communicator and USB/BLE
transport that opened it; communicator replacement is an invalidation, not a
transport handoff.

There is no persisted prepared or armed state. Process death resets the native
in-memory bridge. The service remains installed and visible, but a new preparation
is required.

### Firmware session state

```text
INACTIVE
   |
   | selected 6011/6014 START succeeds
   v
ACTIVE(session_id, owner transport, ISO-DEP block state)
   |                 |                  |
   | 6012 success    | 6013/reset       | owner START/RF error/
   | keep state      | DESELECT         | mode/invalidation/link loss
   +-----------------+------------------+
                     v
                  INACTIVE
```

Only one global session exists. While it is active, the command transport that sent
START owns it and every command from the other USB/BLE transport is rejected with
`STATUS_DEVICE_MODE_ERROR`. A replacement START is therefore accepted only from the
owner transport. It invalidates the old session before trying to activate the
replacement card; if replacement activation fails, the old session is not restored.

Session IDs come from an incrementing nonzero 32-bit counter initialized at
firmware boot. Allocation skips zero and wraps to `1` after `0xFFFFFFFF`, so an ID
could repeat during an extremely long boot. IDs are not random, are not
credentials, and are not bound to UID or a host process. Session ownership is bound
separately to the USB or BLE command transport that opened it.

## Chameleon binary frame

Commands `6011`-`6014` use the normal Chameleon request/response frame over USB CDC
or BLE NUS:

```text
Offset  Size  Field
0       1     SOF = 0x11
1       1     LRC1
2       2     command, unsigned big-endian
4       2     status, unsigned big-endian
6       2     payload length, unsigned big-endian
8       1     LRC2
9       N     payload
9+N     1     LRC3
```

Total frame size is `10 + N`. Generic payload capacity is 4096 bytes, although the
session commands impose smaller bounds.

Each LRC is the 8-bit two's complement of the byte sum for its protected region:

```text
LRC = (0x100 - (sum(bytes) & 0xFF)) & 0xFF
```

`LRC1` protects SOF, `LRC2` protects the preamble through payload length, and
`LRC3` protects the payload. Because a valid preamble sums to zero modulo 256,
implementations may calculate the final LRC over the accumulated frame and obtain
the same value.

Request status is normally zero. Response status is independent of the APDU's
SW1/SW2. All multi-byte values described below are big-endian.

The outer protocol has no transaction or sequence ID. CU GUI serializes commands
and matches a response by the one active command ID. After a response timeout it
quarantines that command ID. A timed-out or otherwise uncertain `6012` must never be
retried because the physical card may already have processed the APDU and advanced
ISO-DEP or EMV state.

If the communicator has intact framing and remains writable, the authorized relay
instead queues `6013` with the uncertain session ID. USB and BLE preserve response
order: receipt of the empty 6013 response means every earlier 6012 response has
already been consumed or made obsolete. Status `0x68` means STOP closed the matching
session; `0x60` means it was already inactive after the uncertain exchange; `0x66`
means reader-mode invalidation closed it. In this ordered reset context, all three
confirm closure and form a barrier. The GUI then clears only the 6012 quarantine;
other uncertain command IDs remain quarantined, and the same connection can open a
new session.

If 6013 itself times out, has malformed payload, or returns another status, reset is
not confirmed and the host reconnects. Reconnection is also required when a partial
frame, write failure, or transport failure has already invalidated the communicator.
An uncertain START cannot use this reset path without a known session ID and is not
retried automatically.

### Handler validation order

The active-session transport-owner gate runs before command dispatch, so a
non-owner START, EXCHANGE, STOP, or unrelated command returns
`STATUS_DEVICE_MODE_ERROR` without reaching a handler or disturbing the session.
For the owner transport, both START commands are reader-mode-gated before their
empty-payload check. EXCHANGE and STOP check
payload length before checking mode. Therefore a malformed EXCHANGE or STOP in the
wrong mode returns `STATUS_PAR_ERR`; only a correctly sized request reaches the
mode check and aborts with `STATUS_DEVICE_MODE_ERROR`.

## Command 6011: session START

### Request

```text
Command: 6011 / 0x177B
Payload length: 0
```

A nonempty payload returns `STATUS_PAR_ERR`. In reader mode, an owner-transport
START first aborts an existing session, so even a malformed nonempty owner START
invalidates the old session. A non-owner START is rejected by the ownership gate
before this behavior.

### Successful response

Command status:

```text
STATUS_HF_TAG_OK = 0x0000
```

Payload:

```text
Offset          Size       Field
0               4          session_id, nonzero uint32
4               1          uid_len: 4, 7, or 10
5               uid_len    UID
5+uid_len       2          ATQA in reader receive order
7+uid_len       1          SAK
8+uid_len       1          ats_len
9+uid_len       ats_len    ATS without RF CRC
```

Total response payload length is:

```text
9 + uid_len + ats_len
```

The client requires SAK bit `0x20` and at least two ATS bytes.

### Activation failure response

The command status is the HF failure status and the diagnostic payload is:

```text
Offset  Size  Field
0       1     phase: 0x01 scan/select, 0x02 ATS/ISO-DEP qualification
1       1     HF status low byte
```

Parameter and mode errors have no diagnostic payload.

### Activation sequence

Firmware performs:

1. Abort prior session.
2. Antenna off.
3. Wait 5 ms.
4. Reset RC522.
5. Antenna on.
6. Wait 8 ms.
7. Temporarily use a 200 ms scan timeout.
8. Run the automatic scan, which performs WUPA, anticollision, SELECT, and RATS,
   then retries that complete scan once if the first attempt fails.
9. Restore the prior reader timeout.
10. Require SAK bit `0x20` and at least two ATS bytes; this is not full ATS syntax
    validation.
11. Initialize retained ISO-DEP sequence state.
12. Allocate a session ID and record the requesting command transport as owner.

The RATS request uses FSDI 4, FSD 48, and CID 0.

## Command 6014: Apple Transit session START

### Request

```text
Command: 6014 / 0x177E
Payload length: 0
```

Request validation, response metadata, diagnostic payloads, session token,
transport ownership, EXCHANGE behavior, and STOP behavior are identical to command
6011. Before the normal WUPA/anticollision/SELECT/RATS activation, firmware
temporarily configures:

```text
ECP2 frame: 6A 02 C8 01 00 03 00 02 79 00 00 00 00 C2 D8
retries:    30
delay:      5 ms
timeout:    2 ms
```

The RC522 polling path alternates the annotation with WUPA attempts. Firmware
clears the annotation after success, no-card, collision, ATS failure, or internal
setup failure, and exact prior reader timeout is restored. A successful 6014
response proves only that an ISO-DEP target activated; it does not prove ECP2
caused Wallet presentation, that the credential is transit-enabled, or that a
transaction was approved.

## Command 6012: session EXCHANGE

### Request

```text
Command: 6012 / 0x177C
Payload length: 5..516
```

Payload:

```text
Offset  Size    Field
0       4       session_id, nonzero uint32
4       1..512  logical command APDU
```

Do not include ISO-DEP PCB, CID, NAD, chaining bytes, or CRC. Firmware owns the
ISO-DEP transport layer.

The 512-byte limit is a command-layer bound. Successful RF transmission is also
limited by card FSC and the command-chain guard. With CID disabled, each I-block
carries at most `FSC - 3` APDU bytes, with at most 32 intermediate chained blocks
plus one final block. For example, an FSC of 16 limits a command to 429 bytes even
though the handler accepts up to 512.

### Successful response

Command status:

```text
STATUS_HF_TAG_OK = 0x0000
```

Payload:

```text
2..512 bytes of logical response APDU, including SW1/SW2
```

Card status words such as `6A82` are still successful command-transport responses.
The outer Chameleon status reports RF/protocol transport, not EMV application
success.

### Stale or inactive session

Status:

```text
STATUS_PAR_ERR = 0x0060
```

Payload length is zero. A zero, stale, inactive, or owner-mismatched token does not
destroy a different active session. A request from the non-owner transport is
rejected earlier with `STATUS_DEVICE_MODE_ERROR`.

### RF/ISO-DEP failure

The firmware aborts the session and turns the field off. The payload is:

```text
Offset  Size  Field
0       1     iso_dep_error
1       1     underlying RF status
2       1     WTX count diagnostic
```

ISO-DEP error values:

| Value | Meaning |
|---:|---|
| 0 | No classified ISO-DEP framing error |
| 1 | Parameter error |
| 2 | Transport error |
| 3 | CRC error |
| 4 | Block-format error |
| 5 | Sequence error |
| 6 | Reassembly overflow |
| 7 | Timeout |

Value `0` can still occur in a failed EXCHANGE when ISO-DEP framing completed but
the reassembled logical response was shorter than the required two status bytes.
That case maps to `STATUS_HF_ERR_STAT` and invalidates the session.

The outer response status is selected as follows:

- `ISO_DEP_ERR_CRC` maps to `STATUS_HF_ERR_CRC`;
- `ISO_DEP_ERR_TIMEOUT` or RF status `STATUS_HF_TAG_NO` maps to
  `STATUS_HF_TAG_NO`;
- an underlying known HF status for state, CRC, collision, BCC, parity, or ATS is
  preserved;
- every other case maps to `STATUS_HF_ERR_STAT`.

Firmware records the WTX count as each valid request is accepted and preserves it
in synchronized failure diagnostics. A zero count means failure before the first
accepted WTX; it does not prove that the command I-block was never transmitted.

## Command 6013: session STOP

### Request

```text
Command: 6013 / 0x177D
Payload length: 4
```

Payload:

```text
Offset  Size  Field
0       4     session_id, nonzero uint32
```

### Successful response

```text
Status: STATUS_SUCCESS = 0x0068
Payload length: 0
```

Firmware sends an ISO-DEP S(DESELECT) on a best-effort basis, invalidates the
session, clears retained block state, and turns the antenna off. It does not fail
STOP if the DESELECT response is missing.

A stale STOP returns `STATUS_PAR_ERR` and does not stop a different active session.
If the device is no longer in reader mode, a correctly formed STOP returns
`STATUS_DEVICE_MODE_ERROR` after ensuring the session is aborted.

Normal STOP callers still require `STATUS_SUCCESS`. After an uncertain 6012,
however, the same serialized connection may use 6013 as an idempotent reset. With no
intervening START, an empty response with `STATUS_SUCCESS`, `STATUS_PAR_ERR`, or
`STATUS_DEVICE_MODE_ERROR` confirms the uncertain session is closed. Receiving that
response is the ordering barrier that permits clearing only the 6012 quarantine.
No response, malformed data, or any other status leaves reset unconfirmed and
requires reconnection before another relay session.

## Relevant outer status values

| Status | Value | Meaning in this feature |
|---|---:|---|
| `STATUS_HF_TAG_OK` | `0x00` | START or EXCHANGE succeeded |
| `STATUS_HF_TAG_NO` | `0x01` | No card or RF timeout |
| `STATUS_HF_ERR_STAT` | `0x02` | Generic RF/ISO-DEP failure |
| `STATUS_HF_ERR_CRC` | `0x03` | RF CRC failure |
| `STATUS_HF_COLLISION` | `0x04` | ISO14443-A collision |
| `STATUS_HF_ERR_BCC` | `0x05` | UID BCC failure |
| `STATUS_HF_ERR_PARITY` | `0x07` | RF parity failure |
| `STATUS_HF_ERR_ATS` | `0x08` | ATS missing or unusable |
| `STATUS_PAR_ERR` | `0x60` | Invalid length, ID, or inactive session |
| `STATUS_DEVICE_MODE_ERROR` | `0x66` | Ultra is not in reader mode or request came from the non-owner transport |
| `STATUS_SUCCESS` | `0x68` | STOP succeeded |

## Firmware ISO-DEP behavior

The retained reader state includes:

- next PCD I-block number;
- next expected PICC I-block number;
- CID state;
- card frame size;
- frame waiting timeout.

The transport implements:

- command I-block fragmentation;
- response I-block reassembly;
- I-block sequence continuity across command `6012` calls;
- R-ACK and R-NAK handling;
- up to two retransmissions of an outbound command I-block when the card returns
  R-NAK;
- up to two R-NAK requests for the expected response after a timeout following the
  final command I-block or an accepted WTX, without replaying the command APDU;
- malformed-frame and CRC failures are not hidden by response recovery;
- CRC validation;
- CID consistency checks, although persistent sessions do not negotiate or enable
  CID: RATS uses CID 0, outgoing blocks omit CID, and incoming CID is rejected;
- NAD rejection;
- at most 32 intermediate command chain blocks plus a final command block;
- at most 32 response I-blocks;
- up to 512 response bytes;
- backend card WTX processing.

ATS FSCI values 0..8 determine card frame size, capped to 64 bytes. Unsupported or
reserved FSCI values leave the 16-byte default. ATS FWI defaults to 4 when TB is
absent, is capped at 14, and determines a base frame waiting time rounded and
clamped to 50..5000 ms. Firmware does not reject every ATS TL/interface-byte
inconsistency.

### No firmware or arm inactivity timeout

A successful START remains active without an elapsed-time or inactivity limit.
Firmware does not expire it between matching exchanges, and CU GUI does not maintain
any backend-session lease. GUI/native arm likewise has no overall lease or expiry.
Only the per-terminal-APDU native watchdog is time-bounded at 50..5000 ms. Waiting
for the first terminal APDU therefore keeps the same card-first firmware session and
does not issue a replacement START.

### Other invalidation triggers

The session is invalidated by:

- a matching STOP;
- an RF or ISO-DEP exchange failure;
- another START from the owner transport;
- leaving reader mode;
- explicit HF field on/off commands;
- a valid HF14A raw command;
- another HF reader command using the shared reader pre-hook;
- an EXCHANGE or STOP that detects the device is no longer in reader mode;
- loss of the owner USB or BLE command link.

Commands arriving on the non-owner transport are rejected before dispatch and do
not invalidate or take over the session. Owner USB/BLE disconnection is reported to
the session state machine and aborts it, clears retained block state, and powers
down the field.

When BLE pairing is enabled, the firmware's global authorization gate can reject
commands before these handlers run if the BLE command link is not authorized. The
session token is not a substitute for BLE pairing or transport authorization.

## Android native protocol

### Static HCE declaration

The service uses:

```text
Class: AuthorizedRelayHostApduService
Category: payment
Static AID: PPSE
Device unlock required: true
Permission: android.permission.BIND_NFC_SERVICE
```

The component is exported for Android NFC service binding but protected by
`BIND_NFC_SERVICE`.

### MethodChannel

Channel:

```text
io.chameleon.ultra/authorized_relay_methods
```

| Method | Arguments | Result |
|---|---|---|
| `getReadiness` | none | HCE, NFC, lock, default service, registered AIDs |
| `allocateArmToken` | none | New process-static positive arm token and diagnostic monotonic timestamp |
| `registerAids` | `{aids: List<String>}` | `true` after dynamic registration |
| `authorizeAndEnable` | `{armToken: number, aids: List<String>, deadlineMs: number}` | `true` after atomic, unbounded arm |
| `setEnabled` | `{enabled: false, armToken?: number}` | `true` only when token-bound disable was applied |
| `isPending` | `{armToken: number, id: number}` | Whether that request is enabled, current, and before its absolute native deadline |
| `respond` | `{armToken: number, id: number, response: Uint8List}` | Whether response matched pending APDU |
| `openPaymentSettings` | none | Whether settings opened |

`getReadiness` returns:

```text
{
  hceSupported: bool,
  nfcEnabled: bool,
  deviceLocked: bool,
  isDefaultPaymentService: bool,
  registeredAids: List<String>
}
```

Android's `getAidsForService()` reports the retained dynamic group after cleanup.
On a fresh installation before first preparation, the static PPSE remains active
even if this dynamic list is empty.

### EventChannel

Channel:

```text
io.chameleon.ultra/authorized_relay_events
```

APDU event:

```text
{
  type: "apdu",
  armToken: positive integer,
  id: integer,
  apdu: Uint8List containing the exact command bytes,
  receivedUs: monotonic microseconds,
  expiresAtUs: absolute monotonic deadline in microseconds
}
```

Deadline event:

```text
{
  type: "expired",
  armToken: positive integer,
  id: integer
}
```

NFC deactivation event:

```text
{
  type: "deactivated",
  armToken: positive integer,
  reason: integer
}
```

Native readiness or event-delivery failure uses an `invalidated` event with the
same `armToken` and a diagnostic `reason`. Native state and dynamic routing are
already fail-closed before this event is attempted.

Android reason `0` represents link loss and `1` represents terminal DESELECT. Both
close the current armed relay and run the same cleanup, but the GUI reports them
differently. DESELECT after at least one delivered APDU is treated as normal
terminal closure. Link loss before any completed APDU is an error. Link loss after
delivered APDUs is shown as an informational field-closure summary because readers
commonly turn their field off normally; the terminal result remains authoritative.
DESELECT before any completed APDU points to routing/AID selection failure.

APDU request IDs are monotonically allocated native correlation IDs. They are not
the firmware session ID and are not sent to the physical card. `respond` accepts
only the current arm token, currently pending ID, and a binary response of 2..512
bytes. A stale token, stale ID, or malformed response returns `false`; the
pending APDU then remains until its watchdog or another cleanup path handles it.

Immediately before `6012`, Flutter calls `isPending` with the same arm token and
request ID. Native compares `SystemClock.elapsedRealtimeNanos()` with the stored
absolute expiry. An expired check runs native expiry completion and returns false;
Flutter then tears down without sending the APDU to the physical card. This closes
the queued-event case where Dart receives an APDU only after its watchdog elapsed.
Native repeats that monotonic expiry check atomically inside `respond`; delayed
main-thread watchdog scheduling therefore cannot deliver a late backend response.
If the deadline or NFC deactivation occurs after 6012 began, GUI reports backend
card state as uncertain and instructs the operator not to retry that APDU.

### Terminal-facing status words generated by the relay

| SW | Condition |
|---|---|
| `6700` | Null APDU or APDU outside 4..512 bytes |
| `6985` | Another terminal APDU is already pending |
| `6400` | Relay unavailable/disarmed, backend failure, or rejected Apple Transit GPO while the request was pending |
| `6401` | Native deadline expired first; a later backend response is not delivered |
| Card SW | Successful backend response forwarded unchanged in every mode |

`6401` is a relay-specific diagnostic status, not an EMV approval or issuer
response.

### Native readiness checks

Arming requires:

- HCE hardware support;
- NFC adapter enabled;
- device unlocked;
- CU GUI selected as the payment default;
- dynamic group containing PPSE;
- at least one dynamic non-PPSE payment AID;
- native bridge in prepared state.

Android verifies these conditions when arming. Once Android routes an APDU to the
armed payment service, the hot path rechecks the native arm token, event sink,
pending ownership, and monotonic deadline without repeating package-manager or
payment-default binder queries. Android's unlocked-device service requirement and
NFC routing remain authoritative for delivery to `processCommandApdu()`.

## Timing and WTX

There are two independent ISO-DEP links:

```text
Link A: terminal <-> Android HCE
Link B: ChameleonUltra reader <-> physical card
```

Do not treat WTX on one link as WTX on the other.

### Backend card WTX

On Link B, the physical card may send ISO-DEP S(WTX). Ultra accepts WTX multiplier
values `1..59`, supports at most 64 WTX requests in one exchange, caps each WTX wait
at 5000 ms, and enforces a separate 5000 ms cumulative WTX budget for the complete
APDU. Count and cumulative-time bounds prevent an unresponsive target from holding
the reader indefinitely.

If the target times out after the final command I-block or immediately after an
accepted WTX, Ultra sends at most two ISO-DEP `R(NAK)` blocks requesting
retransmission of the expected response block. This does not resend the command
APDU. Recovery waits count against the same 5000 ms budget, so a departed target
still fails closed.

This allows the physical card more time relative to Ultra. It does not notify the
terminal and does not reset Android's application watchdog.

### Terminal-facing WTX

On Link A, application code receives logical APDUs through `HostApduService`.
Returning `null` and later calling `sendResponseApdu()` is Android's asynchronous
response mechanism. NFC frame-level handling, including any WTX generated while
waiting for the host, belongs to the Android NFC controller and vendor stack.

CU GUI cannot emit, inspect, or force a raw terminal-facing S(WTX) block through the
public HCE API. A terminal that supports WTX may wait longer if the Android stack
uses it, but this is device- and terminal-dependent.

Deterministic control of terminal-facing WTX would require the ChameleonUltra, or
other controllable card-emulation hardware, to be the endpoint presented directly
to the terminal. That is not the topology implemented here.

### Independent timeout layers

| Layer | Timeout/limit |
|---|---|
| Android native APDU watchdog | Configurable 50..5000 ms, default 1000 ms |
| GUI command 6011 wait | 6 seconds |
| GUI command 6014 wait | 6 seconds |
| GUI command 6012 wait | 6 seconds |
| GUI command 6013 wait | 6 seconds |
| Python command 6011 wait | 6 seconds |
| Python command 6014 wait | 6 seconds |
| Python command 6012 wait | 10 seconds |
| Python command 6013 wait | 6 seconds; includes backend DESELECT wait |
| Backend ISO-DEP base FWT | Derived from ATS, clamped 50..5000 ms |
| RC522 software deadline resolution | App-timer ticks; no 10 ms phase quantization |
| Preferred BLE peripheral interval | 7.5..15 ms; central negotiation remains authoritative |
| Requested Android BLE ATT MTU | 247, best effort |
| Backend firmware inactivity | None |
| GUI/native armed-session lease | None; only each terminal APDU has a deadline |
| Terminal EMV limits | Terminal/kernel-specific |

The Android watchdog normally expires before the GUI's six-second command timeout
when configured at the five-second maximum. A backend operation can still finish
after Android has already returned `6401`; card state may therefore have advanced
even though the terminal did not receive the real response.

The immediate native `isPending` check prevents starting a new backend exchange
after the native deadline. It cannot cancel a `6012` that was valid at the check and
crosses the deadline while RF I/O is already in progress.

There is no overall firmware-session or GUI/native arm lease. A card-first wait keeps
the original session and never replaces it merely because time passed. Explicit
cleanup and the event-driven invalidation conditions remain authoritative.

Never interpret a timeout as proof that the physical card did nothing. Do not retry
a state-changing transaction blindly.

## Typical EMV APDU sequence

A terminal may send a sequence such as:

1. SELECT PPSE

   ```text
   00 A4 04 00 0E 325041592E5359532E4444463031 00
   ```

2. SELECT one card AID

   ```text
   00 A4 04 00 <Lc> <AID> 00
   ```

3. GET PROCESSING OPTIONS

   ```text
   80 A8 00 00 <Lc> 83 <length> <PDOL values> 00
   ```

4. READ RECORD commands from the AFL

   ```text
   00 B2 <record> <SFI control> 00
   ```

5. Scheme/card-specific commands, possibly including GENERATE AC

   ```text
   80 AE <cryptogram type> 00 <Lc> <CDOL1 values> 00
   ```

Transparent mode does not parse or rewrite these terminal APDUs. Apple Transit
mode observes SELECT AID/FCI and applies only the bounded GPO rewrite documented
above; all other commands and all responses retain arrival order and bytes. The
preparation PPSE parse only determines which Android AIDs route to the service.

## Manual CLI use of the persistent session

The Python CLI exposes the firmware session independently of Android HCE. Start the
CLI from `software/script/`, connect the Ultra, and enter reader mode as required.

Start a session:

```text
hf 14a session start
hf 14a session start --express-transit
```

The second form selects command 6014 ECP2 activation. It does not perform the GUI's
GPO rewrite; manual CLI EXCHANGE sends exactly the APDU supplied by the operator.

Example output fields:

```text
Session ID: 7 (0x00000007)
UID: <redacted>
ATQA: <value>  SAK: <value>
ATS: <value>
```

Select PPSE using the returned session ID:

```text
hf 14a session exchange 7 00A404000E325041592E5359532E444446303100
```

Continue with another logical APDU using the same ID:

```text
hf 14a session exchange 7 00A4040007A000000003101000
```

Stop the exact matching session:

```text
hf 14a session stop 7
```

Session IDs accept decimal or `0x` notation. APDUs accept an even-length hex string
up to 512 bytes. CLI EXCHANGE and STOP do not silently switch the Ultra back into
reader mode; a mode change invalidates the session.

## Logging, evidence, and privacy

The relay page retains at most 100 in-memory summaries. Each summary contains:

- command APDU header `CLA INS P1 P2`;
- command APDU total byte length;
- final response status word;
- measured backend command duration;
- delivery/failure state;
- native arm token, request ID and monotonic receive/expiry timestamps;
- backend firmware session ID;
- backend mode (`transparent` or `appleTransit`);
- whether the response came from the backend or was relay fallback `6400`;
- device status, ISO-DEP error, RF status, WTX count, and whether firmware already
  closed the backend session;
- for an Apple Transit GPO only, selected AID, PDOL definition/length, APDU/Le
  lengths, and before/after bytes for the three approved fields;
- a bounded policy reason when SELECT/FCI/PDOL/GPO validation failed.

The page does not retain APDU bodies in its summary records. Transit evidence does
not include amount, unpredictable number, Le value, complete FCI, complete GPO, or
response bytes. Command `6012` request and response data are redacted from normal
GUI communicator logs.

The copy button beside **Relayed APDUs** exports a versioned
`chameleon-authorized-relay-debug` JSON report containing those summaries plus
current readiness, transport, routing, backend RF identity, AIDs, error, and closure
notice. The report deliberately excludes complete command and response APDU bodies.

Full APDUs still exist transiently in:

- Android byte arrays;
- platform-channel hex strings;
- Dart byte arrays;
- Chameleon command frames;
- firmware buffers;
- the terminal and physical card.

Debug firmware can hex-dump BLE NUS frames, and low-level parser errors can include
unexpected raw chunks. Do not treat UI redaction as a guarantee that sensitive
bytes can never appear in debug memory or diagnostic logs. Do not publish raw
captures from a real card.

The timing shown in the page measures the awaited command `6012` exchange. It does
not include all native-to-Dart dispatch time, Dart-to-native response time, or final
NFC controller delivery, so it is not a full terminal-to-card round-trip metric.

## Limits

| Item | Limit |
|---|---:|
| Active firmware sessions | 1 global session |
| Firmware inactivity | No timeout |
| GUI/native arm duration | No overall lease or expiry |
| Preparation no-card retry delay | 500 ms plus scan/transport time |
| Command APDU | 1..512 bytes in firmware; terminal path requires 4..512 |
| Response APDU | 2..512 bytes |
| Terminal APDUs pending | 1 |
| Native deadline | 50..5000 ms |
| Payment AID length | 5..16 bytes |
| Dynamic AID group | 32 total including PPSE |
| PPSE TLV nesting | 16 nested levels before rejection |
| BER tag length | Up to 4 bytes |
| BER long-form length | 1..3 length bytes; indefinite form rejected |
| Backend command chaining | 32 intermediate blocks plus final block |
| Backend response chaining | 32 I-blocks total |
| Backend WTX requests | 64, additionally bounded to 5000 ms cumulative wait |
| Response-block recovery | 2 `R(NAK)` requests; command APDU is never resent |
| GUI summary records | Latest 100 |

## Lifecycle and recovery details

### Prepared and armed are intentionally separate

Dynamic AID registration sets native `prepared=true`. HCE forwarding remains
disabled until arm sets `enabled=true`. This separation is required because Android
can instantiate the payment service while the operator is selecting it in system
settings. Service creation must not interpret a valid prepared-but-not-armed state
as stale.

Native HCE arming is refused when native preparation is false. The GUI satisfies
that invariant automatically by running preparation before its arm request when
needed; direct platform callers still cannot bypass preparation.

### Component lifetime differs from routing lifetime

The component is permanent, but three routing states are possible:

| State | Component | Static PPSE | Dynamic PPSE/card AIDs | Forwarding |
|---|---|---|---|---|
| Fresh install, never prepared | Enabled | Available | Absent | Returns `6400` |
| Prepared, not armed | Enabled | Replaced by dynamic group | Present | Returns `6400` |
| Prepared and armed | Enabled | Replaced by dynamic group | Present | Forwards one APDU at a time |
| Disarmed after preparation | Enabled | Replaced by retained dynamic group | Present | Returns `6400` |

The readiness API continues to report the retained dynamic group while disarmed.
This does not mean forwarding remains armed: native authorization and the live
firmware session are separate and have already been cleared.

### Cleanup is fail-closed and reports incomplete external cleanup

Disabling native forwarding is immediate and fails a pending APDU with `6400`.
Firmware session reset is an external operation and can fail. Flutter invalidates
its local arm and prepared state but reports `Relay cleanup incomplete` rather than
claiming successful teardown when reset cannot be confirmed. Dynamic AIDs are not
part of cleanup and remain registered intentionally.

Page disposal cannot await asynchronous cleanup. It starts HCE disable, firmware
reset, and event cancellation, and leaves a continuation attached to any in-flight
START so its exact communicator resets a late successful session.
Keep the app open until normal deactivation or use the arm switch to disarm before
leaving the page.

### Event-listener loss

If the Flutter EventChannel listener disappears, native code disables forwarding,
clears native preparation, clears a pending request, and returns `6400` to that
request. `MainActivity.onCancel` removes routing only when the cancelling listener
still owns the current native sink; a late cancellation from an older engine cannot
clear its replacement.
Replacing one non-null event sink with another also invalidates the old arm and
pending request before installing the new listener, then triggers the same routing
reset. This prevents a recreated Flutter engine from inheriting an unowned native
arm.

The always-mounted unarmed service still fails closed while the retained dynamic
routing remains visible to Android.

### Process death and restart

Prepared card metadata, the firmware token held by Flutter, and native prepared/
enabled flags are RAM-only. After process death:

- the component remains installed and enabled;
- Android retains the last dynamic AID registration;
- a cold service instance sees no native authorization and returns `6400`;
- a new Flutter engine leaves the retained routing intact;
- the operator must prepare and arm again.

The physical firmware session is independent of Android process memory but is owned
by its USB/BLE command link. Process death normally closes that link, which aborts
the session immediately. If a platform keeps the link alive after losing the token,
the session cannot be recovered by Flutter; close/reconnect the owner link before a
new START.

### Repeated preparation

Pressing Prepare clears the page's old prepared metadata before the new preflight.
The existing native dynamic group is replaced only when new registration succeeds.
If a repeated preparation fails before registration, a previous group can remain
in Android while the page reports no prepared card. This retention is intentional;
press Arm again to reacquire the backend and atomically replace the group after a
successful PPSE preflight. The unarmed service continues to return `6400` meanwhile.

### Default-payment changes after arming

Every APDU that still reaches CU GUI rechecks the default service and receives
`6400` if CU GUI is no longer default. A new NFC activation can instead route
directly to the newly selected payment service and never invoke CU GUI. Changing
the default does not guarantee that Flutter immediately updates its armed display.
Manually disarm and prepare again after fixing settings.

### Mode and monitor side effects

Preparation switches ChameleonUltra into reader mode when necessary and does not
restore the prior emulation mode afterward. The GUI pauses emulated-tag change
monitoring while preparing, arming, or holding the backend session, then resumes it
on cleanup. Other tools must not send HF reader commands concurrently.

### GUI rendezvous states

The arm switch reports whether the relay is waiting for the backend, waiting for
the terminal, or exchanging. It remains cancellable in either waiting state.
Native code still authoritatively enforces one pending APDU. Disarming during an
exchange fails the pending terminal request with `6400`, stops the bound backend if
possible, and ignores events or responses carrying an older arm token.

## Failure handling and troubleshooting

### `Firmware does not advertise persistent ISO-DEP relay sessions`

Cause: the connected device does not advertise 6012/6013 or the selected START
command. Apple Transit specifically requires 6014; transparent mode requires 6011.

Actions:

1. Confirm the device is a ChameleonUltra, not Lite.
2. Build and flash the current Ultra application firmware.
3. Reconnect so CU GUI refreshes the capability list.

### `Preparation failed: Backend card did not accept PPSE`

Cause: PPSE did not return `9000`, RF placement failed, or the card is not exposing
a normal payment PPSE application.

Actions:

1. Reposition the card on the Ultra antenna.
2. Remove other cards from the RF field.
3. Retry preparation once.
4. Inspect the direct CLI START and PPSE EXCHANGE response.

Do not use arbitrary AID probing as a substitute for the strict preparation flow.

### Prepare keeps showing `Waiting continuously for a backend card`

This is the expected state after firmware returns `STATUS_HF_TAG_NO`. Place one
card on the center of the Ultra HF antenna. CU GUI retries after every completed
no-card response and prepares automatically when START succeeds.

If no card will be used, press `Cancel waiting`. Cancellation can take up to the
current START command timeout if a command is already in flight. If a card is
present but preparation reports CRC, parity, collision, or ATS failure, the GUI
stops instead of retrying indefinitely so that RF placement or card compatibility
can be corrected.

### Armed shows `waiting for backend card`

HCE is already enabled. A terminal APDU may be pending under the native watchdog,
or no side may have arrived yet. Place the prepared card on the Ultra and keep it
stable. Clean no-card START responses are retried every 500 ms. A START timeout,
transport uncertainty, identity mismatch, or native deadline is not retried and
requires a fresh Prepare; reconnect the Chameleon after transport uncertainty. For
a mobile Wallet backend whose UID changes each activation, enable rotating-UID mode
before arming.

Phone-first success is not guaranteed by selecting a 5000 ms watchdog. Android's
public HCE API cannot force terminal-facing WTX, and the terminal may enforce a
shorter limit. Card detection, START, identity validation, and EXCHANGE must
complete inside the actual terminal/native deadline. Rotating-UID validation adds a
PPSE exchange.

### Armed shows `waiting for terminal`

The prepared backend session is active and has no inactivity timeout. CU GUI keeps
that exact card-first session and does not recycle it, regardless of how long the
terminal takes to arrive. If it closes while waiting, investigate owner USB/BLE link
loss, backend RF loss, reader-mode exit, explicit cleanup, or another owner command;
elapsed time alone is not a cause.

### Android reports `LINK_LOSS`

Android reported HCE deactivation reason `0` (`DEACTIVATION_LINK_LOSS`). Before any
delivered APDU, this means the phone left the RF field or the terminal stopped
waiting before the relay response arrived. After one or more delivered APDUs, the
same reason normally means the reader turned its field off after completing the NFC
exchange. CU GUI labels this as normal transport closure rather than a relay error.
The ISO-DEP activation cannot be resumed in either case.

Actions:

1. Use backend-first ordering: keep the backend card/Wallet on the Ultra until the
   GUI says `Armed: waiting for terminal`.
2. Use USB rather than BLE.
3. Keep the CU GUI phone stationary on the terminal antenna for the complete run.
4. For rotating-UID mode, account for the extra PPSE identity exchange.
5. Do not assume that increasing the native deadline makes the terminal wait;
   Android HCE cannot force terminal-facing WTX.

After delivered APDUs, CU GUI shows an informational summary with the number of
responses instead of a red error. Check the terminal result: relay transport does
not prove approval or completion. If the terminal reports failure or no result,
card state can still be uncertain; do not retry a state-changing command blindly.

### `Terminal deselected CU GUI before any APDU completed`

Android reported reason `1` (`DEACTIVATION_DESELECTED`) before a successful relay
exchange. Confirm CU GUI is still the payment default and that preparation
registered every AID advertised by PPSE. A DESELECT after delivered APDUs is treated
as normal terminal closure and does not show this error.

### Trace aborts PPSE after repeated `F2 01` frames

`F2 <WTXM>` is an ISO-DEP S(WTX) request from the backend. A trace containing
repeated card-to-reader and reader-to-card `F2 01` frames, followed by APDU status
word `FFFF`, `applicationCount=0`, and an aborted result, means the target requested
more processing time but never returned the PPSE response before the reader's WTX
guard fired.

Older firmware allowed only eight WTX requests, so a ninth `F2 01` aborted after
roughly 350 ms. Current firmware derives FWT from ATS with a 50 ms practical floor,
accepts up to 64 WTX frames, and enforces a 5000 ms cumulative WTX budget. Flash the updated firmware;
changing only the Android APK does not alter this backend ISO-DEP limit.

If multiple Ultras are present, verify the USB serial number being programmed and
disconnect the others before DFU. A successful flash of one unit does not update the
different unit retained by the GUI over BLE.

### Relay fails on `80 CA 9F36` after successful PPSE/GPO/READ RECORD

This is GET DATA for the application transaction counter. Card status `6400` or
`6A81` is a logical application response and must be forwarded unchanged. Device
status `0x01` with ISO-DEP error `7` instead means the backend stopped returning an
RF response after its WTX sequence. Firmware closes that session; a subsequent STOP
for the same ID would therefore return `0x60` and is intentionally omitted by the
GUI.

The relay sends fail-closed fallback `6400` to the terminal, records
`responseSource=relayFallback`, and does not replay `80 CA 9F36`. Current firmware
may issue up to two `R(NAK)` response-block retransmission requests before declaring
the timeout. The same recovery applies when the diagnostic WTX count is zero and no
first response block arrived. If the failure remains repeatable after several Wallet
taps but a fresh EMV trace completes, reset/reopen the backend Wallet and Prepare a
new session; do not convert the transport failure into a fabricated card response.

### `Timed out after 6000 ms waiting for command 6011`

This is not a clean no-card result. The host did not receive a complete START
response, so the command state is uncertain and automatic retry is intentionally
stopped. A late response can still arrive and the firmware may have opened a
session.

The unknown outcome provides no session ID with which to confirm a 6013 reset, so
reconnect the Chameleon before pressing Prepare again. If it repeats with USB and a
stable card position, verify that the current firmware is flashed and inspect the
command transport. Do not add the timeout case to the no-card retry loop.

### `PPSE did not advertise a payment AID`

Cause: the PPSE body contained no valid tag `4F` under an application template
`61`, or it was malformed.

Actions:

1. Confirm the card and PPSE response in an authorized diagnostic trace.
2. Do not manually inject an AID merely to bypass parser validation.

### CU GUI disappears from payment settings

The current component is permanent. Disappearance normally indicates an old APK,
a retained disabled-component override, or an Android NFC cache issue.

Actions:

1. Install the current APK with `adb install -r`.
2. Launch CU GUI once so it explicitly enables the service component.
3. Verify the service with `cmd package query-services`.
4. Toggle NFC off and on.
5. Reopen payment settings.

### CU GUI is selected but readiness says it is not default

Actions:

1. Return from Settings to CU GUI and wait for lifecycle refresh.
2. Unlock the phone.
3. Toggle NFC off/on if the vendor NFC service retained the old route.
4. Compare `settings get secure nfc_payment_default_component` and `dumpsys nfc`.

The secure setting can name CU GUI while a stale in-memory NFC cache still routes
PPSE to the previous Wallet. Restarting NFC rebuilds the cache on affected devices.

### `Backend UID changed after preparation`

Cause: START returned a different UID. For a physical card this normally means the
backend changed. For a phone Wallet it can be normal randomized-UID behavior.

Actions:

1. For a physical card, put the original card back and prepare again.
2. For an authorized phone Wallet backend, disarm and enable
   `Mobile Wallet backend (rotating UID)`.
3. Arm again; CU GUI automatically prepares if cleanup cleared the old preflight.
4. Keep the backend device stable through each active ISO-DEP exchange.

If rotating mode reports `Mobile Wallet identity changed after preparation`, UID
rotation was not the only difference: ATQA, SAK, ATS, or the complete PPSE response
changed. CU GUI fails closed rather than accepting a generic scheme AID match.

### Terminal immediately receives `6400`

Possible causes:

- relay not armed;
- Flutter page/event listener not active;
- Android locked;
- NFC disabled;
- CU GUI no longer the default payment service;
- native prepared state lost after process death;
- backend exchange failed and the GUI returned the fail-closed status.

Reopen the page, prepare, verify every readiness row, and arm again. If the terminal
arrived first, confirm that backend acquisition could finish inside its deadline.

### Terminal receives `6401`

The native per-APDU watchdog expired.

Actions:

1. Use USB instead of BLE.
2. Increase the deadline only if that phone/terminal pair is empirically known to
   tolerate delayed asynchronous HCE responses.
3. Keep Android unlocked and avoid background load.
4. Check backend RF placement.
5. Treat card state as uncertain if the APDU could be state-changing.

If 6012 was already in flight, CU GUI never retries it. Cleanup sends ordered 6013;
statuses `0x68`, `0x60`, and `0x66` confirm reset and allow same-connection reuse.
Reconnect only if that reset fails or the communicator was invalidated.

### Terminal receives `6985`

Android received another APDU while one was still pending. This can indicate an
unexpected terminal retry or a response path slower than the terminal's behavior.
Use USB, inspect timing, and do not run concurrent HCE operations.

### Relay works initially and then fails after waiting

Waiting does not expire the backend session and CU GUI does not recycle it. Check
whether the owner USB/BLE link disconnected, Android deactivated HCE, the Ultra left
reader mode, backend RF failed, cleanup ran, or another owner command invalidated
the shared reader state. Do not diagnose elapsed time as a firmware or GUI lease.

### Normal contactless payments stop working after the lab

CU GUI is still the Android payment default by design. Select the normal Wallet
manually in Android contactless-payment settings.

### Dynamic AIDs remain visible after cleanup

This is expected. CU GUI retains the last PPSE/card AID group so the payment service
does not disappear after disarm or process restart. The live backend session and
native arm are still closed, and an unarmed tap receives `6400`. Preparing another
card atomically replaces the retained group only after Android confirms the new
registration. The component itself remains enabled because it is manifest-enabled.

## Validation and test commands

Firmware native tests:

```bash
make -C firmware/tests test SANITIZE=1
```

Focused session and ISO-DEP tests:

```bash
make -C firmware/tests build/test_iso_dep_reader build/test_iso_dep_session
./firmware/tests/build/test_iso_dep_reader
./firmware/tests/build/test_iso_dep_session
```

Python tests, from `software/script/`:

```bash
python tests/test_iso_dep_session.py
python tests/test_command_ids.py
python -m unittest discover -s tests
```

GUI analysis and focused tests:

```bash
cd ../ChameleonUltraGUI/chameleonultragui
flutter analyze
flutter test test/authorized_relay_test.dart \
  test/chameleon_command_queue_test.dart
```

Android Kotlin compilation:

```bash
cd ../ChameleonUltraGUI/chameleonultragui/android
./gradlew :app:compileDebugKotlin
```

Hardware-free tests validate firmware ISO-DEP/session behavior, Python host parsing,
Flutter PPSE parsing, and basic GUI command framing. They do not currently exercise
the Android `HostApduService`, platform-channel bridge, native deadline, complete
GUI relay state machine, payment-default transitions, or always-mounted cleanup.
They also do not prove RF interoperability, Android vendor-stack timing, terminal
WTX behavior, or EMV scheme compatibility. Validate Ultra, Android, card, and
terminal together in the authorized offline environment.

## Source map

Firmware:

- `firmware/application/src/data_cmd.h`
- `firmware/application/src/app_cmd.c`
- `firmware/application/src/rfid/reader/hf/iso_dep_session.c`
- `firmware/application/src/rfid/reader/hf/iso_dep_session.h`
- `firmware/application/src/rfid/reader/hf/iso_dep_reader.c`
- `firmware/application/src/rfid/reader/hf/iso_dep_reader.h`
- `firmware/tests/test_iso_dep_session.c`
- `firmware/tests/test_iso_dep_reader.c`

Python host:

- `software/script/chameleon_enum.py`
- `software/script/chameleon_cmd.py`
- `software/script/chameleon_cli_unit.py`
- `software/script/tests/test_iso_dep_session.py`
- `software/script/tests/test_command_ids.py`

Android and Flutter GUI, in the separate GUI repository:

- `android/app/src/main/AndroidManifest.xml`
- `android/app/src/main/res/xml/authorized_relay_apdu_service.xml`
- `android/app/src/main/kotlin/io/chameleon/ultra/MainActivity.kt`
- `android/app/src/main/kotlin/io/chameleon/ultra/AuthorizedRelayHostApduService.kt`
- `lib/bridge/authorized_relay_platform.dart`
- `lib/bridge/chameleon.dart`
- `lib/helpers/authorized_relay.dart`
- `lib/gui/menu/hacking/authorized_relay_lab.dart`
- `test/authorized_relay_test.dart`
- `test/chameleon_command_queue_test.dart`

## Final operator checklist

Before the tap:

- [ ] Card and terminal use is explicitly authorized.
- [ ] Terminal has no production network path.
- [ ] Current Ultra firmware advertises `6012`, `6013`, and selected START 6011 or 6014.
- [ ] Ultra is connected, preferably over USB.
- [ ] Physical card is stable on the Ultra antenna.
- [ ] Android supports payment HCE.
- [ ] NFC is enabled.
- [ ] Android is unlocked.
- [ ] Backend preparation succeeded manually or through the Arm switch.
- [ ] Rotating-UID mode is enabled only when the backend is an authorized mobile
      Wallet that changes UID between activations.
- [ ] Apple Transit mode is enabled only for intended ECP2/GPO tests and disabled
      manually before returning to transparent relay operation.
- [ ] CU GUI is the Android contactless default.
- [ ] Deadline is appropriate for the terminal.
- [ ] Relay is armed immediately before the tap.

After the tap:

- [ ] Relay reached cleanup or was manually disarmed.
- [ ] Backend RF session stopped.
- [ ] Dynamic card AIDs were verified absent; an NFC restart alone is not proof of
      removal.
- [ ] Any uncertain 6012 was not retried, and ordered 6013 reset was confirmed or
      the command transport was reconnected.
- [ ] Normal Wallet was manually restored as the payment default.
- [ ] Sensitive debug logs were not retained or published.
