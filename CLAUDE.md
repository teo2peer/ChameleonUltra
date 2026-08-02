# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware and host tooling for the **ChameleonUltra**, an nRF52840-based RFID/NFC research device (HF 13.56 MHz + LF 125 kHz), plus a BLE audit command group. Two halves that talk over one binary command protocol:

- `firmware/` — nRF52840 C firmware (SoftDevice **S140 v7.2.0**), for both `ultra` and `lite` boards.
- `software/` — Python CLI host client (`software/script/`) and native crypto helpers (`software/src/`).

The graphical app is a **separate repository**: `GameTec-live/ChameleonUltraGUI` (Flutter; locally at `../ChameleonUltraGUI`). It speaks the same command protocol and mirrors the command IDs again in Dart (`lib/helpers/definitions.dart`, `enum ChameleonCommand`).

## Build / run / test

**Firmware** (needs the ARM toolchain `arm-none-eabi-gcc`; there is no way to build it without it):
```bash
cd firmware && ./build.sh                 # builds bootloader + application, packages DFU zips
CURRENT_DEVICE_TYPE=lite ./build.sh       # build for the Lite board (default: ultra)
make -j -C firmware/application            # build just the application ELF/hex
cd firmware && docker compose up --pull=always build-ultra   # containerized build (or build-lite); output in firmware/objects/
```
Flash with `firmware/flash-dfu-full.sh` until the enlarged FDS-aware bootloader
has been deployed. App-only packaging/flashing then requires the explicit
`ALLOW_APP_ONLY_DFU=1` migration acknowledgement.

**Host CLI** (Python ≥ 3.9; UV is the intended package manager, see `software/README.md`):
```bash
cd software/script
python3 chameleon_cli_main.py             # interactive CLI; use 'hw connect' to attach a device
```
Deps: `pyserial`, `colorama`, `prompt-toolkit` (`requirements.txt`). Some CLI features shell out to native binaries in `software/script/bin/` compiled from `software/src/` (mfkey/nested/hardnested/darkside).

**Tests** (unittest, run from `software/script/` so imports resolve):
```bash
cd software/script
python -m unittest discover -s tests       # all
python tests/test_ultra.py                 # a single test file
```

**GUI** (separate repo `../ChameleonUltraGUI`, Flutter/Dart):
```bash
cd ../ChameleonUltraGUI/chameleonultragui
flutter run -d macos                                 # run
flutter analyze                                      # keep at 0 issues before committing
flutter gen-l10n                                     # after editing lib/l10n/*.arb
flutter test test/reader_key_recovery_test.dart      # focused pure-logic test
flutter test test/chameleon_command_queue_test.dart test/connection_capability_test.dart test/dfu_timeout_test.dart
flutter test test/ble_address_test.dart test/ble_presentation_test.dart test/ble_reliability_test.dart test/ble_responsive_layout_test.dart test/ble_audit_widgets_test.dart
LIBRECOVERY_PATH="$PWD/build/macos/recovery_test_arm64/librecovery.dylib" flutter test
```

The full GUI suite needs the native recovery dylib for MFKey/nested/hardnested
tests. On non-Apple hosts, point `LIBRECOVERY_PATH` at the locally built library
for that platform. Always run `git diff --check` in addition to analysis/tests.

**Dev scripts at this repo's root** (this fork):
- `flash_firmware.sh` — build the application firmware and flash it over USB DFU. Enters the bootloader via the client (raw `enter_dfu.py` is flaky on macOS), closes a running GUI first to free the serial port, retries programming, then verifies the advertised command count. `--package-only` prepares the signed app-only DFU without touching a connected device.
- `build_gui.sh [--release] [--no-run]` — build the GUI (macOS) and relaunch it.
- `build_ipa.sh` — build the iOS IPA and copy it to this repo's root.

## The command protocol (the core cross-cutting architecture)

Every host↔device operation is one request/response frame (no async push channel). Frame: SOF `0x11`, lrc, `cmd`(2 BE), `status`(2 BE), `len`(2 BE), lrc, `data[len]`, lrc. Max payload **4096** (`NETDATA_MAX_DATA_LENGTH`). The same protocol runs over **USB CDC or BLE NUS interchangeably** — the firmware picks the reply transport automatically in `auto_response_data()`.

