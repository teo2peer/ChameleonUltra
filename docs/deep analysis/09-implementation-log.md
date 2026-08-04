# Registro de implementación

## 2026-07-16: snapshot atómico del slot MIFARE Classic activo

### Implementación

- El comando común 1050 publica una transacción versión 2 solo después de
  desarmar sensing HF/LF; devuelve `{slot, type, owner_generation, revision}` y
  queda ligada al transporte. Generation cambia al reload, no por random UID/RF.
- El gate central permite al owner únicamente control 1050 y lecturas exactas
  4008/4009/4016/4018. Bloquea segundo transporte, botones, sleep, RF rearm y
  mutaciones de slot/tipo/buffer durante el freeze.
- Lease idle 5 s se refresca solo por lecturas válidas y lease absoluto 120 s no
  se extiende. SAVE exige reserva restante de 46 s para el máximo FDS de 45 s y
  no declara expiry durante el commit bloqueante. Abort/expiry restauran sensing.
- `SAVE_RELEASE` revalida revision/owner/slot/type/Normal mode y fuerza solo el
  dump HF activo, saltando el shortcut CRC. FDS failure conserva transacción para
  retry; CRC baseline cambia únicamente después de success.
- Python y Flutter esperan SAVE 55 s, fallan cerrado ante v1/metadatos inválidos,
  abortan revision v2 recuperable y desconectan si outcome/estado queda incierto.

### Evidencia y residual

- Host C ASan/UBSan cubre owner/revision/leases/allowlist, freeze de mutaciones,
  force-write, FDS retry y restore de sensing.
- Tests fake-serial Flutter cubren random UID con owner estable, ABORT/poison de
  BEGIN malformado, timeout SAVE, cancelación por read y retry durable de historia.
- Quedan HIL Ultra/Lite: NFCT/LPCOMP, USB/BLE simultáneo, disconnect/expiry,
  fault injection FDS y power loss durante write.

## 2026-07-15: integridad de memoria, cancelación y settings v6

### Alcance

- F-005: carrera de cancelación física de keyboard payload.
- F-021: bytes no inicializados y offsets incorrectos en NTAG UID/counter mirror.
- F-022: lectura desalineada de Key B en trailers MIFARE Classic.
- F-052: longitud divergente de `GET_DEVICE_SETTINGS` v6.
- F-053: timeout de sleep persistido pero ignorado por runtime.

### Implementación

- `keyboard_payload_cancel_from_button()` publica cancelación; el main loop la
  conserva y la consume antes del primer HID report. El estado puede avanzar
  transitoriamente a RUNNING si la request llega tras el último recheck, pero la
  cancelación no se pierde ni permite salida HID.
- `ntag_mirror_internal.h` genera exactamente UID hex, separador `x` y counter
  hex, y superpone como máximo cuatro bytes dentro de cada página.
- `mf1_key_access_internal.h` detecta/copia seis bytes de key sin convertir una
  dirección potencialmente desalineada a `uint64_t *`.
- `device_settings_payload_internal.h` define el payload v6 canónico de 14 bytes.
  Firmware, CLI Python, GUI Flutter y `docs/protocol-command-reference.md`
  comparten ese contrato; la GUI conserva lectura legacy de 13 bytes.
- El timeout de sleep runtime proviene de `settings_get_sleep_timeout()`.

### Regression tests

- `firmware/tests/test_keyboard_payload.c`: interleaving cancel ARMED -> RUNNING,
  estado CANCELLED y cero HID reports.
- `firmware/tests/test_hf_memory_safety.c`: render golden, stack poison, offsets
  mirror 0..3, canaries y fuentes Key B desalineadas 0..7.
- `firmware/tests/test_device_settings_payload.c`: layout byte a byte y longitud
  exacta de 14 bytes.
- `software/script/tests/test_device_settings.py`: parse exacto y rechazo de
  payloads v6 cortos o largos.

### Validación completa

- 9/9 binaries C host pasaron con Clang ASan/UBSan.
- La suite Python hardware-free completa pasó bajo el entorno `uv` del proyecto.
- Application y bootloader Ultra/Lite compilaron y linkearon con el container
  canónico ARM GCC 12.2.rel1.
- Application text+data: 355,080 B Ultra y 287,224 B Lite.
- Bootloader text+data: 43,436 B Ultra y 43,372 B Lite; headroom 1,620/1,684 B.
- El artifact/layout validator pasó para ambos maps.

RF, HID físico y system-off siguen requiriendo HIL. El build container amd64
produjo fallos aleatorios de `cc1` bajo emulación Apple; reintentos seriales
completaron todos los objetos y los links finales, sin diagnóstico de source.

## 2026-07-15: owner de tag y persistencia truthful

### Implementación

- Los comandos HF de emulación validan que el tipo configurado sea el owner
  exacto cargado del buffer compartido antes de tocar pointers de protocolo.
- MF1, MF0/NTAG, HF14A genérico y HF14A-4 tienen hooks separados; Reader Keys
  físico aplica la misma validación y 4044 permanece reader-only.
- Save de datos actualiza CRC solo tras FDS success. Si una transición
  `SHADOW_REQ` prepara el payload y FDS falla, el callback restaura
  `SHADOW_REQ`; el retry vuelve a preparar y solo entra en `SHADOW` al persistir.
