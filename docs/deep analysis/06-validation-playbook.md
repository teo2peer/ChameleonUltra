# Playbook de validación del firmware

## Principios

- Separar host determinista, cross-build, HIL digital, RF/analog, power y release.
- Ejecutar Ultra y Lite en outputs limpios distintos.
- Guardar firmware SHA, toolchain, board revision y fixture con cada resultado.
- Medir profile/release real; logging/instrumentation builds se reportan aparte.
- Ningún test host prueba interoperabilidad RF, SoftDevice timing o flash power-cut.

## Verificación host mínima

```bash
make -C firmware/tests clean BUILD_DIR=build-deep-analysis
make -C firmware/tests test CC=clang SANITIZE=1 BUILD_DIR=build-deep-analysis
uv run --project software python .github/scripts/run_hardware_free_python_tests.py
HW_VERSION=0 .github/scripts/validate_firmware_release.sh --device ultra
HW_VERSION=1 .github/scripts/validate_firmware_release.sh --device lite
python3 firmware/tools/generate_deep_analysis_signatures.py
git diff --check
```

La auditoría verificó que `make clean test` reconstruye y ejecuta los diez
binaries. CI mantiene clean y test separados para logs y outputs aislados, no
porque se haya reproducido ejecución stale.

## Matriz funcional y de fallos

| Área | Escenario | Medir/observar | Gate |
|---|---|---|---|
| Dataframe | split/coalesce/LRC/length/reset/reconnect | accepted/dropped/order | Sin cross-session ni overflow |
| Command dispatch | cada row, short/exact/long/max | status/len/hooks/ACL | Wire golden exacto Ultra/Lite |
| USB | 0/64/244/4096 B, unplug/DTR | latency/queues/state | Open implica VBUS+port |
| BLE NUS | same payloads, disconnect/burst | credits/latency/reset | Sin command stale |
| BLE auth | open/encrypted/MITM+LESC | command allow/deny | ACL exacta |
| BLE scan | report inmediato al start | active/rearm/count | No logical-active stopped |
| Adv lab | rotate/terminate/connect | generation/on-air payload | Normal o lab coherente |
| BLE central | discover/read/write/sub | ATT status/retries | Error preservado |
| Keyboard | cancel en cada transition | final state/reports | Cancel nunca termina RUNNING |
| FDS write | busy/full/error/late/lost | dirty/busy/retry | Success solo durable |
| FDS GC | near-full y power cut | duration/live/dirty words | Records no relacionados intactos |
| FDS migration | v0-v8, zero/new CRC | exact bytes/defaults | Forward compatible; tipos migrados válidos |
| Slot/type | cada command x cada type | status/RAM/FDS | Incompatible no muta |
| Live HF mutation | reader continuo + writes | torn RF/reset | Old/new completo |
| NFCT errors | parity/frame/overflow/timeout | state/rearm | Error no entra al protocol |
| Sniffer | 4/7/8/9/12/16/72-bit RX/TX | bit length/parity policy | Trace exacto, sin OOB en grupos de 9 bits |
| ATS | FSDI 0..8 | TL/transmitted bytes | TL == ATS length |
| ISO-DEP tag | short/busy/full/WTX | command status/deadline | Accepted truthfully, bounded |
| ISO-DEP reader | chain/R-NAK/WTX/CID | blocks/timeouts | Existing goldens + boundaries |
| MF1 auth log | clear en cada auth step | epoch/records | Sin hybrid transcript |
| NTAG mirror | all legal pages/modes | emitted bytes | No stack leak |
| LF read | each protocol/noisy/inverted | decode/time/heap | No alloc failure/overlap |
| LF emulate | golden waveform | periods/gaps/polarity | Reader interoperability |
| T55xx | password/max blocks | timeslot margin | Completion con margen definido |
| Sleep | expiry + VBUS/BLE/HF/LF | system-off decision | Recheck blockers |
| Low battery | 0% en cada state | WDT/disconnect/off | Forced bounded shutdown |
| DFU | signed/wrong/old/same/cross-board | accept/reject/recovery | Policy exacta |
| DFU power cut | cada erase/write/activate boundary | boot/recovery/FDS | Recoverable |
| SWD production | debugger attach | access/recovery | APPROTECT policy |

## Datasets y fixtures RF

### HF reader