Command IDs are 16-bit, grouped in thousands ranges: **1000s** device/system, **2000s** HF reader, **3000s** LF reader, **4000s** HF emulator, **5000s** LF emulator, **6000s** ISO14443-4 T=CL, **7000s** BLE.

The exhaustive wire reference is `docs/protocol-command-reference.md`. It lists
every assigned ID, request/response layout, status behavior, model availability,
persistence effects, and unassigned gaps (including why 6400 is an APDU status,
not a Chameleon command). Keep it synchronized whenever a handler changes.

**Command IDs are a manual mirror across three places** and must be kept in sync:
- `firmware/application/src/data_cmd.h` — `#define DATA_CMD_X (N)`
- `software/script/chameleon_enum.py` — `class Command(IntEnum)`
- (GUI repo) `lib/helpers/definitions.dart` — `enum ChameleonCommand`

Sync is enforced at **runtime**, not build time: firmware reports its full command list via `GET_DEVICE_CAPABILITIES` (1035) at connect, and `chameleon_com.py` raises if the client sends an id the device didn't advertise. The GUI caches that list once per `ChameleonCommunicator` and rejects unadvertised commands before writing. Firmware that explicitly reports 1035 as unsupported/not implemented is treated as legacy-unknown and allowed optimistically. `@enum.unique` on the Python enum catches duplicate values at import.

Firmware dispatch lives in `firmware/application/src/app_cmd.c`: the `m_data_cmd_map[]` table maps each `cmd` to `{cmd_before, cmd_processor, cmd_after}` hooks; `on_data_frame_received()` linearly scans it. Handlers have the signature `data_frame_tx_t *cmd_processor_x(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` and return `data_frame_make(cmd, STATUS_*, len, buf)`. Status codes are in `firmware/application/src/app_status.h` (e.g. `STATUS_SUCCESS` 0x68, `STATUS_PAR_ERR` 0x60).

**Streaming/continuous data has no push mechanism** — the idiom is "buffer in firmware, host pages by index": a `*_GET_COUNT` command plus a `*_GET_*` command that takes a start index and returns as many fixed records as fit in 4096 bytes (see the MFKey32 detection log, and the 7000-block BLE scan/fuzz/notification logs).

### GUI transport invariants

- `ChameleonCommunicator.sendCmd()` serializes requests, installs the response completer before writing, and matches only the active command ID. Unsolicited frames are discarded rather than retained for a future request.
- The protocol has no transaction/sequence ID. After a response timeout, the GUI quarantines that command ID; retrying it is unsafe. For an uncertain relay EXCHANGE (6012), an ordered empty STOP (6013) response on the same owner connection with status `0x68`, `0x60`, or `0x66` confirms the old session is closed and is a barrier that clears only the 6012 quarantine. Reconnect only if that reset cannot be confirmed or framing/transport was invalidated.
- USB/BLE writes have deadlines. A write timeout invalidates the communicator and disconnects because the underlying platform write may still finish later. Native serial uses libserialport's bounded blocking write and does not call unbounded `drain()`.
- DFU response and firmware-write timeouts likewise invalidate and disconnect the DFU transport. A timed-out DFU session must reconnect before sending another command.
- A communicator belongs to one connection. Publish it only after capability initialization succeeds, and call `dispose()` before disconnecting or replacing it.

### Adding a new command (the most common task)
1. `data_cmd.h`: add `#define DATA_CMD_X (N)` in the right range.
2. Write the firmware handler in the owning module (`app_cmd_ble.c` for BLE) and add a `{ DATA_CMD_X, before, cmd_processor_x, after }` row to `m_data_cmd_map[]` in `app_cmd.c`.
3. `chameleon_enum.py`: add `X = N` to `class Command`.
4. `chameleon_cmd.py`: add a `ChameleonCMD` method calling `self.device.send_cmd_sync(Command.X, payload)`.
5. `chameleon_cli_unit.py`: register a CLI command class under the command tree.
6. GUI `lib/helpers/definitions.dart`: add the same ID to `ChameleonCommand`.
7. GUI bridge: BLE methods belong in `lib/bridge/chameleon_ble.dart`; non-BLE methods stay in `lib/bridge/chameleon.dart`. Add operation-level capability gating when support is optional.
8. `CHANGELOG.md`: add a bullet under `## [unreleased]` — a CI check (`.github/workflows/changelog_reminder.yml`) reminds if missing.

