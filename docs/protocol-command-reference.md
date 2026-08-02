# Referencia completa de comandos del protocolo

Esta es la referencia de **todos los IDs definidos por el firmware de esta rama**.
La fuente de verdad es `firmware/application/src/data_cmd.h`; la disponibilidad
real se obtiene con `GET_DEVICE_CAPABILITIES` (1035). Los comandos no anunciados
no deben enviarse aunque aparezcan en un cliente antiguo.

## Convenciones del cable

Cada operación usa una trama de petición y una trama de respuesta:

```text
SOF=11 | LRC1 | command:u16be | status:u16be | length:u16be | LRC2 |
payload[length] | LRC3
```

- El payload máximo es 4096 bytes.
- Los enteros multibyte son big-endian (`be`), salvo excepciones indicadas.
- `u8`, `u16`, `u32` y `u64` son enteros sin signo.
- `i8` es un entero con signo.
- `bytes[N]` son exactamente N bytes; `bytes[...]` usa el resto del payload.
- `vacío` significa longitud cero.
- `ignorado` significa que el handler actual no valida la longitud y descarta el
  payload; los clientes deben enviar vacío igualmente.
- Los estados APDU `SW1 SW2` pertenecen a la tarjeta y no son el `status` externo
  de Chameleon.
- Si el emparejamiento BLE está habilitado, una conexión NUS no autorizada puede
  recibir `STATUS_DEVICE_MODE_ERROR` antes de llegar al handler.

## Estados de respuesta

| Valor | Nombre | Significado |
|---:|---|---|
| `00` | `STATUS_HF_TAG_OK` | Operación HF correcta |
| `01` | `STATUS_HF_TAG_NO` | No se encontró tarjeta HF |
| `02` | `STATUS_HF_ERR_STAT` | Error de comunicación HF |
| `03` | `STATUS_HF_ERR_CRC` | CRC HF incorrecto |
| `04` | `STATUS_HF_COLLISION` | Colisión HF |
| `05` | `STATUS_HF_ERR_BCC` | BCC incorrecto |
| `06` | `STATUS_MF_ERR_AUTH` | Autenticación MIFARE fallida |
| `07` | `STATUS_HF_ERR_PARITY` | Paridad HF incorrecta |
| `08` | `STATUS_HF_ERR_ATS` | ATS requerido pero ausente/incorrecto |
| `40` | `STATUS_LF_TAG_OK` | Operación LF correcta |
| `41` | `STATUS_LF_TAG_NO_FOUND` | No se encontró tag LF |
| `42` | `STATUS_LF_TAG_LOGIN_REQUIRED` | El tag LF requiere LOGIN |
| `60` | `STATUS_PAR_ERR` | Payload o parámetro inválido |
| `66` | `STATUS_DEVICE_MODE_ERROR` | Modo, enlace o estado incompatible |
| `67` | `STATUS_INVALID_CMD` | ID no despachado |
| `68` | `STATUS_SUCCESS` | Operación de sistema correcta |
| `69` | `STATUS_NOT_IMPLEMENTED` | Función no implementada |
| `70` | `STATUS_FLASH_WRITE_FAIL` | Escritura FDS/flash fallida |
| `71` | `STATUS_FLASH_READ_FAIL` | Lectura FDS/flash fallida |
| `72` | `STATUS_INVALID_SLOT_TYPE` | Tipo de slot incompatible |
| `73` | `STATUS_MEM_ERR` | Error de memoria/buffer |
| `74` | `STATUS_CREATE_RESPONSE_ERR` | No se pudo crear la respuesta |
| `75` | `STATUS_CMD_ERR` | Fallo de ejecución o estado interno |

## Disponibilidad por modelo

- **Ambos**: disponible en Ultra y Lite.
- **Ultra**: solo aparece en capacidades de ChameleonUltra.
- Los comandos 2000, 3000 y 6000 son Ultra-only.
- Los bloques 1000, 4000, 5000 y 7000 se despachan en ambos modelos.
- Un ID no anunciado normalmente devuelve `STATUS_INVALID_CMD` si se fuerza.

## Índice de rangos y huecos

| Rango | Uso | IDs asignados/huecos principales |
|---|---|---|
| 1000-1999 | Dispositivo, slots, ajustes, teclado | 1000-1021, 1023-1053; 1022 y 1054-1999 libres |
| 2000-2999 | Lector HF | 2000-2018, 2020-2025, 2100-2101, 2200-2201 |
| 3000-3999 | Lector LF | 3000-3006, 3009-3016, 3018-3020, 3030-3032 |
| 4000-4999 | Emulación HF | 4000-4001, 4004-4044 |
| 5000-5999 | Emulación LF | 5000-5013 |
| 6000-6999 | ISO-DEP/EMV | 6000-6014; 6015-6999 libres |
| 7000-7999 | BLE | 7000-7006, 7010-7032, 7040-7047, 7050-7054 |

**6400 no es un comando Chameleon.** En la documentación de relay, `6400` es
un status word APDU. Si se envía como ID de comando, devuelve
`STATUS_INVALID_CMD`. Lo mismo aplica a cualquier hueco no anunciado por 1035.

## 1000-1053: dispositivo, slots y teclado

Todos los comandos de este bloque están disponibles en Ultra y Lite. Los índices
de slot enviados por cable son `0..7`; la interfaz suele mostrarlos como `1..8`.
Los tipos de sentido son `1=LF`, `2=HF`.

