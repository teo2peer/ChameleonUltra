# Roadmap de corrección y optimización

Este documento es la fuente canónica del orden. Cada PR debe ser pequeña,
reversible y acompañada por tests y map diff.

## Fase 0: baseline y decisiones de seguridad

1. Congelar toolchain release GCC 12.2, SDK y source commit.
2. Build limpio Ultra/Lite application+bootloader y guardar map/size JSON.
3. Mantener budget bootloader: GCC 12.2 confirmado deja 1,620 B Ultra/1,684 B
   Lite; optimizar/reparticionar antes de consumirlo sin debilitar security.
4. Capturar stack/heap watermark inicial en hardware.
5. Inventariar devices y diseñar migración del trust root DFU.
6. Definir command ACL por USB, BLE open, encrypted, MITM+LESC y physical confirm.

**Gate:** artefactos reproducibles/identificados, layout checks verdes y recovery
plan documentado. No rotar la key sin ruta de actualización fleet-wide.

## Fase 1: safety net host

### 1A Command contract harness

- Compilar `app_cmd*` Ultra y Lite con stubs.
- Cada dispatch row: NULL/0/short/exact/long/max.
- Validar status, length, endianness, hooks, ACL y response transport.

### 1B FDS fault injection

- Fake backend para busy/no-space/GC/timeout/error/event late/lost.
- Settings/slot migrations v0-v8, slot/tag save/delete/switch/shutdown.

### 1C Pure protocol tests

- NFCT frame wrap/unwrap/status y partial bits.
- ATS por FSDI, ISO-DEP response states/WTX.
- NTAG mirror, MF1 access/log epoch, ioProx polarity, Wiegand ranges.
- BER-TLV/DOL/AFL y wire serializers.

### 1D Concurrency models

- Deterministic interleavings para dataframe reset, keyboard cancel, scan start,
  advertising rotation, USB removal, button events y sleep expiry.

**Gate:** ASan/UBSan verde, coverage publicado y tests fallan con el bug actual
para cada P0 que se vaya a corregir.

## Fase 2: correctness e integridad temprana

1. **Corregido para commands/button:** active-type guards; type change conserva
   un residual de owner/persistencia en F-017.
2. **Parcial:** persistence result propagation y commit baseline solo en success;
   factory default, migration validation y normal-sleep retry corregidos; faltan
   type change, emergency-shutdown HIL y atomicidad multi-record.
3. **Corregido:** NTAG mirror init/offsets y MF1 unaligned access.
4. **Parcial:** factory MF1 ya hace zero-init; eliminar objetos multi-KiB del stack
   con strategy medida que no añada otro buffer a RAM Ultra.
5. **Parcial:** parity unwrap exacto corregido; faltan NFCT RX error, sniff bit
   lengths, ATS TL, ISO-DEP accepted y darkside status.
6. LF streaming/ring/arena replacement; no añadir un static 12 KiB al layout.
7. **Corregido para seguridad HID:** keyboard cancel request se conserva y se
   consume antes del primer report; no se afirma transición de estado atómica.
8. **Corregido:** settings v6 wire response sincronizada con Python/GUI y sleep
   timeout runtime.
9. Persisted UID/ATS/enums/static-response semantic validators.
10. ioProx raw polarity y Wiegand/HID range/format rejection.
11. Hard nonce RNG pool/DRBG separado de weak/static modes.
12. MF1 log generation y live data staging/quiesce.
13. Dataframe complete/reset atomic ownership.
14. Sleep/low-battery generation y HF/LF separate field flags, con policies
   distintas para normal sleep y emergency bounded shutdown.

**Gate:** fault tests, interleavings y protocol goldens verdes; map RAM no empeora
sin justificación; zero sanitizer findings.

## Fase 3: security migration

### 3A Command plane

- ACL central; sensitive BLE commands requieren MITM+LESC. USB sensible requiere
  host tratado explícitamente como confiable, device unlocked y confirmación local
  según la clase; conectar un cable no autentica por sí solo.
- PIN único provisionado, rotation efectiva inmediata, bonds/repairing policy.
- DFU entry autenticado y target-bound.

### 3B Boot chain