## Host CLI layers (`software/script/`)

- `chameleon_cli_main.py` — entry point + prompt loop.
- `chameleon_cli_unit.py` — the command tree (`CLITree` from `chameleon_utils.py`; subgroups declared together near the top). Each action is a class decorated `@group.command("name")`, extending `BaseCLIUnit` → `DeviceRequiredUnit` (requires USB open) → `ReaderRequiredUnit` (also switches the device into reader mode), implementing `args_parser()` and `on_exec()`. The `@expect_response(Status.X)` decorator (in `chameleon_utils.py`) unwraps `resp.parsed` or raises.
- `chameleon_cmd.py` — `ChameleonCMD`, one thin method per device command.
- `chameleon_com.py` — serial transport, framing, and the capabilities check.
- `chameleon_enum.py` — `Command` IDs, `Status`, tag-type enums.

## GUI layers (`../ChameleonUltraGUI/chameleonultragui/lib/`)

- `bridge/chameleon.dart` — framing, serialized request/response transport, timeout quarantine, the capability cache, and non-BLE commands.
- `bridge/chameleon_ble.dart` — BLE command extension and all 7000-block payload/status parsing.
- `helpers/ble/ble_address.dart` — strict display-order MAC parsing, SoftDevice little-endian conversion, and static-random validation.
- `helpers/ble/ble_presentation.dart` — pure advertising, UUID, appearance, property, hex, and device-info parsing/formatting.
- `gui/menu/hacking/ble_app.dart` — three-tab Bluetooth hub. `ble_audit.dart`, `ble_radio_identity.dart`, and `ble_stress.dart` own the tool state machines.
- `ble_capability_gate.dart` gates complete operations, not the entire hub. Scope-dependent commands must disable only the unsupported scope/action.
- `ble_audit_status.dart`, `ble_characteristic_tile.dart`, and `ble_responsive.dart` are stateless presentation modules. Preserve scrollable tabs, wrapped actions, and narrow-screen/large-text behavior.
- BLE-visible prose belongs in `lib/l10n/app_en.arb`; run `flutter gen-l10n`. Bluetooth SIG names and wire-level tokens may remain canonical technical text.

## Firmware structure (`firmware/application/src/`)

- `app_main.c` — `main()` and the main loop that pumps `data_frame_process()`.
- `app_cmd.c` / `data_cmd.h` — central command dispatch table and command IDs. BLE command handlers live in `app_cmd_ble.c`; keep the dispatch rows in `app_cmd.c`.
- `ble_main.c` — BLE **peripheral** behavior (advertising, NUS command transport, battery service, pairing). Passive observer scanning and retained scan records live in `ble_scan.c`; the central-role GATT client lives in `ble_central.c`: connect to one target, discover primary services / characteristics / descriptors, read/write a value, subscribe to notifications, link-probe, exchange MTU, and the directed fuzzer. See `docs/ble-audit.md` for the full BLE tool reference.
- `rfid_main.c` and `rfid/` — HF (RC522/ISO14443) and LF (125 kHz) readers and tag emulators; slots (8) hold emulated tags.
- `nrf52_sdk/` — vendored Nordic SDK; `sdk_config.h` configures the SoftDevice/SDK.
- `application.ld` — memory layout. The RAM `ORIGIN` is coupled to the SoftDevice config: changing BLE link counts (`NRF_SDH_BLE_CENTRAL_LINK_COUNT` etc. in `sdk_config.h`) changes the SoftDevice's required RAM start, and `nrf_sdh_ble_enable` asserts `NRF_ERROR_NO_MEM` at boot if `ORIGIN` is too low (it logs the exact value to use).

**Ultra vs Lite**: code guarded by `#if defined(PROJECT_CHAMELEON_ULTRA)` is Ultra-only (the Lite lacks the HF/LF reader front-end); device-level and BLE commands compile for both. Keep new reader/emulator handlers inside that guard, and device/BLE handlers outside it, matching the existing dispatch-table layout.

## This fork's additions (branch `cybersecurity`)

Tooling layered on top of the upstream project. Each area has a reference under `docs/` (`docs/README.md` indexes them):