| ID | Comando | Petición | Respuesta correcta y efecto |
|---:|---|---|---|
| 1000 | `GET_APP_VERSION` | ignorado | `major:u8, minor:u8` |
| 1001 | `CHANGE_DEVICE_MODE` | `mode:u8` (`0=tag`, `1=reader`) | Vacía. Reader mode es Ultra-only; Lite responde `NOT_IMPLEMENTED` para `1` |
| 1002 | `GET_DEVICE_MODE` | ignorado | `reader:u8`; 1=reader, 0=tag/no inicializado |
| 1003 | `SET_ACTIVE_SLOT` | `slot:u8` (`0..7`) | Guarda el slot saliente, carga el entrante y lo activa; `FLASH_WRITE_FAIL` conserva slot/RAM saliente |
| 1004 | `SET_SLOT_TAG_TYPE` | `slot:u8, tag_type:u16be` | Cambia el tipo HF o LF en RAM; persistir con 1009 |
| 1005 | `SET_SLOT_DATA_DEFAULT` | `slot:u8, tag_type:u16be` | Crea datos de fábrica solo para el tipo configurado; mismatch devuelve `INVALID_SLOT_TYPE` y fallo FDS `FLASH_WRITE_FAIL` |
| 1006 | `SET_SLOT_ENABLE` | `slot:u8, sense:u8, enabled:u8` | Actualiza inmediatamente el sense si el slot está activo; slots inactivos no fuerzan switch y reader mode no rearma emulación; si el save falla, revierte |
| 1007 | `SET_SLOT_TAG_NICK` | `slot:u8, sense:u8, nick[0..32]` | Guarda el nombre inmediatamente en FDS |
| 1008 | `GET_SLOT_TAG_NICK` | `slot:u8, sense:u8` | Nick raw de 0..32 bytes |
| 1009 | `SLOT_DATA_CONFIG_SAVE` | vacío obligatorio | Guarda configuración y datos activos; devuelve `FLASH_WRITE_FAIL` y conserva dirty state para retry |
| 1010 | `ENTER_BOOTLOADER` | vacío obligatorio | Entra en DFU; normalmente no llega respuesta al host |
| 1011 | `GET_DEVICE_CHIP_ID` | ignorado | `DEVICEID1:u32be, DEVICEID0:u32be` |
| 1012 | `GET_DEVICE_ADDRESS` | ignorado | Dirección BLE estática como `high:u16be, low:u32be` |
| 1013 | `SAVE_SETTINGS` | vacío obligatorio | Persiste ajustes; puede devolver `FLASH_WRITE_FAIL` |
| 1014 | `RESET_SETTINGS` | vacío obligatorio | Restaura defaults y los persiste |
| 1015 | `SET_ANIMATION_MODE` | `mode:u8` (`0..3`) | 0 full, 1 minimal, 2 none, 3 symmetric; persistir con 1013 |
| 1016 | `GET_ANIMATION_MODE` | ignorado | `mode:u8` |
| 1017 | `GET_GIT_VERSION` | ignorado | Texto raw sin NUL |
| 1018 | `GET_ACTIVE_SLOT` | ignorado | `slot:u8` |
| 1019 | `GET_SLOT_INFO` | ignorado | 8 registros `hf_type:u16be, lf_type:u16be` |
| 1020 | `WIPE_FDS` | vacío obligatorio | Borra FDS y programa reset, incluso si el borrado falla |
| 1021 | `DELETE_SLOT_TAG_NICK` | `slot:u8, sense:u8` | Elimina el nombre de FDS; es idempotente si el record no existe |
| 1023 | `GET_ENABLED_SLOTS` | ignorado | 8 registros `hf_enabled:u8, lf_enabled:u8` |
| 1024 | `DELETE_SLOT_SENSE_TYPE` | `slot:u8, sense:u8` | Elimina datos/tipo y deshabilita solo tras FDS success; `FLASH_WRITE_FAIL` conserva config/owner |
| 1025 | `GET_BATTERY_INFO` | ignorado | `millivolts:u16be, percent:u8` |
| 1026 | `GET_BUTTON_PRESS_CONFIG` | `button:u8` (`A/B/a/b`) | `function:u8` |
| 1027 | `SET_BUTTON_PRESS_CONFIG` | `button:u8, function:u8` | Configura pulsación corta; persistir con 1013 |
| 1028 | `GET_LONG_BUTTON_PRESS_CONFIG` | `button:u8` (`A/B/a/b`) | `function:u8` |
| 1029 | `SET_LONG_BUTTON_PRESS_CONFIG` | `button:u8, function:u8` | Configura pulsación larga; persistir con 1013 |
| 1030 | `SET_BLE_PAIRING_KEY` | 6 dígitos ASCII | Cambia PIN en RAM; persistir con 1013 |
| 1031 | `GET_BLE_PAIRING_KEY` | ignorado | 6 dígitos ASCII |
| 1032 | `DELETE_ALL_BLE_BONDS` | vacío obligatorio | Solicita borrado asíncrono de todos los bonds |
| 1033 | `GET_DEVICE_MODEL` | ignorado | `model:u8`: 0 Ultra, 1 Lite |
| 1034 | `GET_DEVICE_SETTINGS` | ignorado | 14 bytes: versión, animación, 4 botones, pairing, PIN[6], sleep timeout s |
| 1035 | `GET_DEVICE_CAPABILITIES` | ignorado | Lista de IDs `u16be`, sin contador; fuente de disponibilidad |
| 1036 | `GET_BLE_PAIRING_ENABLE` | ignorado | `enabled:u8` |
| 1037 | `SET_BLE_PAIRING_ENABLE` | `enabled:u8` (`0/1`) | Cambia ajuste en RAM; persistir con 1013 |
| 1038 | `GET_ALL_SLOT_NICKS` | ignorado | Por slot: `hf_len, hf_nick, lf_len, lf_nick` |
| 1039 | `GET_SLEEP_TIMEOUT` | ignorado | `seconds:u8` |
| 1040 | `SET_SLEEP_TIMEOUT` | `seconds:u8` (`5..60`) | Cambia timeout; persistir con 1013 |
| 1051 | `GET_KEYBOARD_HID_ENABLE` | ignorado | `enabled:u8` |
| 1052 | `SET_KEYBOARD_HID_ENABLE` | `enabled:u8` (`0/1`) | Opt-in HID de teclado (off por defecto); cambia ajuste en RAM, persistir con 1013 y reiniciar para (des)exponer HID USB/BLE |
| 1053 | `SET_RUNTIME_UNDERCOVER_MODE` | `enabled:u8` (`0/1`) | Sólo enlace NUS BLE activo. Suprime en RAM la barra RGB y el LED de campo; no persiste y se revoca automáticamente al desconectarse ese enlace periférico |

Funciones de botón: `0=disabled`, `1=next slot`, `2=previous slot`,
`3=clone UID`, `4=battery`, `5=NFC field`, `6=reader-key capture`.
Clone UID y NFC field requieren Ultra. El firmware acepta el byte antes de que
la validación de guardado rechace valores fuera de rango.

### 1041-1049: keyboard payload

Estos comandos usan bytecode RAM-only, versión 1, máximo 4096 bytes. Por BLE
requieren enlace NUS cifrado, MITM y LE Secure Connections.