- Migration bootloader firmado por old key que instala new offline trust root.
- Revocar old key en nuevos bootloaders/CI; no basta borrar Git file.
- ECDSA boot validation obligatoria en production profile.
- APPROTECT production con destructive recovery definido.

### 3C Deployment helpers

- Migration state por immutable serial/chip ID y bootloader layout query.
- Clean build dirs keyed por board/toolchain/config.
- Serial selection y deadlines en DFU scripts.

**Gate:** old key rechazada tras migración, new key aceptada, downgrade/same/wrong
board/interrupt tests verdes en Ultra y Lite, y bootloader cabe con budget acordado.
Development image claramente unlocked.

## Fase 4: FDS format e integridad

1. Explicit serializers para settings/slot config; no raw compiler bitfields.
2. Activar FDS read/write CRC con zero-CRC migration probada.
3. Dirty generations explícitas; CRC solo como optimization.
4. Idempotent writes y FDS metrics/GC budget.

**Gate:** golden flash images antiguas/nuevas, bit-flip/power-cut tests y restore
sin pérdida de records no relacionados.

## Fase 5: state machines y energía

1. Deadline utility y replacement del BSP 100 Hz timer.
2. Bounds/cancel para RF waits, WTX, FDS y RNG startup.
3. Unified reader-field source y live sense updates.
4. BLE scan/advertising/radio generation-safe transitions.
5. **Parcial:** USB open/connected se invalida en stop/removal; falta bond-delete
   typed result y event-injection completo.
6. Per-button event queue.

**Gate:** event-injection host tests y HIL stress; PPK2 idle/connected current no
regresa; no active session se apaga por sleep.

## Fase 6: memoria y rendimiento medidos

1. Stack/heap high-water por escenario Ultra/Lite.
2. Top RAM/flash symbols y per-PR budgets.
3. Command/transport p50/p95/p99 y queue depth.
4. FDS write/GC latency; RF APDU/scan/decode timing.
5. Reducir buffers/stack o ajustar heap solo con evidencia.
6. Evaluar dispatch lookup, queue depth, LTO/logging/backend crypto después.

**Gate:** memoria con margen explícito y no solo linker success; performance mejora
sin cambiar RF/wire/persistence behavior.

## Fase 7: reutilización estructural

Orden sugerido:

1. Bounded wire codecs y CRC32.
2. BER-TLV/DOL y APDU writer.
3. Persistence commit helper.
4. RC522 transfer core.
5. ISO-DEP activation/tag serializer.
6. LF acquisition lifecycle.
7. Canonical tag/protocol descriptors.
8. Real MIFARE trace.
9. Partir command modules.
10. Dead/internal API cleanup.

**Gate:** reglas de `04-reuse-map.md`, build dual-board y HIL correspondiente.

## Fase 8: reproducibilidad y release qualification

- Pin actions/plugins/container/apt snapshot; provenance manifest firmado.
- `SOURCE_DATE_EPOCH` y normalized DFU ZIP metadata.
- Dos builds limpios iguales en CI.
- Static analysis, fuzz corpora y changed-line coverage gates.
- HIL rack Ultra/Lite con power cycle, USB relay, BLE peer y RF fixtures.
- DFU interruption/endurance y FDS power-loss campaigns.

## División sugerida de PRs

1. Command harness + wire goldens.
2. FDS fake backend + failed-save characterization.
3. Type guards + persistence truthfulness.
4. NTAG mirror + unaligned MF1 + RF framing/status fixes.
5. LF bounded streaming/arena replacement.
6. Keyboard cancel + dataframe/log generation races.
7. Factory stack/zero-init y persisted semantic validators.
8. Settings v6/sleep, ioProx/Wiegand y hard nonce RNG correctness.
9. Sleep/field/low-battery state.
10. BLE/USB state interleavings.
11. Command ACL/PIN/USB trust policy.
12. Trust-root migration release train.
13. FDS serializer/CRC migration.
14. Timer/deadline and polling bounds.
15. Memory/performance instrumentación.
16. Pure reuse extractions.
17. Reproducible release/HIL gates.

No mezclar trust-root rotation, persisted-format migration, RF state refactor y
visual/CLI protocol changes en una sola release: impide rollback y diagnóstico.
