# Keyboard Payloads

See the [complete keyboard command reference](keyboard-command-reference.md)
for every script statement, key/modifier name, CLI command, GUI action, and wire
format.

The cybersecurity firmware can act as a USB or BLE HID keyboard and execute a
small, bounded Ducky-style script. Use it only on systems you own or are
explicitly authorized to test.

## Opt-in (off by default)

The keyboard HID feature is **disabled by default**. A disabled device exposes
no USB HID interface, advertises no HID service UUID, registers no BLE HID GATT
service, and forces no BLE pairing — it behaves exactly as a device without the
feature. Enable it explicitly and reboot before use:

- CLI: `hw keyboard enable on` (then reboot). `hw keyboard enable` shows the
  current state; `hw keyboard enable off` disables it again.
- Wire: `SET_KEYBOARD_HID_ENABLE` (1052) sets the flag, `SAVE_SETTINGS` (1013)
  persists it, and a reboot applies it (USB re-enumeration + BLE HID
  registration). `GET_KEYBOARD_HID_ENABLE` (1051) reads it.

While disabled, `KEYBOARD_RUN` (1044) and `KEYBOARD_ARM_BLE` (1049) return
`NOT_IMPLEMENTED`. Registering the BLE HID service never asserts: if the GATT
attribute table is exhausted it is reported as a command error instead of
resetting the device.

## Transport Model

- Upload, status, run, cancel, and clear commands work over USB CDC or BLE NUS.
- All keyboard commands sent over BLE require an encrypted, MITM-protected LE
  Secure Connections link. USB commands use the open CDC command connection.
- A run can send keyboard reports to USB HID, BLE HID, or both.
- When the feature is enabled, the Chameleon is a composite CDC ACM and USB HID
  keyboard device, so the CLI and USB keyboard output operate through the same
  cable. When disabled it is a plain CDC ACM device.
- Either physical button cancels a running payload and releases all keys.
- Losing the USB or BLE command link that started a run cancels that run.
- After installing firmware that adds BLE HID, remove the device's old bond on
  the host and pair it again if the operating system still shows only NUS/BAS.

The GUI exposes a **Pair BLE host** menu whenever BLE output is selected. If a
run is rejected because no BLE HID host is attached, the same menu opens
automatically. It can enable Chameleon pairing, show the current passkey, and
open Bluetooth settings on the GUI host. The receiving computer or phone must
select `ChameleonUltra` and initiate pairing because BLE HID keyboards are
peripherals and cannot initiate a connection to a host.

Payloads are held only in RAM and are lost when the device resets. Upload uses a
staging buffer, CRC32 verification, bytecode validation, and atomic commit, so a
failed replacement does not overwrite the previous committed payload.

## CLI

Compile a script without a device:

```bash
hw keyboard compile payload.txt --out payload.bin
hw keyboard compile payload-es.txt --layout es --out payload-es.bin
```

Upload and commit it through the current USB or BLE command connection:

```bash
hw keyboard upload payload.txt
hw keyboard upload payload-de.txt --layout de
```

Run it as a USB keyboard:

```bash
hw keyboard run --output usb
```

Other output choices are `ble` and `both`. Inspect or stop execution with:

```bash
hw keyboard status
hw keyboard cancel
hw keyboard clear
```

Set a volatile Bluetooth name without resetting or intentionally disconnecting
the current peripheral link:

```bash
hw keyboard name "Lab Keyboard"
hw keyboard name                    # restore ChameleonUltra/ChameleonLite
```

To advertise and execute the committed payload once when a BLE HID host connects:

```bash
hw keyboard arm --name "Lab Keyboard"
```

The arm command restores the default name when it triggers or is cancelled. It
starts connectable advertising immediately when the peripheral slot is free. If
the GUI currently controls Chameleon over BLE, that one peripheral slot remains
occupied and no name is being advertised. The GUI must disconnect after arming;
firmware then restarts general-discoverable advertising without the existing
bond whitelist so the custom name is visible to a receiving host. The payload
remains armed across that pre-trigger disconnect. Some operating systems cache
the old name for an already bonded address; remove/forget that old bond when
testing a changed name on the same host.