| ID | Comando | Petición | Respuesta/efecto |
|---:|---|---|---|
| 1041 | `KEYBOARD_UPLOAD_BEGIN` | `version:u8, total:u16be, crc32:u32be` | `version, upload_id:u32be, next_offset:u16be, max_chunk:u16be` |
| 1042 | `KEYBOARD_UPLOAD_CHUNK` | `version, upload_id:u32be, offset:u16be, data[1..4089]` | `upload_id:u32be, next_offset:u16be`; reintentos deben ser idénticos |
| 1043 | `KEYBOARD_UPLOAD_COMMIT` | `version, upload_id:u32be` | `commit_id:u32be, length:u16be, crc32:u32be`; valida bytecode y CRC |
| 1044 | `KEYBOARD_RUN` | `version, commit_id:u32be, outputs:u8` | `run_id:u32be`; outputs 1 USB, 2 BLE, 3 ambos |
| 1045 | `KEYBOARD_CANCEL` | vacío | Cancela ejecución, armado o upload; libera teclas |
| 1046 | `KEYBOARD_GET_STATUS` | vacío | Registro fijo de 28 bytes descrito abajo |
| 1047 | `KEYBOARD_CLEAR` | vacío | Borra buffers/IDs RAM; falla si está Running |
| 1048 | `KEYBOARD_SET_TEMP_BLE_NAME` | `version, name_len, utf8_name[0..26]` | `version, effective_len, effective_name`; 0 restaura nombre default |
| 1049 | `KEYBOARD_ARM_BLE` | `version, commit_id:u32be` | `run_id:u32be`; ejecuta una vez al estar BLE HID listo |

Respuesta 1046:

```text
version:u8, state:u8, error:u8, outputs:u8,
upload_id:u32be, commit_id:u32be, run_id:u32be,
expected:u16be, received:u16be, program_counter:u16be,
length:u16be, crc32:u32be
```

Estados: 0 Empty, 1 Uploading, 2 Ready, 3 Running, 4 Complete,
5 Cancelled, 6 Error, 7 Armed. Véase
[Keyboard command reference](keyboard-command-reference.md) para opcodes y errores.

### 1050: snapshot atómico del slot activo

`ACTIVE_SLOT_SNAPSHOT` está anunciado por Ultra y Lite. Usa protocolo versión 2,
generation estable de owner y revision opaca, ambas no nulas `u32be`:

| Operación | Petición exacta | Respuesta `SUCCESS` exacta |
|---|---|---|
| `BEGIN=0` | `version=2, operation=0` | `version=2, operation=0, slot:u8, tag_type:u16be, owner_generation:u32be, revision:u32be` (13 bytes) |
| `SAVE_RELEASE=1` | `version=2, operation=1, revision:u32be` | Eco exacto de 6 bytes |
| `ABORT=2` | `version=2, operation=2, revision:u32be` | Eco exacto de 6 bytes |

`BEGIN` requiere tag mode y owner MFC Mini/1K/2K/4K cargado. Desarma sensing HF/LF
antes de publicar la transacción y la liga a slot, tipo y transporte USB/BLE. La
generation cambia al cargar/reload de owner, pero no con escrituras RF ni random
UID; la identidad host es `{slot,tipo,generation}`, nunca UID anticollision. El
owner puede usar solo 1050 y las lecturas 4008/4009/4016/4018; 4008 exige rango
exacto del tipo y las otras tres petición vacía. Una lectura válida refresca lease
idle de 5 s; el lease absoluto de 120 s no se refresca. Expiry equivale a `ABORT`.

Durante el freeze, otro transporte o cualquier comando no permitido recibe
`DEVICE_MODE_ERROR`; lectura permitida mal formada recibe `PAR_ERR`. Revision
incorrecta recibe `CMD_ERR`. `SAVE_RELEASE` exige Normal write mode y revalida
owner/slot/tipo; fuerza solo el record HF activo aunque coincida CRC. Cada espera
FDS dura máximo 15 s y write/GC/retry máximo 45 s. Firmware reserva 46 s antes de
aceptar SAVE, lo rechaza si queda menos lease absoluto y suspende expiry mientras
el commit bloqueante está aceptado; GUI/Python esperan 55 s y desconectan si vence
ese timeout. Fallo FDS
devuelve `FLASH_WRITE_FAIL` y conserva transacción/revision para retry. Baseline
CRC cambia solo tras FDS success. `ABORT` y expiry no escriben. Véase
[Atomic active-slot snapshots](active-slot-snapshot.md).

## 2000-2201: lector HF (solo Ultra)

Los comandos que usan el hook HF requieren reader mode. Salvo los que mantienen
el campo, el firmware selecciona la tarjeta, ejecuta la operación y apaga RF.

| ID | Comando | Petición | Respuesta correcta/efecto |
|---:|---|---|---|
| 2000 | `HF14A_SCAN` | ignorado | `uid_len, uid, atqa[2], sak, ats_len, ats`; apaga RF |
| 2001 | `MF1_DETECT_SUPPORT` | ignorado | Vacía; status indica soporte MIFARE Classic |
| 2002 | `MF1_DETECT_PRNG` | ignorado | `type:u8`: 0 static, 1 weak, 2 hard |
| 2003 | `MF1_STATIC_NESTED_ACQUIRE` | `known_type, known_block, key[6], target_type, target_block` | `uid[4], nt1[4], nt1enc[4], nt2[4], nt2enc[4]` |
| 2004 | `MF1_DARKSIDE_ACQUIRE` | `target_type, target_block, first, sync_max` | Substatus; si 0 añade UID/NT/paridad/keystream/NR/AR |
| 2005 | `MF1_DETECT_NT_DIST` | `known_type, known_block, key[6]` | `uid:u32be, nonce_distance:u32be` |
| 2006 | `MF1_NESTED_ACQUIRE` | Igual que 2003 | Dos registros `nt[4], nt_enc[4], parity:u8` |
| 2007 | `MF1_AUTH_ONE_KEY_BLOCK` | `key_type, block, key[6]` | Vacía; status indica autenticación |
| 2008 | `MF1_READ_ONE_BLOCK` | `key_type, block, key[6]` | Bloque de 16 bytes |
| 2009 | `MF1_WRITE_ONE_BLOCK` | `key_type, block, key[6], data[16]` | Escribe bloque físico |
| 2010 | `HF14A_RAW` | `options, timeout_ms:u16be, tx_bits:u16be, tx_data` | Datos RF raw, máximo 64 bytes |
| 2011 | `MF1_MANIPULATE_VALUE_BLOCK` | `src_type, src_block, src_key[6], op, operand:u32be, dst_type, dst_block, dst_key[6]` | Increment/decrement/restore y transfer |
| 2012 | `MF1_CHECK_KEYS_OF_SECTORS` | `mask[10], keys[N*6]` | `found_mask[10]` y 80 claves de 6 bytes; selección inicial y reselección rápida entre intentos |
| 2013 | `MF1_HARDNESTED_ACQUIRE` | `slow, known_type, known_block, key[6], target_type, target_block` | Registros de 9 bytes `nt1enc, nt2enc, parity`; sin contador |
| 2014 | `MF1_ENC_NESTED_ACQUIRE` | `backdoor_key[6], sector_count, start_sector` | `cuid[4]` y 14 bytes por sector (A+B) |
| 2015 | `MF1_CHECK_KEYS_ON_BLOCK` | `block, key_type, key_count, keys[N*6]` | `found:u8, key[6]` |
| 2016 | `HF14A_SCAN_KEEP` | ignorado | Igual que 2000, pero mantiene selección/campo |
| 2017 | `HF14A_AUTH_TRACE` | `key_type, block, key[6]` y timeout opcional `u16be` | Registros `bits_direction:u16be, frame[...]` |
| 2018 | `MF1_READ_BLOCKS` | `key_type, start_block, count, key[6]` | 1..16 bloques del mismo sector tras una autenticación |
| 2020 | `HF14A_SNIFF` | timeout opcional `u16be` | Hasta 3800 bytes de registros de frames; pensado para emulator mode |
| 2021 | `HF_CAPTURE_START` | `version=2, mode:u8, start_token:u32be` | Metadata v2 de 48 bytes; el token no cero hace idempotente un reintento exacto en el mismo transporte |
| 2022 | `HF_CAPTURE_STATUS` | `version=2, session_id:u32be, start_token:u32be` | Metadata v2; exige la capacidad exacta antes de reasignar ownership o recuperar ID 0 |
| 2023 | `HF_CAPTURE_GET` | `version=2, session_id:u32be, ack_present:u8, ack_sequence:u32be, ack_delivery_token:u64be, requested_bytes:u16be` | Respuesta de 72..4096 bytes con CRC32; `requested_bytes` debe ser 605..4096 y el ACK se aplica antes de construirla |
| 2024 | `HF_CAPTURE_STOP` | `version=2, session_id:u32be` | Detiene RF pero conserva registros no confirmados; devuelve metadata |
| 2025 | `HF_CAPTURE_EVENT` | Nunca se solicita | Evento no solicitado con metadata v2 cuando hay datos o overflow |
| 2100 | `HF14A_SET_FIELD_ON` | vacío obligatorio | Enciende y mantiene el campo HF |
| 2101 | `HF14A_SET_FIELD_OFF` | vacío obligatorio | Aborta sesión ISO-DEP y apaga HF |
| 2200 | `HF14A_GET_CONFIG` | ignorado | 4 bytes: force BCC, CL2, CL3, RATS |
| 2201 | `HF14A_SET_CONFIG` | 4 bytes signed (`0..2` cada uno) | Cambia configuración RAM de anticollision/RATS |