- MIFARE Mini/1K/2K/4K, 4-byte y 7-byte UID.
- Weak/static/hard nonce cards y wrong-key paths.
- NTAG210/212/213/215/216, lock/password/counter/signature cases.
- DESFire and ISO-DEP cards con FSDI/ATS/chaining/WTX variados.
- Malformed emulator/fixture frames para parity, CRC, timeout y partial bits.

### HF emulation

- Cada supported HF tag type y slot vacío/LF-only.
- Reader que cambia FSDI, retransmite, usa WTX y corta field.
- Simultaneous host mutations y saves.

### LF

- Todos los Wiegand formats en min/max/max+1.
- EM410x, HID, ioProx normal/inverted, Viking, PAC, Jablotron, IDTECK.
- Traces clean, jittered, noisy, truncated y amplitude/coupling variants.
- Voltage, temperature y varias distancias/orientaciones.

## Memoria

### Build

- Parsear `.text`, `.rodata`, `.data`, `.bss`, heap, stack y NOINIT por separado.
- Top 30 symbols RAM/flash.
- Fallar si heap/stack overlap, NOINIT overflow o gap disminuye sin approval.
- Fallar si bootloader no cabe o queda por debajo del headroom mínimo acordado;
  GCC 12.2 actual deja 1,620 B Ultra/1,684 B Lite.
- Comparar Ultra y Lite, GCC 12.2 release y optional validation build.

### Runtime

- Stack fill/canary y high-water después de cada scenario.
- Heap wrapper: current/peak, largest allocation, failures y fragmentation proxy.
- ISR nesting peak donde sea medible.
- FDS static write buffer y RF/LF scratch ownership.

Stress Ultra crítico: BLE peripheral+central MTU247, scanning/fuzz timer, NUS 4K,
USB CDC/HID, FDS write/GC y RF operation. Repetir Lite sin reader front-end.

## Rendimiento

Reportar median/p95/p99/max y failure rate:

- Reset -> USB ready y BLE advertising ready.
- Command round-trip 0/64/244/4096 B USB/BLE.
- Dataframe decode/backpressure recovery.
- BLE scan records/s y GATT discovery/read/write/subscribe.
- RC522 scan/auth/read/write/raw maximum timeout.
- ISO-DEP APDU no-WTX, repeated WTX y chained response.
- EMV trace capture y page drain.
- LF acquisition/decode por protocolo.
- FDS create/update/delete/GC.
- DFU transfer/validation/activation.

Instrumentar con DWT CYCCNT/GPIO trace/SEGGER RTT en diagnostic build. No dejar
logging de timing en release si cambia IRQ/timing RF.

## Potencia

PPK2 o equivalente, supply y sample rate documentados:

- System OFF.
- Emulation idle y HF/LF field active.
- USB idle/transfer/HID.
- BLE advertise/connected/scan/central/GATT/stress.
- HF/LF reader field/acquisition.
- LED animation/battery display.
- FDS write/GC y DFU.

Comparar timer BSP actual con deadline-driven replacement. La mejora se acepta si
no aumenta missed events ni protocol latency.

## Fuzz/static analysis

Targets prioritarios:

1. `data_frame_receive_from` con fragmented streams.
2. Command processors con arbitrary length/data y backends stub.
3. Keyboard upload/interpreter.
4. ISO-DEP block parser.
5. EMV BER-TLV/DOL/AFL.
6. NFC frame wrap/unwrap.
7. LF interval decoders y Wiegand unpack.
8. BLE advertisement merge/serializers.

PR smoke corpus bajo ASan/UBSan; scheduled long fuzz. Añadir CodeQL C/C++,
cppcheck/clang-tidy o GCC `-fanalyzer`, `-fstack-usage` y warnings escalonados.

## Release qualification

1. Build limpio GCC 12.2 Ultra/Lite y bootloader.
2. Partition, map/headroom, command mirror y changelog gates.
3. Dos builds aislados byte-identical o diferencias explicadas/provenance.
4. Valid/invalid DFU matrix y interrupted updates.
5. Trust-root migration test old/new en hardware.
6. Production SWD/authenticated boot policy.
7. Full USB/BLE/RF/FDS HIL smoke por board.
8. Power/current regression comparison.

## Formato de resultado

```text
Commit/tree hash:
Dirty state:
Board/revision/serial:
Toolchain/build flags:
Firmware artifact SHA256:
Fixture/peer/tag:
Scenario/dataset:
Metric before:
Metric after:
Functional result:
Memory/map delta:
Power delta:
Tradeoffs/residual risk:
```