- **BLE audit** (commands 7000–7054, `docs/ble-audit.md`) — passive scanner + directed GATT client: connect, discover services/characteristics/descriptors, read, write, subscribe, device-info pull, MTU exchange, link-probe, and the directed fuzzer (7000–7032); plus own-radio identity/power control (7040–7043), operator-authorised stress/broadcast tooling (7044–7051, see fork-specific exemption), and the advertising lab (7052–7054, `docs/ble-advertising-lab.md`) with generic/raw profiles and bounded pairing-discovery sheets. Stress scope is selectable per call: single target, scan-buffer-wide, or environment-wide broadcast. Firmware is split across `ble_scan.c`, `ble_central.c`, `ble_main.c`, and `app_cmd_ble.c`; CLI is under `ble …`; the GUI uses the four-tab `ble_app.dart` hub.
- **HF reader additions** (`docs/hf-additions.md`) — `MF1_READ_BLOCKS` (2018, authenticate-once sector read; used by `hf mf dump`/autopwn), `HF14A_SCAN_KEEP` (2016) / `HF14A_AUTH_TRACE` (2017), `HF14A_4_DESFIRE_SCAN` (6006, one-call DESFire enum via `hf des enum`), and a richer `emv scan`. These build on the upstream `MF1_CHECK_KEYS_OF_SECTORS` (2012), which the fork's key-check flow uses but did not add.
- **MIFARE Classic key-recovery optimizations** (`docs/autopwn-optimizations.md`) — live in the **GUI** engine `lib/helpers/mifare_classic/recovery.dart` (`MifareClassicRecovery`; orchestrates dictionary check → darkside/nested/hardnested/static/RF08S-backdoor → dump via the native FFI in `lib/recovery/`). The order-only ranking/intersection logic is factored into the pure, unit-tested `lib/helpers/mifare_classic/candidate_priority.dart` (`test/candidate_priority_test.dart`). All changes are **speed-/order-only and confirmed on-card** — they never mark a wrong key. When touching recovery, preserve that invariant and the per-step fallbacks.
- **Autopwn+** — a separate GUI workflow; do not change legacy Autopwn behavior to implement Autopwn+ features. The adaptive planner/runner is in `lib/helpers/mifare_classic/autopwn_plus.dart`, the page is `lib/gui/menu/hacking/autopwn_plus.dart`, and tests are in `test/autopwn_plus_test.dart`. It supports Quick/Balanced/Deep profiles, ranked dictionary waves, selected sector ranges, cooperative cancellation, card identity guards, verified-key-only export, lossless partial dumps, and measurable-phase ETA. Hardnested ETA covers nonce collection only; native solver and weak/static nested duration remain variable.
- **Reader-key capture / MFKey32** — firmware commands 4004-4007 retain up to 1000 completed MIFARE Classic authentication transcripts (18 bytes each) in `.noinit_mf1`. Enabling detection clears the log; disabling it preserves the log. The GUI parser validates status, page framing, and count, then `lib/helpers/mifare_classic/reader_key_recovery.dart` groups by `(CUID, sector, Key A/B)`, deduplicates exact transcripts, rejects the full `UINT64_MAX` no-key sentinel before truncating to 48 bits, and performs bounded same-target and shared-key cross-sector pairing. Keep one result per sector/key type even when values repeat; deduplicate values only for dictionary export. A key cannot be recovered unless the reader produced at least two usable transcripts for it, possibly across sectors that reuse the key. Use fixed UID capture for reliable grouping. Tests live in `test/reader_key_recovery_test.dart` and `test/chameleon_command_queue_test.dart`.
- **Retained EMV trace and ISO-DEP diagnostics** (commands 6007-6010) — firmware captures a versioned, paged, CRC32-validated, lossless APDU/RF trace with explicit session, completion, and truncation metadata; 6010 returns ISO-DEP debug counters. CLI and GUI must reject malformed metadata/pages rather than exporting partial or reordered evidence. See the APDU/EMV documents indexed by `docs/README.md`.
- **Authorized ISO-DEP relay** (commands 6011-6014, `docs/authorized-iso-dep-relay.md`) — after strict PPSE preparation, Android HCE and backend START polling rendezvous in either order. Normal START is 6011; explicit Apple Transit mode is persisted locally across relay sessions and app restarts until disabled, uses 6014 for bounded ECP2 polling, and rewrites only GPO `9F66`, `9F35`, and `9F33` from the selected AID's exact PDOL, failing closed without changing card responses. Firmware sessions and GUI/native arms have no inactivity lease: preserve positive arm-token checks, exact USB/BLE command-transport ownership, one pending APDU, no retry after uncertain START or 6012, ordered 6013 reset-barrier recovery for uncertain 6012, and fail-closed cleanup. Android HCE cannot force terminal-facing WTX, so phone-first success still requires acquisition plus exchange inside the per-terminal-APDU deadline.
- **Emulated-tag change history** — the GUI can poll the active MIFARE Classic slot, diff 16-byte blocks, archive the latest 100 before/after events on the phone, notify in-app, and request a slot save. Core model/diff code is `lib/helpers/emulation_change.dart`; lifecycle ownership is in `ChameleonGUIState`; UI is `lib/gui/menu/tools/emulation_change_history.dart`. Monitoring is opt-in, requires a connected/running app, and Normal write mode is required for reader writes to survive a device restart. Shadow mode changes remain RAM-only by design.
- **Keyboard payloads** (`docs/keyboard-payloads.md`, `docs/keyboard-command-reference.md`) — RAM-only Ducky-style scripts uploaded over USB/BLE with run/cancel/status controls and USB/BLE HID output. Keep compiler limits, layout handling, and bridge payload validation aligned across firmware, CLI, and GUI.
- **GUI data sync** (`docs/data-sync.md`) — encrypted direct app-to-app transfer and password-protected bundles for cards, dictionaries, scripts, and explicitly safe settings. Preserve versioning, bounded inputs, authenticated encryption, and the exclusion of secrets/unsafe runtime state.