Opciones de 2010: `04` check/strip CRC RX, `08` mantener RF, `10`
auto-select, `20` añadir CRC TX, `40` esperar respuesta, `80` activar RF.
El tamaño TX máximo es 64 bytes (62 al añadir CRC).

### Captura HF continua 2021-2025

Los modos son `0` emulación, `1` monitor pasivo y `2` trazado lector. El modo
pasivo no transmite ni entra en la máquina de emulación, pero con un único Ultra
solo garantiza reader→card: el front-end NFCT no puede observar simultáneamente
la respuesta de una tarjeta externa. El modo lector registra las operaciones RF
que ejecuten comandos lectores; START por sí solo no genera sondeos.

Metadata v2, 48 bytes:

```
version:u8, state:u8, mode:u8, flags:u8,
session_id:u32be, first_sequence:u32be, next_sequence:u32be,
stored_records:u32be, observed_records:u32be, dropped_records:u32be,
used_bytes:u16be, capacity_bytes:u16be, elapsed_ticks:u64be,
boot_id:u32be, start_token:u32be
```

`state` es 1 running o 2 stopped. `flags bit0` indica que hubo overflow. Los
`boot_id` cambia al reiniciar firmware y evita reanudar una sesión homónima de
otro arranque. `start_token` identifica el intento START que creó la sesión y
permite distinguir una respuesta perdida de una sesión anterior. Los timestamps
usan ticks acumulados de `app_timer`; la secuencia avanza incluso
para registros descartados, de modo que los huecos y `dropped_records` son
evidencia explícita de pérdida.

Cabecera de página v2, 72 bytes: la metadata anterior seguida de
`page_first_sequence:u32be, page_next_sequence:u32be, record_count:u16be,
record_bytes:u16be, crc32:u32be, delivery_token:u64be`. El token usa el rango
positivo `1..7FFFFFFFFFFFFFFF`. El CRC32 IEEE cubre la cabecera completa y el
stream de registros, tratando su propio campo como cuatro bytes cero. Cada
registro es:

```
body_length:u16be, version:u8, type:u8, sequence:u32be,
timestamp_ticks:u64be, direction:u8, flags:u8,
bit_length:u16be, data_length:u16be, data[data_length]
```

Tipos: 1 frame, 2 cambio de campo. Direcciones: 0 reader→card, 1 card→reader,
2 evento. Flags de frame: bit0 paridad empaquetada, bit1 CRC añadido por
hardware y bit2 error RF.

`ack_present=0` no confirma nada y usa secuencia/token cero; con
`ack_present=1`, el firmware acepta
únicamente el token de la página entregada más reciente y una secuencia presente
en su prefijo; entonces elimina los registros hasta ella inclusive, incluida
`FFFFFFFF`. Repetir el mismo par token/secuencia es idempotente, incluso después
de reutilizar una secuencia por wrap. El ACK debe enviarse solo después de
persistir y validar la página completa. GET y STOP rechazan transportes no
propietarios. STOP conserva datos para el drenaje final y un nuevo START se
rechaza mientras queden registros. Al perderse el enlace, firmware invalida el
owner y deja de emitir eventos hasta que STATUS presente el token exacto. Un ID
de sesión exacto puede reasignar ownership a otro transporte con esa capacidad.
El ID 0 se reserva para recuperar un START de respuesta incierta y solo funciona
desde el mismo tipo de transporte que creó la sesión.

## 3000-3032: lector LF (solo Ultra)

Los writers de protocolos a T55xx usan el sufijo común
`new_password[4], old_password[4]...` con 1..255 passwords antiguas. T55xx no
confirma escritura; verificar leyendo por separado.