In the GUI, **Advertise and arm** compiles and uploads the current editor source
when necessary, applies the temporary name (or restores the board default), and
arms the committed payload in one operation.

BLE keyboards cannot force nearby devices to connect. The receiving computer or
phone must explicitly select the advertised name, pair with MITM/LE Secure
Connections, and enable HID notifications. Pairing can be enabled at runtime in
the GUI; a reboot is no longer required for the pairing-enable change itself.

## Saved Scripts

The GUI library stores up to 255 named scripts in the app's local
preferences. Saving records the editable source, keyboard layout, output choice,
and precompiled bytecode. Open **Saved scripts** from the keyboard page toolbar
to load, delete, or quick-launch an entry. Quick launch uploads the stored
bytecode and starts it immediately, without reopening or recompiling the source.
Firmware still validates every uploaded program before committing it.

Saved scripts remain on the phone or computer running the GUI and are not copied
to Chameleon flash. They are ordinary app preferences, not encrypted secret
storage, so scripts should not contain credentials that require protected at-rest
storage.

## Script Format

The compiler targets `us` by default. Select `us`, `uk`, `es`, `de`, `fr`, `it`,
or `pt` with `--layout` on both CLI `compile` and `upload`, or use the Keyboard
layout selector in the GUI. The host receiving the keystrokes must use the same
layout. The compiler supports:

```text
REM Common DuckyScript syntax is supported.
GUI r
DELAY 1000
STRING notepad
ENTER
CTRL SHIFT s
```

- `REM` and lines beginning with `#` are comments.
- `STRING` types text in the selected layout; `STRINGLN` also presses Enter.
- `TEXT` is JSON-quoted and supports the same layout-aware Unicode text. Text is
  normalized to NFC. National characters may compile to a direct, AltGr, or
  deterministic dead-key sequence.
- `DELAY` accepts 1 through 10000 milliseconds per instruction.
- Named keys such as `ENTER`, `TAB`, `DELETE`, arrows, and `F1` through `F12`
  can appear directly on a line.
- Modifier lines support `CTRL`, `SHIFT`, `ALT`, `GUI`, `WINDOWS`, `COMMAND`,
  and their explicit left/right forms followed by one key.
- The original strict `KEY` and plus-separated `CHORD` forms remain available.
  Their logical letter and punctuation keys follow the selected layout.

Supported national text includes Spanish n-tilde, acute vowels, u-diaeresis,
inverted punctuation, and c-cedilla; German umlauts, sharp-s, and common
dead-key accents; French direct lowercase accents plus circumflex/diaeresis
compositions; Italian direct lowercase accents; Portuguese accent
compositions, ordinals, and guillemets; and UK pound, euro, not, and broken-bar
symbols. Legacy French and Italian layouts cannot produce their uppercase
direct accents statelessly, so those characters are rejected.

Firmware limits payloads to 4096 bytes, 1024 key taps, and 60000 milliseconds
of explicit delay. Execution is nonblocking and always attempts an all-keys-up
report on completion, cancellation, or error.

## Protocol Commands

| ID | Command | Purpose |
| ---: | --- | --- |
| 1041 | `KEYBOARD_UPLOAD_BEGIN` | Declare length and CRC32 |
| 1042 | `KEYBOARD_UPLOAD_CHUNK` | Upload sequential bytes |
| 1043 | `KEYBOARD_UPLOAD_COMMIT` | Validate and atomically commit |
| 1044 | `KEYBOARD_RUN` | Start USB, BLE, or dual HID output |
| 1045 | `KEYBOARD_CANCEL` | Stop a run or abandon an upload |
| 1046 | `KEYBOARD_GET_STATUS` | Read upload and execution state |
| 1047 | `KEYBOARD_CLEAR` | Zero staged and committed RAM |
| 1048 | `KEYBOARD_SET_TEMP_BLE_NAME` | Set or restore the volatile GAP/advertising name |
| 1049 | `KEYBOARD_ARM_BLE` | Advertise and run once after a secure BLE HID connection |
