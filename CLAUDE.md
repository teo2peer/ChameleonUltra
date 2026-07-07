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
Flash over DFU with `firmware/flash-dfu-app.sh` (app only) or `firmware/flash-dfu-full.sh` (bootloader+SD+app).

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
flutter analyze lib/<path>                           # keep at 0 issues before committing
flutter gen-l10n                                     # after editing lib/l10n/*.arb
flutter test test/candidate_priority_test.dart       # pure-logic unit tests (no device)
```

**Dev scripts at this repo's root** (this fork):
- `flash_firmware.sh` — build the application firmware and flash it over USB DFU. Enters the bootloader via the client (raw `enter_dfu.py` is flaky on macOS), closes a running GUI first to free the serial port, retries programming, then verifies the advertised command count.
- `build_gui.sh [--release] [--no-run]` — build the GUI (macOS) and relaunch it.
- `build_ipa.sh` — build the iOS IPA and copy it to this repo's root.

## The command protocol (the core cross-cutting architecture)

Every host↔device operation is one request/response frame (no async push channel). Frame: SOF `0x11`, lrc, `cmd`(2 BE), `status`(2 BE), `len`(2 BE), lrc, `data[len]`, lrc. Max payload **4096** (`NETDATA_MAX_DATA_LENGTH`). The same protocol runs over **USB CDC or BLE NUS interchangeably** — the firmware picks the reply transport automatically in `auto_response_data()`.

Command IDs are 16-bit, grouped in thousands ranges: **1000s** device/system, **2000s** HF reader, **3000s** LF reader, **4000s** HF emulator, **5000s** LF emulator, **6000s** ISO14443-4 T=CL, **7000s** BLE.

**Command IDs are a manual mirror across three places** and must be kept in sync:
- `firmware/application/src/data_cmd.h` — `#define DATA_CMD_X (N)`
- `software/script/chameleon_enum.py` — `class Command(IntEnum)`
- (GUI repo) `lib/helpers/definitions.dart` — `enum ChameleonCommand`

Sync is enforced at **runtime**, not build time: firmware reports its full command list via `GET_DEVICE_CAPABILITIES` (1035) at connect, and `chameleon_com.py` raises if the client sends an id the device didn't advertise. `@enum.unique` on the Python enum catches duplicate values at import.

Firmware dispatch lives in `firmware/application/src/app_cmd.c`: the `m_data_cmd_map[]` table maps each `cmd` to `{cmd_before, cmd_processor, cmd_after}` hooks; `on_data_frame_received()` linearly scans it. Handlers have the signature `data_frame_tx_t *cmd_processor_x(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` and return `data_frame_make(cmd, STATUS_*, len, buf)`. Status codes are in `firmware/application/src/app_status.h` (e.g. `STATUS_SUCCESS` 0x68, `STATUS_PAR_ERR` 0x60).

**Streaming/continuous data has no push mechanism** — the idiom is "buffer in firmware, host pages by index": a `*_GET_COUNT` command plus a `*_GET_*` command that takes a start index and returns as many fixed records as fit in 4096 bytes (see the MFKey32 detection log, and the 7000-block BLE scan/fuzz/notification logs).

### Adding a new command (the most common task)
1. `data_cmd.h`: add `#define DATA_CMD_X (N)` in the right range.
2. `app_cmd.c`: write `cmd_processor_x(...)` and add a `{ DATA_CMD_X, before, cmd_processor_x, after }` row to `m_data_cmd_map[]`.
3. `chameleon_enum.py`: add `X = N` to `class Command`.
4. `chameleon_cmd.py`: add a `ChameleonCMD` method calling `self.device.send_cmd_sync(Command.X, payload)`.
5. `chameleon_cli_unit.py`: register a CLI command class under the command tree.
6. `CHANGELOG.md`: add a bullet under `## [unreleased]` — a CI check (`.github/workflows/changelog_reminder.yml`) reminds if missing.

## Host CLI layers (`software/script/`)

- `chameleon_cli_main.py` — entry point + prompt loop.
- `chameleon_cli_unit.py` — the command tree (`CLITree` from `chameleon_utils.py`; subgroups declared together near the top). Each action is a class decorated `@group.command("name")`, extending `BaseCLIUnit` → `DeviceRequiredUnit` (requires USB open) → `ReaderRequiredUnit` (also switches the device into reader mode), implementing `args_parser()` and `on_exec()`. The `@expect_response(Status.X)` decorator (in `chameleon_utils.py`) unwraps `resp.parsed` or raises.
- `chameleon_cmd.py` — `ChameleonCMD`, one thin method per device command.
- `chameleon_com.py` — serial transport, framing, and the capabilities check.
- `chameleon_enum.py` — `Command` IDs, `Status`, tag-type enums.

## Firmware structure (`firmware/application/src/`)