| ID | Comando | Petición | Respuesta correcta/efecto |
|---:|---|---|---|
| 3000 | `EM410X_SCAN` | ignorado | `tag_type:u16be, id[5 o 13]` |
| 3001 | `EM410X_WRITE_TO_T55XX` | `id[5]` + sufijo passwords | Programa EM410X |
| 3002 | `HIDPROX_SCAN` | vacío o `format_hint:u8` | Descriptor HID de 16 bytes |
| 3003 | `HIDPROX_WRITE_TO_T55XX` | descriptor[13] + passwords | Programa HID Prox |
| 3004 | `VIKING_SCAN` | ignorado | ID de 4 bytes |
| 3005 | `VIKING_WRITE_TO_T55XX` | id[4] + passwords | Programa Viking |
| 3006 | `EM410X_ELECTRA_WRITE_TO_T55XX` | id[13] + passwords | Programa Electra |
| 3009 | `ADC_GENERIC_READ` | ignorado | Hasta 800 muestras ADC escaladas |
| 3010 | `IOPROX_SCAN` | format hint opcional | Descriptor ioProx de 16 bytes |
| 3011 | `IOPROX_WRITE_TO_T55XX` | descriptor[16] + passwords | Programa ioProx |
| 3012 | `IOPROX_DECODE_RAW` | raw frame[8] | Descriptor ioProx de 16 bytes |
| 3013 | `IOPROX_COMPOSE_ID` | `version, facility, card_number:u16be` | Descriptor ioProx de 16 bytes |
| 3014 | `PAC_SCAN` | ignorado | ID PAC de 8 bytes |
| 3015 | `PAC_WRITE_TO_T55XX` | id[8] + passwords | Programa PAC; bytes ID deben ser de 7 bits |
| 3016 | `LF_T55XX_WRITE` | `block, word:u32be, use_password, password:u32be, page1` | Escribe palabra exacta T55xx |
| 3018 | `IDTECK_WRITE_TO_T55XX` | frame[8] + passwords | Programa IDTECK |
| 3019 | `JABLOTRON_SCAN` | ignorado | ID de 5 bytes |
| 3020 | `JABLOTRON_WRITE_TO_T55XX` | id[5] + passwords | Programa Jablotron |
| 3030 | `EM4X05_SCAN` | ignorado | Actualmente siempre `STATUS_NOT_IMPLEMENTED` |
| 3031 | `LF_SNIFF` | timeout opcional `u16be` | Hasta 4000 muestras ADC, ~125 kHz |
| 3032 | `EM4X05_READSNIFF` | Sin protocolo | **Definido pero no despachado**; devuelve `INVALID_CMD` |

Descriptor HID Prox de 13 bytes:
`format, facility:u32be, card_number:u40be, issue, oem:u16be`.
Descriptor ioProx de 16 bytes:
`version, facility, card_number:u16be, raw[8], reserved[4]`.

## 4000-4044: emulación HF (Ultra y Lite)

Los setters modifican RAM salvo que se indique otra cosa. Para persistir el slot
activo use 1009, cambie de slot o apague de forma controlada. Los comandos host
de memoria ignoran access bits y write mode porque no son escrituras RF.
Los comandos que leen o mutan el buffer MFC o MF0/NTAG requieren que el tipo
configurado sea el owner exacto cargado del slot activo; 4001/4018 aceptan
cualquier owner HF cargado. La telemetría retenida 4005/4006 y 4034/4035 no toca
ese buffer y permanece descargable tras cambiar owner. Incompatibilidad o load
fallido devuelve `INVALID_SLOT_TYPE`. 4044 es una animación reader-only y tampoco
requiere owner de emulación.

| ID | Comando | Petición | Respuesta correcta/efecto |
|---:|---|---|---|
| 4000 | `MF1_WRITE_EMU_BLOCK_DATA` | `start_block, blocks[N*16]`, N=1..255 | Escribe RAM MFC; solo limita a 256 bloques |
| 4001 | `HF14A_SET_ANTI_COLL_DATA` | `uid_len, uid, atqa[2], sak, ats_len, ats` | Cambia UID/ATQA/SAK/ATS del slot HF activo |
| 4004 | `MF1_SET_DETECTION_ENABLE` | `enabled:u8` | 1 limpia y arma log; 0 desarma sin borrar |
| 4005 | `MF1_GET_DETECTION_COUNT` | ignorado | `count:u32be`, máximo 1000 |
| 4006 | `MF1_GET_DETECTION_LOG` | `start_index:u32be` | Registros de autenticación de 18 bytes, hasta 227 por página |
| 4007 | `MF1_GET_DETECTION_ENABLE` | ignorado | `enabled:u8` |
| 4008 | `MF1_READ_EMU_BLOCK_DATA` | `start_block, count` (1..32) | `count*16` bytes de RAM MFC |
| 4009 | `MF1_GET_EMULATOR_CONFIG` | ignorado | `detection, gen1a, gen2, block0_coll, write_mode` |
| 4010 | `MF1_GET_GEN1A_MODE` | ignorado | `enabled:u8` |
| 4011 | `MF1_SET_GEN1A_MODE` | `enabled:u8` | Habilita/deshabilita backdoor Gen1A |
| 4012 | `MF1_GET_GEN2_MODE` | ignorado | `enabled:u8` |
| 4013 | `MF1_SET_GEN2_MODE` | `enabled:u8` | Permite/deniega escritura autenticada de bloque 0 |
| 4014 | `MF1_GET_BLOCK_ANTI_COLL_MODE` | ignorado | `enabled:u8` |
| 4015 | `MF1_SET_BLOCK_ANTI_COLL_MODE` | `enabled:u8` | Usa UID/ATQA/SAK de bloque 0 para UID de 4 bytes |
| 4016 | `MF1_GET_WRITE_MODE` | ignorado | `mode:u8` (puede devolver transición 4) |
| 4017 | `MF1_SET_WRITE_MODE` | `mode:u8` (`0..3`) | Establece Normal/Denied/Deceive/Shadow |
| 4018 | `HF14A_GET_ANTI_COLL_DATA` | ignorado | `uid_len, uid, atqa[2], sak, ats_len, ats` |
| 4019 | `MF0_NTAG_GET_UID_MAGIC_MODE` | ignorado | `enabled:u8`; `PAR_ERR` si no hay MF0 válido |
| 4020 | `MF0_NTAG_SET_UID_MAGIC_MODE` | `enabled:u8` | Control de UID magic MF0/NTAG |
| 4021 | `MF0_NTAG_READ_EMU_PAGE_DATA` | `start_page, count` | `count*4` bytes; en error de rango puede incluir `page_count` |
| 4022 | `MF0_NTAG_WRITE_EMU_PAGE_DATA` | `start_page, count, pages[count*4]` | Escribe páginas RAM |
| 4023 | `MF0_NTAG_GET_VERSION_DATA` | ignorado | Version data[8] |
| 4024 | `MF0_NTAG_SET_VERSION_DATA` | version[8] | Escribe version data RAM |
| 4025 | `MF0_NTAG_GET_SIGNATURE_DATA` | ignorado | Signature[32] |
| 4026 | `MF0_NTAG_SET_SIGNATURE_DATA` | signature[32] | Escribe firma RAM |
| 4027 | `MF0_NTAG_GET_COUNTER_DATA` | `index:u8` | `counter[3], tearing:u8` |
| 4028 | `MF0_NTAG_SET_COUNTER_DATA` | `index_flags, counter[3]` | Bit 7 de index limpia tearing |
| 4029 | `MF0_NTAG_RESET_AUTH_CNT` | vacío obligatorio | Devuelve contador AUTH anterior y lo limpia |
| 4030 | `MF0_NTAG_GET_PAGE_COUNT` | ignorado | `public_page_count:u8` |
| 4031 | `MF0_NTAG_GET_WRITE_MODE` | ignorado | `mode:u8` |
| 4032 | `MF0_NTAG_SET_WRITE_MODE` | `mode:u8` (`0..4`) | Establece write mode MF0/NTAG |
| 4033 | `MF0_NTAG_SET_DETECTION_ENABLE` | `enabled:u8` | 1 limpia y arma log de passwords; 0 conserva log |
| 4034 | `MF0_NTAG_GET_DETECTION_COUNT` | ignorado | `count:u32be`, máximo 32 |
| 4035 | `MF0_NTAG_GET_DETECTION_LOG` | `start_index:u32be` | Passwords intentados, 4 bytes cada uno |
| 4036 | `MF0_NTAG_GET_DETECTION_ENABLE` | ignorado | `enabled:u8` |
| 4037 | `MF0_NTAG_GET_EMULATOR_CONFIG` | ignorado | `detection, uid_magic, write_mode` |
| 4038 | `MF1_SET_FIELD_OFF_DO_RESET` | `enabled:u8` | Cambia config; efecto RF completo al recargar slot |
| 4039 | `MF1_GET_FIELD_OFF_DO_RESET` | ignorado | `enabled:u8` |
| 4040 | `MF1_GET_PRNG_TYPE` | ignorado | `type:u8`: 0 static, 1 weak, 2 hard |
| 4041 | `MF1_SET_PRNG_TYPE` | `type:u8` (`0..2`) | Devuelve el type seleccionado |
| 4042 | `MF1_SET_RANDOM_UID_MODE` | `enabled:u8` | Genera UID nuevo en cada activación; desactiva block0 mode |
| 4043 | `MF1_GET_RANDOM_UID_MODE` | ignorado | `enabled:u8` |
| 4044 | `MF1_SET_READER_KEYS_ANIM` | `enabled:u8` | Animación rainbow RAM-only de captura Reader Keys |

