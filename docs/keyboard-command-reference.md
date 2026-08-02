# Keyboard Command Reference

This document lists every keyboard-script statement, key name, CLI operation,
and firmware command currently supported by this fork.

Use keyboard payloads only on systems you own or are explicitly authorized to
test.

## Script Statements

Primary statement names are uppercase and one statement is accepted per line.
Blank lines and lines beginning with `#` are ignored.

| Syntax | Description | Example |
| --- | --- | --- |
| `REM text` | Comment | `REM Open the editor` |
| `# text` | Comment | `# Linux path` |
| `STRING text` | Type text using the selected layout | `STRING Hello world` |
| `STRINGLN text` | Type text, then press Enter | `STRINGLN whoami` |
| `TEXT "json string"` | Type a JSON-quoted string | `TEXT " leading and trailing "` |
| `DELAY milliseconds` | Wait 1 through 10000 ms | `DELAY 750` |
| `KEY name` | Press one named key | `KEY ENTER` |
| `CHORD modifier+key` | Press modifiers and one key | `CHORD LCTRL+LSHIFT+S` |
| `MODIFIER ... key` | Common DuckyScript chord form | `CTRL SHIFT S` |
| `key` | Press a bare named key | `ENTER` |

`TEXT` supports JSON escapes such as `\"`, `\\`, and `\u00E9`. Use `TEXT` when
leading or trailing spaces must be preserved. Escapes that decode to untypable
control characters, including `\n` and `\t`, are rejected; use `ENTER` or `TAB`
instead.

Examples:

```text
REM Windows Run dialog
GUI r
DELAY 500
STRINGLN notepad
DELAY 500
TEXT "Hello from Chameleon!"
ENTER
CTRL SHIFT s
```

```text
# Strict legacy forms remain supported
KEY TAB
CHORD LCTRL+LALT+DELETE
```

## Modifiers

The following modifier names are accepted in Ducky-style modifier lines.

| Name | Effective modifier |
| --- | --- |
| `CTRL`, `CONTROL` | Left Control |
| `SHIFT` | Left Shift |
| `ALT` | Left Alt |
| `GUI`, `WINDOWS`, `COMMAND` | Left GUI/Windows/Command |
| `LCTRL` | Left Control |
| `LSHIFT` | Left Shift |
| `LALT` | Left Alt |
| `LGUI` | Left GUI |
| `RCTRL` | Right Control |
| `RSHIFT` | Right Shift |
| `RALT` | Right Alt/AltGr |
| `RGUI` | Right GUI |

Multiple distinct modifiers can precede exactly one key:

```text
CTRL ALT DELETE
RCTRL RSHIFT F12
GUI r
```

The strict `CHORD` form uses `+` separators and uppercase names:

```text
CHORD LCTRL+C
CHORD LCTRL+LSHIFT+S
CHORD LCTRL++
```

Duplicate modifiers and chords containing more than one non-modifier key are
rejected.

## Named Keys

These names can be used bare, after `KEY`, in `CHORD`, or as the final key in a
modifier line:

| Category | Names |
| --- | --- |
| Editing | `ENTER`, `ESC`, `TAB`, `SPACE`, `BACKSPACE`, `DELETE` |
| Arrows | `UP`, `DOWN`, `LEFT`, `RIGHT` |
| Navigation | `HOME`, `END`, `PAGEUP`, `PAGEDOWN` |
| Functions | `F1`, `F2`, `F3`, `F4`, `F5`, `F6`, `F7`, `F8`, `F9`, `F10`, `F11`, `F12` |

Uppercase letters `A` through `Z`, digits `0` through `9`, and supported
single-character punctuation can also be chord keys. Text case belongs in
`STRING`, `STRINGLN`, or `TEXT`; lowercase one-character `KEY`/`CHORD` keys are
rejected to avoid ambiguous physical-key behavior.

## Punctuation Key Names

Punctuation follows the selected host layout. These symbolic names are
available where a single key is expected:

| Character | Name | Character | Name |
| --- | --- | --- | --- |
| `-` | `MINUS` | `_` | `UNDERSCORE` |
| `=` | `EQUAL` | `+` | `PLUS` |
| `[` | `LEFTBRACKET` | `{` | `LEFTBRACE` |
| `]` | `RIGHTBRACKET` | `}` | `RIGHTBRACE` |
| `\` | `BACKSLASH` | `|` | `PIPE` |
| `;` | `SEMICOLON` | `:` | `COLON` |
| `'` | `APOSTROPHE` | `"` | `QUOTE` |
| `` ` `` | `GRAVE` | `~` | `TILDE` |
| `,` | `COMMA` | `<` | `LESS` |
| `.` | `PERIOD` | `>` | `GREATER` |
| `/` | `SLASH` | `?` | `QUESTION` |
| `!` | `EXCLAMATION` | `@` | `AT` |
| `#` | `HASH` | `$` | `DOLLAR` |
| `%` | `PERCENT` | `^` | `CARET` |
| `&` | `AMPERSAND` | `*` | `ASTERISK` |
| `(` | `LEFTPAREN` | `)` | `RIGHTPAREN` |

Examples:

```text
CTRL SLASH
KEY BACKSLASH
CHORD LCTRL++
```

## Keyboard Layouts

| Option | Layout |
| --- | --- |
| `us` | US English |
| `uk` | UK English |
| `es` | Spanish |
| `de` | German QWERTZ |
| `fr` | French AZERTY |
| `it` | Italian |
| `pt` | Portuguese |

The receiving operating system must use the same layout selected in the GUI or
with `--layout`. Text supports direct, Shift, AltGr, and deterministic dead-key
sequences where the chosen layout can produce the character.

## CLI Commands

Commands are entered in the interactive Python CLI.

### `hw keyboard compile`

Compile without connecting to a device:

```text
hw keyboard compile SCRIPT [--layout us|uk|es|de|fr|it|pt] [--out FILE]
```

Without `--out`, compiled bytecode is printed as hexadecimal.

### `hw keyboard upload`

Compile, upload, validate, and commit a script:

```text
hw keyboard upload SCRIPT [--layout us|uk|es|de|fr|it|pt]
```

### `hw keyboard status`

Display upload, commit, run, output, error, and CRC state:

```text
hw keyboard status
```

### `hw keyboard run`

Immediately execute the committed payload:

```text
hw keyboard run --output usb
hw keyboard run --output ble
hw keyboard run --output both
```

The selected HID output must already be ready.

### `hw keyboard name`

Set a volatile Bluetooth GAP and advertising name:

```text
hw keyboard name "Lab Keyboard"
```

Restore the compile-time board name:

```text
hw keyboard name
```

Names must be valid UTF-8, contain no control characters, and encode to at most
26 bytes. Updating the name does not intentionally disconnect the current BLE
peripheral link. If connected, the new advertising name is staged for the next
advertising start.

### `hw keyboard arm`

Advertise and execute the committed payload once after a secure BLE HID host
explicitly connects and enables keyboard notifications:

```text
hw keyboard arm [--name "Temporary name"]
```

Omitting `--name` restores and advertises the default board name. The command
reserves a run ID immediately. The payload remains armed across a pre-trigger
disconnect and does not depend on the original USB/BLE command link.
When arming through the BLE GUI connection, disconnect that GUI after the arm
response. Firmware then restarts advertising without the bond whitelist. A host
that already bonded the same address may display its cached old name until that
bond is forgotten.

### `hw keyboard cancel`

Cancel a running payload, disarm an armed payload, or abandon an active upload:

```text
hw keyboard cancel
```

Disarming restores the default board name but retains the committed payload.

### `hw keyboard clear`

Disarm, release keys, restore the default name, and zero staged and committed
payload RAM:

```text
hw keyboard clear
```

## GUI Actions

The Keyboard Payload page provides:

| Action | Behavior |
| --- | --- |
| **Compile** | Compile the editor source for the selected layout |
| **Save script** | Store source, layout, output, and precompiled bytecode in the GUI |
| **Saved scripts** | Load, delete, or quick-launch up to 255 local scripts |
| **Upload** | Upload and atomically commit the compiled program |
| **Status** | Refresh the fixed firmware status record |
| **Run** | Execute immediately over USB, BLE, or both |
| **Pair BLE host** | Enable pairing, display the passkey, and open Bluetooth settings |
| **Apply temporary name** | Change the volatile BLE name without an intentional disconnect |
| **Restore default name** | Restore `ChameleonUltra` or `ChameleonLite` |
| **Advertise and arm** | Compile/upload if needed, set the name, advertise, and wait for one secure HID host |
| **Cancel** | Cancel execution or disarm |
| **Clear** | Clear payload RAM and restore the default name |

A BLE keyboard cannot force nearby systems to connect. The receiving host must
select the advertised name and approve pairing. The firmware supports one BLE
peripheral link, so a mobile GUI connected over BLE occupies the slot until it
disconnects or acts as the HID host itself.

## Firmware Protocol Commands

All multi-byte fields are big-endian. Requests and responses below describe the
command data payload inside the normal Chameleon frame. `version` is currently
`1`.

| ID | Command | Request data | Success response data |
| ---: | --- | --- | --- |
| 1041 | `KEYBOARD_UPLOAD_BEGIN` | `version:u8, total:u16, crc32:u32` | `version:u8, upload_id:u32, next_offset:u16, max_chunk:u16` |
| 1042 | `KEYBOARD_UPLOAD_CHUNK` | `version:u8, upload_id:u32, offset:u16, bytes` | `upload_id:u32, next_offset:u16` |
| 1043 | `KEYBOARD_UPLOAD_COMMIT` | `version:u8, upload_id:u32` | `commit_id:u32, length:u16, crc32:u32` |
| 1044 | `KEYBOARD_RUN` | `version:u8, commit_id:u32, outputs:u8` | `run_id:u32` |
| 1045 | `KEYBOARD_CANCEL` | Empty | Empty |
| 1046 | `KEYBOARD_GET_STATUS` | Empty | Fixed 28-byte status record |
| 1047 | `KEYBOARD_CLEAR` | Empty | Empty |
| 1048 | `KEYBOARD_SET_TEMP_BLE_NAME` | `version:u8, name_length:u8, utf8_name` | `version:u8, effective_length:u8, effective_utf8_name` |
| 1049 | `KEYBOARD_ARM_BLE` | `version:u8, commit_id:u32` | `run_id:u32` |

Output masks for command 1044:

| Value | Output |
| ---: | --- |
| `0x01` | USB HID |
| `0x02` | BLE HID |
| `0x03` | USB and BLE HID |

The 28-byte status response is:

```text
version:u8
state:u8
error:u8
outputs:u8
upload_id:u32
commit_id:u32
run_id:u32
expected:u16
received:u16
program_counter:u16
length:u16
crc32:u32
```

Status states:

| Value | State |
| ---: | --- |
| 0 | Empty |
| 1 | Uploading |
| 2 | Ready |
| 3 | Running |
| 4 | Complete |
| 5 | Cancelled |
| 6 | Error |
| 7 | Armed for BLE HID |

Status errors:

| Value | Error |
| ---: | --- |
| 0 | None |
| 1 | Cancelled |
| 2 | Command link lost |
| 3 | HID output unavailable |
| 4 | CRC mismatch |
| 5 | Missing bytecode end marker |
| 6 | Data after bytecode end marker |
| 7 | Truncated opcode |
| 8 | Unknown opcode |
| 9 | Delay outside the per-command range |
| 10 | Total explicit delay exceeded |
| 11 | HID usage outside the allowed range |
| 12 | Tap count exceeded |
| 13 | HID report submission failed |

## Limits

| Limit | Value |
| --- | ---: |
| Compiled program | 4096 bytes |
| Script key taps | 1024 |
| Delay per `DELAY` statement | 1-10000 ms |
| Total explicit delay | 60000 ms |
| Wire upload chunk | 4089 bytes |
| CLI upload chunk | 4089 bytes |
| GUI upload chunk | 128 bytes |
| Temporary advertised name | 26 UTF-8 bytes |
| Saved GUI scripts | 255 |

Payload buffers and armed state are RAM-only and disappear on reset. Completion,
cancellation, errors, and link loss always attempt to release all pressed keys.