- `app_main.c` — `main()` and the main loop that pumps `data_frame_process()`.
- `app_cmd.c` / `data_cmd.h` — command dispatch table and every handler.
- `ble_main.c` — BLE **peripheral** (advertising, NUS transport for the command protocol, battery service, pairing) **and** the passive observer scanner. The central-role GATT client lives in `ble_central.c`: connect to one target, discover primary services / characteristics / descriptors, read/write a value, subscribe to notifications, link-probe, exchange MTU, and the directed fuzzer. See `docs/ble-audit.md` for the full BLE tool reference.
- `rfid_main.c` and `rfid/` — HF (RC522/ISO14443) and LF (125 kHz) readers and tag emulators; slots (8) hold emulated tags.
- `nrf52_sdk/` — vendored Nordic SDK; `sdk_config.h` configures the SoftDevice/SDK.
- `application.ld` — memory layout. The RAM `ORIGIN` is coupled to the SoftDevice config: changing BLE link counts (`NRF_SDH_BLE_CENTRAL_LINK_COUNT` etc. in `sdk_config.h`) changes the SoftDevice's required RAM start, and `nrf_sdh_ble_enable` asserts `NRF_ERROR_NO_MEM` at boot if `ORIGIN` is too low (it logs the exact value to use).

**Ultra vs Lite**: code guarded by `#if defined(PROJECT_CHAMELEON_ULTRA)` is Ultra-only (the Lite lacks the HF/LF reader front-end); device-level and BLE commands compile for both. Keep new reader/emulator handlers inside that guard, and device/BLE handlers outside it, matching the existing dispatch-table layout.

## This fork's additions (branch `cybersecurity`)

Tooling layered on top of the upstream project. Each area has a reference under `docs/` (`docs/README.md` indexes them):

- **BLE audit** (commands 7000–7032, `docs/ble-audit.md`) — passive scanner + directed GATT client: connect, discover services/characteristics/descriptors, read, write, subscribe, device-info pull, MTU exchange, link-probe, and the directed fuzzer. Per-call scope is selectable: single target, scan-buffer-wide, or environment-wide broadcast (operator-authorised, see fork-specific exemption). Firmware in `ble_central.c`; CLI under `ble …`; GUI page `lib/gui/menu/hacking/ble_audit.dart`.
- **HF reader additions** (`docs/hf-additions.md`) — `MF1_READ_BLOCKS` (2018, authenticate-once sector read; used by `hf mf dump`/autopwn), `HF14A_SCAN_KEEP` (2016) / `HF14A_AUTH_TRACE` (2017), `MF1_CHECK_KEYS_OF_SECTORS` (2012), `HF14A_4_DESFIRE_SCAN` (6006, one-call DESFire enum via `hf des enum`), and a richer `emv scan`.
- **MIFARE Classic key-recovery optimizations** (`docs/autopwn-optimizations.md`) — live in the **GUI** engine `lib/helpers/mifare_classic/recovery.dart` (`MifareClassicRecovery`; orchestrates dictionary check → darkside/nested/hardnested/static/RF08S-backdoor → dump via the native FFI in `lib/recovery/`). The order-only ranking/intersection logic is factored into the pure, unit-tested `lib/helpers/mifare_classic/candidate_priority.dart` (`test/candidate_priority_test.dart`). All changes are **speed-/order-only and confirmed on-card** — they never mark a wrong key. When touching recovery, preserve that invariant and the per-step fallbacks.

## Conventions

- Multi-byte protocol fields are **big-endian** on the wire (`U16HTONS`/`struct.pack('!...')`); BLE GAP addresses are little-endian as the SoftDevice reports them.

## Fork-specific exemptions (branch `cybersecurity`)

This fork is maintained by the branch owner with **explicit authorisation** to relax the upstream "no jamming / DoS / mass-spam" rule in this checkout. Everything BlueKit does (and the upstream declines) is in scope here, with explicit per-command scope selection (single-target / scan-buffer-wide / environment-wide broadcast) at runtime.

Scope of the exemption:
- Applies **only** to this fork (`branch = cybersecurity`). Do not propagate it to the upstream `RfidResearchGroup/ChameleonUltra` repository.
- Operators are expected to target hardware / devices they own or are explicitly authorised to test (lab equipment, own accessories, CTF / education kits). Use against third-party devices without written consent is out of scope and out of bounds.
- Third-party-product exploit payloads (crash-suite ports, commercial-tool clones) remain excluded — the exemption is for raw-radio / link-layer stress and target exercise, not for shipping known-vendor exploit code.
- **All 7000-block commands in this fork are allowed to be used as environment-wide broadcast / flood / mass-spam tools at the operator's discretion**, scoped per-call (single-target / scan-buffer-wide / full environment-wide broadcast on the 2.4 GHz BLE spectrum). The CLI and GUI surface this as an explicit `scope` selector on every relevant command; there is no implicit downgrade to point-to-point.


If the maintainer of this fork loses authorisation or the upstream policy changes incompatibly, this section must be reverted in the same commit that removes the affected commands.