Registro 4006, exactamente 18 bytes:

```text
block:u8
flags:u8                 # bit0 Key B, bit1 nested
cuid:u32be
nt:u32be
nr_encrypted:u32be
ar_encrypted:u32be
```

Se registran autenticaciones completas correctas y fallidas. El log es global,
está en RAM retenida `.noinit_mf1`, satura a 1000 y no se guarda en FDS.

Write modes MFC/MF0:

| Valor | Modo | Escritura recibida por RF |
|---:|---|---|
| 0 | Normal | ACK y modifica RAM; un save posterior puede persistir |
| 1 | Denied | NAK, no modifica |
| 2 | Deceive | ACK, no modifica |
| 3 | Shadow | ACK y modifica RAM, pero no persiste cambios RF |
| 4 | Shadow request | Estado interno para guardar una baseline y pasar a 3 |

Si FDS rechaza la baseline de una transición 4 -> 3, firmware restaura el estado
4 y devuelve `FLASH_WRITE_FAIL`; un retry vuelve a preparar la baseline. El modo
3 solo queda activo después de persistir correctamente.

Los comandos host 4000, 4022, 4024, 4026, 4028 y 4029 escriben RAM aunque el
write mode RF sea Denied/Deceive/Shadow.

## 5000-5013: emulación LF (Ultra y Lite)

Los setters reinician la modulación LF pero no guardan inmediatamente en FDS.
Los getters solo devuelven datos si el tipo configurado es también el owner exacto
cargado del buffer LF; de lo contrario responden `PAR_ERR` sin bytes stale.

| ID | Comando | Petición | Respuesta correcta/efecto |
|---:|---|---|---|
| 5000 | `EM410X_SET_EMU_ID` | ID[5] o Electra ID[13], según slot | Cambia ID emulado |
| 5001 | `EM410X_GET_EMU_ID` | ignorado | `tag_type:u16be, id[5 o 13]` |
| 5002 | `HIDPROX_SET_EMU_ID` | Descriptor HID[13] | Cambia HID Prox emulado |
| 5003 | `HIDPROX_GET_EMU_ID` | ignorado | Descriptor HID[13] |
| 5004 | `VIKING_SET_EMU_ID` | ID[4] | Cambia Viking emulado |
| 5005 | `VIKING_GET_EMU_ID` | ignorado | ID[4] |
| 5006 | `PAC_SET_EMU_ID` | ID[8], bytes de 7 bits | Cambia PAC emulado |
| 5007 | `PAC_GET_EMU_ID` | ignorado | ID[8] |
| 5008 | `IOPROX_SET_EMU_ID` | Descriptor ioProx[16] | Cambia ioProx emulado |
| 5009 | `IOPROX_GET_EMU_ID` | ignorado | Descriptor ioProx[16] |
| 5010 | `JABLOTRON_SET_EMU_ID` | ID[5] | Cambia Jablotron emulado |
| 5011 | `JABLOTRON_GET_EMU_ID` | ignorado | ID[5] |
| 5012 | `IDTECK_SET_EMU_ID` | Frame[8] | Cambia IDTECK emulado |
| 5013 | `IDTECK_GET_EMU_ID` | ignorado | Frame[8] |

## 6000-6014: ISO14443-4, ISO-DEP y EMV (solo Ultra)

Los APDU responses contienen su propio `SW1 SW2`. Un SW de error de tarjeta puede
llegar con status externo `HF_TAG_OK`.
Los comandos de emulación 6000-6003 requieren un owner `HF14A_4` cargado. Los
counters globales 6010 y los comandos reader 6004-6009 y 6011-6014 no usan ese
guard.