## Current safety and persistence constraints

- Firmware runtime/release hardening is present on branch `cybersecurity` (baseline commit `a625c63`). Do not weaken command length/status validation, BLE operation state checks, FDS bounds, ISO-DEP framing, or release checks when adding features.
- DFU signing keys must be supplied from outside the checkout. The legacy `resource/dfu_key/chameleon.pem` is compromised and retained only for migration compatibility; do not use it for a new release trust root.
- App-only DFU remains disabled until the enlarged FDS-aware bootloader has been deployed and `ALLOW_APP_ONLY_DFU=1` explicitly acknowledges that migration.
- EM4x05 scanning currently returns `STATUS_NOT_IMPLEMENTED`; do not present it as functional in a host client.
- RF/BLE/FDS/DFU behavior still needs physical Ultra/Lite validation. Hardware-free tests prove parsing, orchestration, and invariants, not field performance or protocol interoperability.

## Conventions

- Multi-byte protocol fields are **big-endian** on the wire (`U16HTONS`/`struct.pack('!...')`); BLE GAP addresses are little-endian as the SoftDevice reports them.

## Fork-specific exemptions (branch `cybersecurity`)

This fork is maintained by the branch owner with **explicit authorisation** to relax the upstream "no jamming / DoS / mass-spam" rule in this checkout. Everything BlueKit does (and the upstream declines) is in scope here, with explicit per-command scope selection (single-target / scan-buffer-wide / environment-wide broadcast) at runtime.

Scope of the exemption:
- Applies **only** to this fork (`branch = cybersecurity`). Do not propagate it to the upstream `RfidResearchGroup/ChameleonUltra` repository.
- Operators are expected to target hardware / devices they own or are explicitly authorised to test (lab equipment, own accessories, CTF / education kits). Use against third-party devices without written consent is out of scope and out of bounds.
- **All 7000-block commands in this fork are allowed to be used as environment-wide broadcast / flood / mass-spam tools at the operator's discretion**, scoped per-call (single-target / scan-buffer-wide / full environment-wide broadcast on the 2.4 GHz BLE spectrum). The CLI and GUI surface this as an explicit `scope` selector on every relevant command; there is no implicit downgrade to point-to-point.


If the maintainer of this fork loses authorisation or the upstream policy changes incompatibly, this section must be reverted in the same commit that removes the affected commands.


# IMPORTANT
NEVER USE WORKTREES USE DIRECTLY THE FOLDER