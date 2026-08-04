# Mapa de reutilización del firmware

La etiqueta **alto/medio/bajo** expresa valor de consolidación, no prioridad P0/P1
de impacto. Primero se corrigen bugs y
se escriben characterization tests. La extracción debe reducir al menos dos call
sites sin añadir flags genéricos que oculten semántica RF/ISR.

## Oportunidades

### R-01 Bounded wire reader/writer - valor alto

**Repetición:** `firmware/application/src/app_cmd.h`,
`firmware/application/src/app_cmd_ble.c`,
`firmware/application/src/app_cmd_keyboard.c` y EMV trace.

**Extraer:** `wire_reader_t`/`wire_writer_t` con remaining, explicit `be16/be32` y
`le16/le32`, put bytes y finish exact. Sin alloc ni unaligned casts.

**Riesgo:** varios legacy payloads usan endianness histórica distinta. Migrar
command por command con golden wire tests, no reemplazo masivo.

### R-02 Persistence commit primitive - valor alto

**Repetición:** settings, slot config, tag data, nickname.

**Extraer:** helper pequeño `changed -> write -> commit baseline on success` y
typed FDS result. Mantener serializers/migrations separados.

### R-03 BER-TLV/DOL iterator - valor alto

**Repetición:** cuatro walkers en `firmware/application/src/app_cmd.c` y
`firmware/application/src/rfid/reader/hf/emv_trace.c`.

**Extraer:** iterator bounded y query helper con depth/length limits; DOL parser
separado de terminal-value policy.

**No unificar:** workflow legacy command 6005 con retained trace 6007-6009.

### R-04 RC522 transfer core - valor medio

**Repetición:** `pcd_14a_reader_bytes_transfer*` en
`firmware/application/src/rfid/reader/hf/rc522.c:355-498,624-758`.

**Extraer:** core interno con options tipadas y wrappers delgados. Characterize
WDT feed, trace callbacks, Crypto1 retention, timeout y error mapping antes.

### R-05 LF acquisition runner - valor medio

**Repetición:** alloc codec, queue wait, timeout, copy result y cleanup en seis
`firmware/application/src/rfid/reader/lf/lf_*_data.c`.

**Extraer:** lifecycle runner y cleanup ownership. Mantener adapters distintos
para GPIOTE edge y SAADC samples, y preservar PAC power order/ioProx generation.

### R-06 Canonical tag/protocol descriptors - valor medio

**Repetición:** tag macro lists, registry rows, size/load/save/factory switches y
LF protocol capabilities.

**Extraer:** lookup único por tag type y descriptor LF que exprese decode,
emulate y T55xx support por separado. Enum/FDS bytes no cambian.

### R-07 Real MIFARE auth trace - valor medio

**Repetición:** `authex` y `auth_trace_do_auth`; trace actual ya existe en RC522.

**Extraer:** auth primitive única y capture de frames reales. El trace sintetizado
actual usa REQA mientras scan real usa WUPA.

### R-08 ISO-DEP activation/serializer - valor medio

**Repetición:** polling/activation en
`firmware/application/src/rfid/reader/hf/iso_dep_session.c`,
`firmware/application/src/app_cmd.c` y
`firmware/application/src/rfid/reader/hf/emv_trace.c`; serializer
`uid|atqa|sak|ats` aparece varias veces.

**Extraer:** pure bounded serializer y activation helper con profile/timing
explícitos. No esconder field ownership ni Apple Transit annotations.

### R-09 IEEE CRC32 - valor bajo

Implementaciones equivalentes en keyboard y EMV trace. Extraer pure utility con
known-answer tests. No confundir con CRC16 ISO14443/FDS/dirty CRC.

### R-10 Command module decomposition - valor medio

Mover handlers HF batch/EMV, LF reader y emulation a módulos dueños. Mantener en
un único punto la dispatch table, command ACL, Ultra/Lite availability y hooks.

### R-11 Generation/state transition primitive - valor medio

BLE scan, advertising lab, dataframe reset, keyboard cancel y sleep comparten el
patrón async generation. Reutilizar reglas/documentación y helpers atómicos
pequeños, no una state machine universal.

### R-12 Deadline utility - valor medio

Un `deadline_expired(now,start,budget)` wrap-safe reduce loops ad hoc en BSP,
RF, FDS, keyboard y BLE. El budget y el servicio permitido durante espera siguen
siendo domain-specific.

### R-13 APDU pair writer/tag serializer - valor bajo

Reemplazar macros `SEND_APDU`/`APPEND_PAIR` por funciones bounded con explicit
buffer/capacity. Evita dependencia preprocessor accidental de DESFire.

## Código potencialmente muerto

Confirmar con call graph Ultra y Lite antes de borrar:

- `firmware/application/src/rfid/reader/lf/lf_gap.c/.h`.
- `em4x05_read` y `set_scan_tag_timeout`.
- `timeslot_start/stop/cancel`.
- `bsp_timer_uninit/stop`.
- `ble_central_get_char_count`, `ble_central_notif_count` y probe-log copy.
- `nfc_tag_14a_set_sniff_passive`, `nfc_tag_14a_is_reset_enable`.
- RC522 parity toggles, `cascade_to_cmd` y selectable hardware CRC path.

Varias helpers RC522/T55xx/MF1/NFC-A deberían hacerse `static` si siguen siendo
internas. `DATA_CMD_EM4X05_READSNIFF` no es dead accidental: es wire ID reservado.

## Límites: no unificar

- NFC-A anticollision, MF1, NTAG y ISO-DEP emulation state machines.
- GPIOTE LF edge readers y SAADC LF sample readers.
- USB CDC y BLE NUS TX queues: concurrencia, credits y fragmentation difieren.
- USB HID y BLE HID completion/security.
- Settings, slot config y tag-data serializers/migrations.
- Ultra/Lite board mappings.
- Legacy EMV 6005 y retained trace 6007-6009 a nivel workflow/wire.
- Weak/static/hard MF1 nonce modes.
- Command capability (recognized ID) y feature availability real.

## Gate para cada extracción

1. Characterization test antes del movimiento.
2. Mismo wire transcript/status/endianness.
3. Mismo ISR/main ownership y critical-section duration.
4. Mismo persisted byte layout o migración explícita.
5. Map diff de flash/RAM/stack.
6. Ultra y Lite compilan por separado.
7. HIL cuando toca RF/BLE/USB/DFU.