| ID | Comando | Petición | Respuesta correcta/efecto |
|---:|---|---|---|
| 6000 | `HF14A_4_APDU_RECV` | vacío | APDU pendiente de emulación, hasta 260; `HF_TAG_NO` si no hay |
| 6001 | `HF14A_4_APDU_SEND` | `response_len:u16be, response[...]` | Entrega respuesta host al emulador tras WTX |
| 6002 | `HF14A_4_SET_ANTI_COLL` | `uid_len, uid, atqa[2], sak, ats_len, ats` | Configura identidad ISO-DEP emulada |
| 6003 | `HF14A_4_STATIC_RESP` | `00` limpia; o `prefix_len, prefix, resp_len:u16be, response` | Añade regla APDU-prefix -> response |
| 6004 | `HF14A_4_READER_APDU` | APDU[1..512] | Selecciona/RATS e intercambia un APDU; respuesta hasta 512 |
| 6005 | `HF14A_4_EMV_SCAN` | vacío o amount n12 BCD[6] | Scan EMV legacy empaquetado; con amount puede cambiar estado |
| 6006 | `HF14A_4_DESFIRE_SCAN` | vacío | Enumeración DESFire bounded en formato legacy |
| 6007 | `HF14A_4_EMV_TRACE_START` | Request versionado de 25/30/35 bytes | `version, state, scan_id:u32be, flags:u32be` |
| 6008 | `HF14A_4_EMV_TRACE_META` | `version, scan_id:u32be` | Metadata, conteos, CRC32, timing e identidad de tarjeta |
| 6009 | `HF14A_4_EMV_TRACE_GET` | `version, scan_id, start_record:u32be, requested_bytes:u16be` | Página de registros atómicos, nunca partidos |
| 6010 | `HF14A_4_DEBUG_COUNTERS` | ignorado | 4 bytes: I-blocks RX/TX, last PCB, last static match |
| 6011 | `HF14A_4_READER_SESSION_START` | vacío | `session_id:u32be` + identidad; deja ISO-DEP/RF activo y liga la sesión al transporte solicitante |
| 6012 | `HF14A_4_READER_SESSION_EXCHANGE` | `session_id:u32be, APDU[1..512]` | Respuesta APDU manteniendo block state; solo transporte owner |
| 6013 | `HF14A_4_READER_SESSION_STOP` | `session_id:u32be` | S(DESELECT), cierra sesión y apaga campo; también sirve como reset ordenado |
| 6014 | `HF14A_4_READER_SESSION_START_APPLE_TRANSIT` | vacío | Envía polling ECP2 Apple Transit antes de activación; misma respuesta/sesión owner que 6011 |

Formato legacy de 6005/6006:

```text
uid_len, uid, atqa[2], sak, ats_len, ats, num_apdus,
repeated:
  cmd_len:u8, command[cmd_len],
  resp_len:u16le, response[resp_len]   # excepción little-endian
```

Request base 6007:

```text
version=1, options, max_aids, max_records_per_app,
max_apdus:u16be, budget_ms:u32be,
amount[6], country[2], currency[2], date[3],
transaction_type, cryptogram_type
```

Con option bit 7 se añaden `terminal_profile, custom_ttq[4]`. La variante de 35
bytes añade `polling_profile, behavior, poll_retries, poll_delay_ms,
poll_timeout_ms`. META y GET deben validar `scan_id`, cursores, longitudes
atómicas y CRC32 antes de exportar. Véase
[ISO-DEP and APDU command reference](apdu-command-reference.md) para todos los
flags y layouts de records.

Si un sweep solicita reacquire entre perfiles, cada activación debe completarse y
conservar el UID de la tarjeta retenida al inicio. Perder la tarjeta o leer un UID
distinto aborta el trace para no marcar evidencia incompleta como complete ni
mezclar APDUs de dos tarjetas.

6011 y 6014 son START alternativos; ambos continúan con 6012 y terminan con 6013.
6014 instala temporalmente el frame ECP2
`6A 02 C8 01 00 03 00 02 79 00 00 00 00 C2 D8`, con 30 intentos,
5 ms de pausa y timeout de 2 ms, y limpia la anotación después de activar o
fallar. Las sesiones no tienen timeout de inactividad. Quedan ligadas al transporte
USB/BLE del START; mientras están activas, cualquier comando del otro transporte
recibe `STATUS_DEVICE_MODE_ERROR` sin afectar la sesión. STOP/reset explícito, un
START de reemplazo del owner, fallo RF, salida de reader mode u otra invalidación
segura del owner, y pérdida del enlace owner invalidan el token y apagan el campo.

Un 6012 con resultado host incierto nunca se reintenta. Si framing y transporte
siguen válidos, una respuesta vacía al 6013 ordenado con status `0068`, `0060` o
`0066` confirma que la sesión anterior está cerrada. Esa respuesta es una barrera:
el host puede limpiar solo la cuarentena de 6012 y reutilizar la conexión. Debe
reconectar únicamente si no puede confirmar el reset o si framing/transporte ya se
invalidó.

## 7000-7054: BLE (Ultra y Lite)

Los comandos asíncronos solo confirman que la operación fue aceptada; el cliente
debe consultar 7012 o el getter específico. Direcciones BLE se transmiten como
`addr[6]` en orden little-endian del SoftDevice.

### 7000-7006: scan, advertising local y probe

| ID | Comando | Petición | Respuesta correcta/efecto |
|---:|---|---|---|
| 7000 | `BLE_SCAN_START` | `active:u8` (`0/1`) | Inicia scan; passive=0 no envía scan requests |
| 7001 | `BLE_SCAN_STOP` | vacío | Detiene scan sin borrar cache |
| 7002 | `BLE_SCAN_GET_COUNT` | vacío | `distinct_count:u8`, máximo 40 |
| 7003 | `BLE_SCAN_GET_RESULTS` | `start_index:u8` | Records `addr[6], type, rssi:i8, adv_len, adv` |
| 7004 | `BLE_ADVERTISING_SET` | `enabled, erase_bonds` | `effective_state:u8`; controla advertising normal |
| 7005 | `BLE_ADVERTISING_GET` | vacío | `state:u8` |
| 7006 | `BLE_LINK_PROBE` | vacío/00 enlace actual, 01 batch cache | Resultado/progreso en 7012 |

### 7010-7032: central, GATT y device info