- Explicit save, slot switch y delete propagan fallo de flash. Switch conserva
  slot/owner/buffer y delete conserva config/owner cuando FDS falla.
- No cambió ningún record on-flash. Shutdown y atomicidad multi-record permanecen
  pendientes de fault injection y HIL.
- `SET_SLOT_TAG_TYPE` todavía puede reemplazar el owner compartido al cargar un
  tipo de un slot inactivo sin guardar primero datos dirty. Factory default sigue
  traduciendo un callback FDS fallido a `STATUS_NOT_IMPLEMENTED`, no a
  `STATUS_FLASH_WRITE_FAIL`.

### Validación

- 10/10 binaries C host pasaron con Clang ASan/UBSan.
- `test_tag_persistence.c` enlaza `tag_emulation.c` real y cubre failure/retry,
  rollback de callback, failed switch, failed delete y owner exacto.
- Esta evidencia termina en la frontera `tag_emulation.c`; no ejecuta directamente
  los handlers de `app_cmd.c` que traducen el resultado a status wire.
- Hardware-free Python completo pasó, incluida la matriz estructural de hooks.
- Application GCC 12.2 text+data: 356,072 B Ultra y 288,120 B Lite, incremento
  de 992/896 B frente al primer lote; gap RAM 3,576/60,824 B.
- Application y bootloader Ultra/Lite linkearon; artifact/layout checks pasaron.

## 2026-07-15: rescan profundo de persistencia, RF y release

### Implementación

- Factory default exige que el tipo solicitado coincida con el tipo configurado
  del mismo sense/slot; mismatch devuelve `STATUS_INVALID_SLOT_TYPE` y fallo FDS
  devuelve `STATUS_FLASH_WRITE_FAIL`.
- Deshabilitar un slot inactivo ya no puede cambiar el active slot; el slot activo
  actualiza inmediatamente solo el sense afectado y reader mode no rearma tag
  sensing. Los getters LF y Clone UID exigen owner `{slot,type}` cargado.
- Un scan EM410x fallido ya no cambia Electra/EM410x. Clone LF hace pre-save,
  guarda tipo/buffer/CRC, restaura ante reload fallido y no muestra éxito.
- `crc_valid` separa una baseline cargada/persistida de un CRC stale: missing
  record o type reload invalida; solo load/write exitoso valida.
- Config legacy se valida otra vez después de migrar; una conversión inválida
  restaura defaults y persiste o deja dirty la recuperación para retry.
- Delete nickname usa el último error FDS, por lo que ausencia de record es éxito
  idempotente. Los buffers nickname clone y el objeto factory MF1 se inicializan a
  cero antes de persistirse.
- NFC-A parity unwrap termina grupos exactos y decodifica trailing data bits sin
  parity. EMV profile reacquire aborta si falla la activación o cambia el UID.
- Shutdown normal se difiere y rearma el sleep timer si FDS falla; low-battery
  conserva shutdown best-effort para proteger hardware.
- USB STOPPED/POWER_REMOVED invalida `g_usb_port_opened`.
- Python vuelve a representar Reader Keys como `READERKEYS=6`.
- El workflow privilegiado usa scripts del default branch, valida metadata/path
  del run y exige hex exacto antes de evaluar map regions en Bash.
- `pyrefly` excluye tanto `.venv` como `venv`, evitando analizar site-packages
  embebidos; los errores propios restantes se conservan como deuda visible.

### Regression tests

- `test_tag_persistence.c` rechaza factory data para un tipo distinto al
  configurado y prueba que una baseline CRC inválida fuerza write.
- `test_hf_memory_safety.c` cubre parity unwrap de 7/9/10/17/72/73 bits,
  buffers exactos e in-place bajo ASan/UBSan, y continuidad/cambio de UID EMV.
- `test_device_settings.py` fija el valor wire `READERKEYS=6`.
- `test_artifact_checker.sh` acepta maps válidos y rechaza un origin con forma de
  command substitution, comprobando que no se crea el marker lateral.

### Validación completa

- 10/10 binaries C host pasaron con Clang ASan/UBSan.
- La suite canónica hardware-free pasó; `test_hard_acquire.py` y `test_ultra.py`
  permanecen excluidos porque requieren dispositivo/tag físico.
- `ruff check .`: 70 errores; `pyrefly check`: 420. Ambos gates siguen rojos y no
  se ocultaron mediante una edición masiva del CLI ajena a este lote.
- Application GCC 12.2 text+data: 357,712 B Ultra y 289,520 B Lite; gap regular
  heap-limit -> stack-limit: 3,568/60,816 B.
- Bootloader GCC 12.2 text+data: 43,436 B Ultra y 43,372 B Lite; headroom
  1,620/1,684 B.
- Application/bootloader Ultra y Lite linkearon y sus artifact/layout checks
  pasaron. El container amd64 siguió mostrando segfaults aleatorios de
  assembler/`cc1` bajo QEMU; retries `-j1` completaron sin diagnóstico de source.
- `check_no_new_secrets.sh` y `git diff --check` pasaron.
- `actionlint` no está instalado localmente; el workflow recibió revisión manual,
  pero su ejecución real sigue siendo un gate CI pendiente.
