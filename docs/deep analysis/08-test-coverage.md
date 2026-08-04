# Cobertura y testabilidad

## Estado actual

Hay diez binaries C host definidos en `firmware/tests/Makefile`. En la auditoría
se recompilaron con Clang `-O1`, ASan y UBSan y todos pasaron. El mismo target
ejecuta además una regression shell del parser de artifacts/map.

| Binary | Production code directo | Cobertura principal |
|---|---|---|
| `test_crc_utils` | `rfid/crc_utils.c` | CRC14A básico |
| `test_circular_buffer` | LF circular buffer | FIFO/full/wrap/static/double free |
| `test_dataframe` | `utils/dataframe.c` | encode, split streams, resync, queue |
| `test_device_settings_payload` | serializer settings v6 | layout exacto de 14 bytes |
| `test_keyboard_payload` | keyboard interpreter | upload/CRC/run/cancel/transports |
| `test_hf_memory_safety` | helpers NTAG/MF1/NFC-A/EMV | mirror, canaries, unaligned keys, parity groups y continuidad UID |
| `test_iso_dep_reader` | ISO-DEP reader | chaining, WTX, R-NAK, CRC |
| `test_iso_dep_session` | session wrapper | start/exchange/stop/replacement |
| `test_lf_helpers` | Manchester/diphase/Wiegand/Jablotron/parity | selected vectors |
| `test_tag_persistence` | `tag_emulation.c` | owner, failed save retry, switch/delete |

Solo 13 translation units productivas se enlazan directamente frente a unas 66:
aproximadamente 20% de file reach. No hay instrumentación line/branch/function,
por lo que no es coverage real.

## Bien cubierto parcialmente

- Dataframe framing/backpressure básico.
- Keyboard upload atomicity e interpreter happy/error paths principales.
- ISO-DEP reader/session state nominal y varios errors.
- Circular buffer.
- Cinco Wiegand formats y Jablotron modulation.
- Host mirrors de command IDs/status y parsers BLE/EMV/keyboard.

## Gaps P0

### Command handlers

`app_cmd.c`, `app_cmd_ble.c` y `app_cmd_keyboard.c` no se ejecutan en tests C.
Faltan lengths, NULL, max payload, hooks, transport ACL, Ultra/Lite dispatch y
wire serialization de todos los commands.

### FDS/settings/tag persistence

`utils/fds_util.c` tiene timeout, event matching, busy retention, GC retry,
migration y Peer Manager filtering sin fake backend ni fault injection. La capa
`tag_emulation.c` sí cubre ahora propagación/retry ante un backend que falla.

### NFCT/emulator

El helper parity unwrap se ejecuta con grupos exactos/parciales e in-place, pero no
su integración NFCT ni wrap. Tampoco se ejecutan anticollision, ATS, ISO-DEP tag,
MF1 access/log, MF0 locks/counters/password o live publication.

### Concurrencia

Se modela determinísticamente cancelación keyboard durante la transición de
readiness ARMED y se comprueba cero HID. Falta la ventana estrecha
post-recheck/pre-asignación y no se modelan otros interleavings de
SoftDevice/app_timer/NFCT con main: decoder reset, scan startup, advertising
rotation, USB removal, sleep y logs.

## Gaps P1

- `ble_scan.c`, `ble_central.c`, `ble_main.c`, `usb_main.c`, `keyboard_hid.c`.
- EMV trace C: continuidad UID tiene helper cubierto; faltan TLV/DOL/AFL/meta/page/CRC/truncation e integración RF.
- Crypto1 known-answer y differential entre implementaciones.
- Todos los LF codecs, noisy/jittered/inverted traces y all Wiegand formats.
- RC522 scripted-register harness.
- Settings migrations y explicit persisted serializers.
- Bootloader HIL y script fixture tests.

## Matriz build actual

| Validación | Ultra | Lite |
|---|---:|---:|
| Application cross-build CI | Sí | Sí |
| Bootloader cross-build CI | Sí | Sí |
| Full signed package main/tags | Sí | Sí |
| Native C tests con board macro | No | No |
| `SDK_VALIDATION=1` | No | No |
| HIL USB/BLE/FDS | No | No |
| HIL HF/LF reader | Manual | N/A |
| HIL emulator | No automatizado | No automatizado |
| DFU interruption/wrong-board | No automatizado | No automatizado |
| Power/performance regression | No | No |

## Suite incremental propuesta

### Nivel 1: cada PR

- Host C GCC+Clang, C99/C11, O0/O1/O2/O3, ASan/UBSan/LSan Linux.
- Command contract, FDS fake backend, pure RF/TLV/LF tests.
- Ultra/Lite clean compile y map budgets.
- Short libFuzzer corpus.
- Python hardware-free.
- Signature generator y docs links.

### Nivel 2: scheduled

- Long fuzz corpora.
- Static analyzers y CodeQL.
- Two-build reproducibility.
- FDS endurance/power-cut simulator where possible.
- Long BLE event/state randomized model.

### Nivel 3: release HIL

- Ultra y Lite power-controlled rack.
- USB relay y BLE peer con security profiles.
- HF/LF fixtures y tag matrix.
- PPK2 capture.
- Full/ app-only DFU valid/invalid/interrupted.
- Reboot/system-off/WDT/field wake retained-data matrix.

## Coverage gates sugeridos

- Publicar line, branch y function totals primero, sin imponer porcentaje ciego.
- 90% branch para framing, wire codecs, command validation, FDS wrapper y parsers.
- 80% changed-line para código pure/host-testable.
- Todo bug P0 añade regression test que falla antes del fix.
- Hardware-only code exige compile + state model + HIL scenario documentado.
- Ninguna caída de Ultra linker gap/NOINIT headroom sin aprobación explícita.

## Build hygiene

- Output dirs separados por board/toolchain/config.
- Generated header dependencies en host Makefile.
- Clean y build como invocaciones separadas.
- GCC 12.2 canónico para release; GCC 8.5 local solo diagnóstico.
- Guardar maps y artifact hashes como CI artifacts.
- Excluir Nordic SDK del coverage project-owned, pero compilarlo normalmente.