| ID | Comando | Petición | Respuesta correcta/efecto |
|---:|---|---|---|
| 7010 | `BLE_CONNECT` | `addr_type, addr[6]` | Inicia conexión central asíncrona |
| 7011 | `BLE_DISCONNECT` | vacío | Cancela conexión/operación o desconecta |
| 7012 | `BLE_CENTRAL_STATE` | vacío | Estado central fijo de 21 bytes |
| 7013 | `BLE_GATT_DISCOVER` | vacío | Descubre characteristics, máximo 48 |
| 7014 | `BLE_GATT_GET_CHARS` | `start_index:u8` | Records `handle:u16be, properties, uuid_type, uuid:u16be` |
| 7015 | `BLE_FUZZ_START` | `handle:u16be, max_iter:u16be, interval_ms:u16be` | Inicia corpus de writes; max=0 hasta stop |
| 7016 | `BLE_FUZZ_STOP` | vacío | Detiene fuzzer |
| 7017 | `BLE_FUZZ_GET_LOG` | `start_index:u16be` | Records index/len/status/primeros 16 bytes |
| 7018 | `BLE_GATT_READ` | `value_handle:u16be` | Inicia read asíncrono |
| 7019 | `BLE_GATT_GET_READ` | vacío | `state, gatt_status, len, value[len]`, máximo 244 |
| 7020 | `BLE_SUBSCRIBE` | `cccd_handle:u16be, mode` | mode 0 off, 1 notify, 2 indicate |
| 7021 | `BLE_GET_NOTIFICATIONS` | `start_index:u16be` | Records `handle:u16be, len, data[len]`; máximo 64 |
| 7022 | `BLE_FIND_CCCD` | `value_handle:u16be` | Inicia búsqueda descriptor CCCD |
| 7023 | `BLE_GET_CCCD` | vacío | `state, cccd_handle:u16be` |
| 7024 | `BLE_GATT_WRITE` | `value_handle:u16be, data[1..244]` | Write-with-response; respeta MTU-3 |
| 7025 | `BLE_GET_WRITE` | vacío | `state, gatt_status` |
| 7026 | `BLE_GET_MTU` | vacío | `effective_mtu:u16be`, default 23, máximo 247 |
| 7027 | `BLE_DESC_DISCOVER` | vacío | Descubre descriptors, máximo 48 |
| 7028 | `BLE_DESC_GET` | `start_index:u8` | `state` + records `handle, uuid_type, uuid` |
| 7029 | `BLE_SVC_DISCOVER` | vacío | Descubre primary services, máximo 16 |
| 7030 | `BLE_SVC_GET` | `start_index:u8` | `state` + UUID/start/end por servicio |
| 7031 | `BLE_DEVICE_INFO` | vacío | Lee GAP/DIS/Battery tras discovery |
| 7032 | `BLE_GET_DEVICE_INFO` | vacío | `state, count` + UUID/status/len/value |

Respuesta 7012:

```text
conn_state, discovery_state, characteristic_count, fuzz_state,
fuzz_sent:u16be, target_alive, last_disconnect_reason,
probe_state, probe_result, probe_index, probe_total,
flood_state, flood_sent:u32be,
read_state, write_state, notification_count:u16be
```

### 7040-7047: radio propio, flood y ciclos

| ID | Comando | Petición | Respuesta correcta/efecto |
|---:|---|---|---|
| 7040 | `BLE_SET_ADDR` | `mode` o `mode=1, addr[6]` | 0 restore, 1 static, 2 RPA, 3 NRPA |
| 7041 | `BLE_GET_ADDR` | vacío | `addr_type, addr[6]` |
| 7042 | `BLE_RADIO_SET` | `on:u8` | 0 para radio policy off solo se permite por USB |
| 7043 | `BLE_RADIO_GET` | vacío | `radio_on, advertising, scanning, central_connected` |
| 7044 | `BLE_FLOOD_START` | `scope, handle_or_fill:u16be, size, max_iter:u16be, interval_ms:u16be` | Scope 0 enlace, 1 cache, 2 advertising flood |
| 7045 | `BLE_FLOOD_STOP` | vacío | Detiene floods central/cache/advertising |
| 7046 | `BLE_FLOOD_COUNT` | vacío | `accepted_write_commands:u32be` |
| 7047 | `BLE_KICK` | `scope, cycles` | Scope 0 enlace actual; scope 1 cache, 1..10 ciclos |

### 7050-7054: advertising flood y advertising lab

| ID | Comando | Petición | Respuesta correcta/efecto |
|---:|---|---|---|
| 7050 | `BLE_ADV_FLOOD_START` | `scope=2, fill_byte, interval_units` (1..102) | Advertising legacy no conectable de 31 bytes |
| 7051 | `BLE_ADV_FLOOD_STOP` | vacío | Detiene/deconfigura flood; no restaura advertising normal |
| 7052 | `BLE_ADV_LAB_START` | Config versión 1 variable | **Solo USB**; inicia custom/raw/rotating advertising |
| 7053 | `BLE_ADV_LAB_STATUS` | vacío | Status autoritativo de 20 bytes |
| 7054 | `BLE_ADV_LAB_STOP` | vacío | Detiene lab y restaura advertising normal si corresponde |

Request 7052:

```text
version=1, profile, mode, name_target,
interval_units:u16be, rotation_ms:u16be, duration_units:u16be,
max_adv_events, adv_length, scan_length, name_count,
advertisement[adv_length], scan_response[scan_length],
repeated name_length, utf8_name[name_length]
```

Status 7052-7054:

```text
version, state, profile, mode, last_reason, active_name_index,
name_count, effective_adv_length, effective_scan_length,
interval_units:u16be, rotation_ms:u16be, duration_units:u16be,
max_adv_events, rotation_count:u32be
```

Límites principales: advertising/scan response máximo 31 bytes, nombres UTF-8
1..26 bytes, hasta 32 nombres, intervalo 20 ms..10.24 s para conectable y al
menos 100 ms para no conectable. Véase [BLE audit tools](ble-audit.md) para
estados, scopes, seguridad y flujo operativo.

## IDs libres y reservados

Los siguientes IDs no tienen handler en esta versión:

```text
1022
1051-1999
2019, 2026-2099, 2102-2199, 2202-2999
3007-3008, 3017, 3021-3029
3032 (nombre reservado pero sin dispatch), 3033-3999
4002-4003, 4045-4999
5014-5999
6015-6999 (incluye 6400)
7007-7009, 7033-7039, 7048-7049, 7055-7999
```

No se debe inferir funcionalidad por el rango. Consulte 1035 y trate cualquier
ID ausente como no soportado.

## Fuentes y mantenimiento

- IDs: `firmware/application/src/data_cmd.h`
- Dispatch y payloads: `firmware/application/src/app_cmd.c`,
  `app_cmd_ble.c`, `app_cmd_keyboard.c`
- Estados: `firmware/application/src/app_status.h`
- Mirror Python: `software/script/chameleon_enum.py`
- Wrappers Python: `software/script/chameleon_cmd.py`
- Mirror Flutter: `../ChameleonUltraGUI/chameleonultragui/lib/helpers/definitions.dart`

Al añadir o retirar un comando, actualice esta referencia, los tres mirrors y
`CHANGELOG.md`. La prueba `software/script/tests/test_command_ids.py` verifica
la igualdad firmware/Python y la composición Ultra/Lite, pero esta documentación
también debe revisarse porque los layouts no pueden deducirse solo del ID.
