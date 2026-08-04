# Ledger de hallazgos y sospechas

Fecha de captura: 2026-07-15. Las líneas corresponden al árbol de trabajo actual,
que contiene cambios aún no committeados.

## Seguridad, boot y release

| ID | Estado | Prioridad | Hallazgo o sospecha | Evidencia / seguimiento |
|---|---|---|---|---|
| F-001 | Confirmado | P0 | La private key DFU publicada coincide con el trust root activo del bootloader | `resource/dfu_key/chameleon.pem`, `firmware/bootloader/src/dfu_public_key.c:7-10`; migrar trust root |
| F-002 | Confirmado | P0 | NUS y comandos sensibles quedan abiertos por defecto, incluido DFU y storage | `firmware/application/src/settings.c:82-90`, `firmware/application/src/app_cmd.c:3609-3630,3864-3873` |
| F-003 | Confirmado | P0 | PIN por defecto conocido, DISPLAY_ONLY, rotation no efectiva hasta reboot y repairing abierto | `firmware/application/src/ble_main.c:41-55,889-910,1099-1103,1131-1140` |
| F-004 | Confirmado | P0 | SWD/APPROTECT permanece abierto y permite saltar el modelo de trust con acceso físico | `firmware/bootloader/src/sdk_config.h:235-245`, `firmware/bootloader/src/main.c:183-190` |
| F-005 | Corregido | P0 | Cancel físico de keyboard payload podía perder la carrera ARMED -> RUNNING | Cancel request publicado por `keyboard_payload_cancel_from_button()` y revalidado antes del primer HID report; regression en `firmware/tests/test_keyboard_payload.c` |
| F-006 | Confirmado | P0 | Acknowledgement de migración app-only es global, no por device | `flash_firmware.sh:33-35,116-127,233-240` |
| F-007 | Confirmado | P0 | Helper app-only puede reutilizar objetos Lite/Ultra/toolchain/config stale | `flash_firmware.sh:154-162`, `firmware/Makefile.defs:14-29` |
| F-008 | Confirmado | P1 | Signed install no exige authenticated boot de la aplicación instalada | `firmware/bootloader/src/sdk_config.h:59-81`, `firmware/build.sh:129-134` |
| F-009 | Confirmado | P1 | Single-bank DFU permite pérdida de aplicación y no cabe dual-bank con imagen actual | `firmware/bootloader/src/sdk_config.h:1054-1089`; fault injection físico |
| F-010 | Confirmado | P1 | Build/release no es bit-reproducible ni fija toda la supply chain | `firmware/Dockerfile`, `firmware/docker-compose.yml`, `.github/workflows/build_firmware.yml` |
| F-011 | Confirmado | P2 | Scripts DFU no seleccionan un serial único y algunos waits no tienen deadline | `firmware/flash-dfu-full.sh`, `firmware/flash-dfu-app.sh`, `flash_firmware.sh:197-225` |
| F-012 | Confirmado | P2 | `enter_dfu.py` puede ocultar el error original usando `serial_instance` sin asignar | `resource/tools/enter_dfu.py:21-30` |
| F-013 | Sospecha | P1 | Bootloader/product/application versions pueden divergir de la versión anti-rollback | `firmware/Makefile.defs:29-32`, `firmware/build.sh:26-75`, `firmware/bootloader/src/app_config.h` |
| F-097 | Corregido | P0 | El workflow privilegiado podía ejecutar scripts del `head_sha` y evaluar aritmética controlada por artifacts no confiables | Default branch + metadata/path validada + hex exacto; `test_artifact_checker.sh` prueba rechazo sin command-substitution side effect |

## Memoria, persistencia y runtime

| ID | Estado | Prioridad | Hallazgo o sospecha | Evidencia / seguimiento |
|---|---|---|---|---|
| F-014 | Corregido | P0 | Todos los tipos HF comparten buffer y los comandos type-specific no validaban owner activo | Predicate exacto de slot/type cargado, before-hooks por familia y matriz estructural en `test_command_ids.py`; botón Reader Keys también valida owner |
| F-015 | Probable | P0 | Main reemplaza/muta datos HF mientras NFCT ISR puede consumirlos | `firmware/application/src/rfid/nfctag/tag_emulation.c:284-379,686-699`, `firmware/application/src/rfid/nfctag/hf/nfc_14a.c:349-719` |
| F-016 | Corregido | P0 | Fallo FDS o baseline stale podía suprimir retries y explicit save respondía success | CRC commit/validity solo tras load/write exitoso; missing record fuerza primera escritura, rollback/retry `SHADOW_REQ`; regressions production-linked |
| F-017 | Parcial | P0 | Slot switch/delete/type change/shutdown podían descartar estado dirty o resultados de persistencia | Switch/delete y shutdown normal abortan/difieren transición ante fallo; low-battery sigue best-effort por seguridad física, y type change/atomicidad multi-record requieren fault injection/HIL |
| F-018 | Confirmado | P0 | FDS CRC read/write está desactivado aunque el recovery code espera CRC | `firmware/application/src/sdk_config.h:6824-6845`, `firmware/application/src/utils/fds_util.c:257-279` |
| F-019 | Probable | P0 | Decoder BLE puede ser reseteado desde IRQ mientras main snapshot/queuea frame completo | `firmware/application/src/utils/dataframe.c:176-235,270-304` |
| F-020 | Probable | P0 | Readers LF solicitan buffers de 12,290 bytes con heap configurado en 8,192 | `firmware/application/src/rfid/reader/lf/lf_hidprox_data.c:20,55-57`, `firmware/application/src/rfid/reader/lf/lf_ioprox_data.c:18,97-100`, `firmware/application/src/rfid/reader/lf/lf_pac_data.c:18,61` |
| F-021 | Corregido | P0 | NTAG UID+counter mirror dejaba bytes stack sin inicializar y pisaba parte del UID | Render exacto y overlay acotado en `ntag_mirror_internal.h`; golden/canary tests para offsets 0..3 en `firmware/tests/test_hf_memory_safety.c` |
| F-022 | Corregido | P0 | Cast desalineado a `uint64_t *` en trailer MF1 podía hard-fault en Cortex-M4 | Copia bytewise en `mf1_key_access_internal.h`; fuentes offsets 0..7 y canaries bajo ASan/UBSan en `firmware/tests/test_hf_memory_safety.c` |
| F-023 | Probable | P0 | Sleep expiry puede apagar tras aparecer conexión o campo nuevo | `firmware/application/src/utils/syssleep.c:22-75`, `firmware/application/src/app_main.c:302-307` |
| F-024 | Confirmado | P0 | Low-battery usa idle sleep bloqueable por BLE/RF; display de 0% puede loop hasta WDT | `firmware/application/src/ble_main.c:1285-1292`, `firmware/application/src/app_main.c:612-626` |
| F-025 | Confirmado | P1 | CRC16 es la autoridad única de dirty state para buffers HF de más de 4 KiB | `firmware/application/src/rfid/nfctag/tag_emulation.c:349-366` |
| F-026 | Confirmado | P1 | Timer pool devuelve pointer fuera del array cuando se agotan 10 slots | `firmware/application/src/bsp/bsp_time.c:28-38` |
| F-027 | Sospecha | P1 | Evento FDS realmente perdido deja `busy` permanente tras timeout | `firmware/application/src/utils/fds_util.c:98-110,134-165,407-429` |
| F-028 | Confirmado | P1 | Timer repetido de 10 ms despierta CPU 100 veces/s aunque no haya deadlines | `firmware/application/src/bsp/bsp_time.c:5-20,62-94` |
| F-029 | Probable | P1 | Ambos botones comparten debounce/timestamp/result y flags ISR sin publicación formal | `firmware/application/src/app_main.c:54-65,191-271,982-1013` |
| F-030 | Confirmado | P1 | Updates idénticos de nickname fuerzan FDS writes/GC y desgaste | `firmware/application/src/app_cmd.c:1736-1758`, `firmware/application/src/utils/fds_util.c:183-223` |
| F-031 | Confirmado | P1 | RAM Ultra deja solo 3,568 bytes entre heap reservado y stack reservado | Build canónico GCC 12.2; `firmware/application/application.ld:8-16`, `firmware/application/Makefile:475-478` |
| F-032 | Sospecha | P1 | RAM origin SoftDevice sigue documentado como estimación manual | `firmware/application/application.ld:10-16`, `firmware/application/src/ble_main.c:831-845` |
| F-033 | Confirmado | P2 | Reset classification borra NOINIT por addresses hardcoded y elimina evidencia en WDT/software reset | `firmware/application/src/app_main.c:480-552`, `firmware/common/noinit.ld:6-18` |
| F-034 | Corregido | P2 | Clone nickname persistía hasta 29 bytes stack sin inicializar | Ambos buffers nickname se inicializan a cero antes de escribir el record completo |
| F-035 | Confirmado | P2 | System-off GPIO duplica MOSI y omite SPI SCK | `firmware/application/src/app_main.c:374-387` |
| F-036 | Confirmado | P2 | Persistencia usa compiler bitfields/enum layout como formato on-flash | `firmware/application/src/settings.h:47-75`, `firmware/application/src/rfid/nfctag/tag_emulation.h:49-76` |
| F-037 | Sospecha | P1 | RNG startup puede esperar para siempre antes de iniciar watchdog | `firmware/application/src/app_main.c:124-141,1103-1110` |
| F-038 | Probable | P1 | Alloc/free repetido de codecs LF puede fragmentar un heap ya insuficiente | Protocol allocators y `firmware/application/src/rfid/nfctag/lf/utils/circular_buffer.c:24-43`; medir high-water/fragmentation |
| F-039 | Parcial | P0 | Factory MF1 usa ~4.4 KiB stack y HF14A-4 ~3.5 KiB | MF1 ahora zero-inicializa el objeto completo antes de persistir; los picos de stack siguen pendientes de high-water/refactor sin añadir RAM estática |
| F-098 | Corregido | P0 | Factory default aceptaba un `tag_type` distinto al configurado y podía sobrescribir el dump HF/LF del slot | Handler/API exigen tipo exacto; mismatch devuelve `INVALID_SLOT_TYPE` y callback FDS fallido `FLASH_WRITE_FAIL`; regression production-linked |
| F-099 | Corregido | P1 | Deshabilitar un slot inactivo podía buscar/cambiar el active slot usando el índice del payload | La transición automática solo se evalúa si el slot modificado es el activo |
| F-100 | Corregido | P1 | Getters LF validaban configuración pero podían leer el buffer compartido con otro owner o tras load fallido | Los siete getters LF exigen owner activo exacto antes de serializar el buffer |
| F-101 | Corregido | P1 | Una migración legacy validaba tamaño/active slot antes de convertir, pero no los tipos resultantes | La configuración migrada se revalida; una conversión inválida restaura defaults y persiste/reintenta esa recuperación |
| F-102 | Corregido | P1 | Clone UID físico podía interpretar buffers HF/LF stale si el tipo estaba configurado pero no cargado | Las rutas clone HF/LF exigen el owner exacto cargado antes de leer o mutar el buffer compartido |
| F-103 | Corregido | P2 | Delete nickname trataba record inexistente como fallo de flash | El resultado se decide por `fds_util_last_error()`; cero records con `NRF_SUCCESS` es éxito idempotente |
| F-110 | Corregido | P1 | Clone LF podía mutar tipo tras scan fallido, reportar éxito tras reload fallido o saltar write por CRC stale | Pre-save, cambio solo tras scan OK, snapshot/rollback exacto y `crc_valid` explícito fuerzan escritura cuando no existe baseline |

## RF, protocolo y estados

| ID | Estado | Prioridad | Hallazgo o sospecha | Evidencia / seguimiento |
|---|---|---|---|---|
| F-040 | Confirmado | P0 | NFCT procesa RX_FRAMEEND aun con `rx_status` de error | `firmware/application/src/rfid/nfctag/hf/nfc_14a.c:731-767` |
| F-041 | Confirmado | P0 | Sniffer RX contradice contrato de parity removal y TX calcula mal partial-bit length | `firmware/application/src/rfid/nfctag/hf/nfc_14a.c:349-379,708-720` |
| F-042 | Confirmado | P0 | ATS truncado conserva TL original y produce ATS inválido para FSD pequeño | `firmware/application/src/rfid/nfctag/hf/nfc_14a.c:571-590` |
| F-043 | Confirmado | P0 | Comando ISO-DEP responde success aunque response sea short/busy o static table esté llena | `firmware/application/src/rfid/nfctag/hf/nfc_14a_4.c:115-147,440-464`, `firmware/application/src/app_cmd.c:2525-2586` |
| F-044 | Confirmado | P1 | ISO-DEP emulation puede encadenar WTX sin count/deadline | `firmware/application/src/rfid/nfctag/hf/nfc_14a_4.c:266-273,297-310,409-428` |
| F-045 | Confirmado | P1 | Persisted protocol objects validan size, no UID/ATS/enums/resp lengths semánticos | `firmware/application/src/rfid/nfctag/hf/nfc_mf1.c:1292-1320`, `firmware/application/src/rfid/nfctag/hf/nfc_mf0_ntag.c:1226-1251`, `firmware/application/src/rfid/nfctag/hf/nfc_14a_4.c:545-581` |
| F-046 | Confirmado | P1 | Un bool representa campos HF y LF simultáneos; retirar uno puede permitir sleep | `firmware/application/src/rfid/nfctag/tag_emulation.c:30-31`, `firmware/application/src/rfid/nfctag/hf/nfc_14a.c:665-705`, `firmware/application/src/rfid/nfctag/lf/lf_tag_em.c:49-111` |
| F-047 | Confirmado | P1 | Dos fuentes de verdad controlan reader field y sleep puede ver estado incorrecto | `firmware/application/src/app_main.c:70,913-964`, `firmware/application/src/rfid_main.c:12,51-55`, `firmware/application/src/rfid/reader/hf/rc522.c:1464-1476` |
| F-048 | Confirmado | P1 | Darkside no-NAK devuelve enum en status transport equivocado | `firmware/application/src/rfid/reader/hf/mf1_toolbox.c:525-539`, `firmware/application/src/app_cmd.c:342-358` |
| F-049 | Confirmado | P1 | ioProx inverted capture normaliza fields pero guarda raw con polaridad incorrecta | `firmware/application/src/rfid/nfctag/lf/protocols/ioprox.c:115-153,233-280,363-414` |
| F-050 | Confirmado | P1 | HID/Wiegand oversized/unknown se trunca o genera frame cero y reporta éxito | `firmware/application/src/rfid/nfctag/lf/protocols/wiegand.c:91-103,828-872`, `firmware/application/src/rfid/nfctag/lf/protocols/hidprox.c:161-201`, `firmware/application/src/rfid/nfctag/lf/lf_tag_em.c:304-310` |
| F-051 | Confirmado | P1 | Clear de MF1 log entre frames puede completar record híbrido | `firmware/application/src/rfid/nfctag/hf/nfc_mf1.c:422-475,1412-1416` |
| F-052 | Corregido | P1 | `GET_DEVICE_SETTINGS` v6 devolvía 13 bytes mientras el cliente esperaba 14 | Payload canónico de 14 bytes en `device_settings_payload_internal.h`; tests C/Python/GUI y referencia wire sincronizados |
| F-053 | Corregido | P1 | Sleep timeout se persistía pero runtime usaba 8,000 ms fijo | `app_main.c` usa `settings_get_sleep_timeout()`; timeout se expone en segundos como byte 13 del payload v6 |
| F-054 | Corregido | P1 | Toggle de sense en active slot no actualizaba inmediatamente emulación viva | El handler actualiza solo el sense activo afectado, evita cambios por slots inactivos y no rearma tag sensing en reader mode |
| F-055 | Confirmado | P1 | Cycle slot por botones en reader mode puede reactivar tag sensing | `firmware/application/src/app_main.c:191-198,590-609`, `firmware/application/src/rfid_main.c:17-36` |
| F-056 | Confirmado | P1 | PRNG MF1 hard usa `rand()` con seed hardware de solo 32 bits | `firmware/application/src/app_main.c:124-149`, `firmware/application/src/rfid/nfctag/hf/nfc_mf1.c:390-410` |
| F-057 | Confirmado | P2 | Button config acepta enums fuera de rango/no soportados por Lite | `firmware/application/src/app_cmd.c:230-251`, `firmware/application/src/settings.c:42-48,75-79` |
| F-058 | Confirmado | P1 | Long polling HF/LF monopoliza main; raw timeout host permite ~65 s | `firmware/application/src/rfid/reader/hf/rc522.c:397-404,1637-1721`, `firmware/application/src/rfid/reader/lf/lf_reader_generic.c:44-69` |
| F-059 | Sospecha | P1 | NFCT frame-delay timeout puede no rearmar RX en todos los estados driver | `firmware/application/src/rfid/nfctag/hf/nfc_14a.c:751-767`; hardware |
| F-060 | Sospecha | P2 | LF sensing mantiene HFXO y puede elevar idle current | `firmware/application/src/rfid/nfctag/lf/lf_tag_em.c:175-217`; PPK2 |
| F-061 | Sospecha | P1 | Gap de ~2 ms entre bursts LF puede romper readers estrictos | `firmware/application/src/rfid/nfctag/lf/lf_tag_em.c:125-150`; matrix de readers |
| F-062 | Sospecha | P1 | T55xx password write deja margen aproximado de 1.5 ms en timeslot | `firmware/application/src/rfid/reader/lf/lf_t55xx_data.c:49-81,111`; medir |
| F-063 | Sospecha | P1 | Thresholds/Goertzel LF fijos requieren voltage/temp/coupling matrix | LF readers y demodulators; HIL analógico |
| F-106 | Corregido | P0 | Parity unwrap podía leer más allá en múltiplos exactos y declarar trailing bits sin decodificarlos | Termina grupos exactos y copia 1..8 bits parciales; ASan/UBSan cubre 7/9/10/17/72/73 bits e in-place |
| F-107 | Corregido | P1 | EMV profile reacquire podía continuar con otra tarjeta o marcar complete tras perder la original | Reacquire fallido o UID distinto retorna failure, conserva status RF/ATS cuando aplica y finaliza aborted |

## BLE, USB y command plane

| ID | Estado | Prioridad | Hallazgo o sospecha | Evidencia / seguimiento |
|---|---|---|---|---|
| F-064 | Confirmado | P1 | Radio off deja conexión peripheral normal activa y reporta off | `firmware/application/src/ble_main.c:1483-1542` |
| F-065 | Confirmado | P1 | Advertising rotation puede reconfigurar normal advertising tras termination/connect race | `firmware/application/src/ble_main.c:669-697,1712-1737,1876-1915` |
| F-066 | Confirmado | P1 | Scan report puede llegar antes de publicar active y dejar scan lógicamente activo pero parado | `firmware/application/src/ble_scan.c:87-102,197-208` |
| F-067 | Corregido | P1 | USB POWER_REMOVED/STOPPED no limpiaba `g_usb_port_opened` | Ambos eventos invalidan ahora el estado open además de resetear transporte/colas |
| F-068 | Probable | P1 | Bond deletion convierte busy/FDS transitorio en APP_ERROR reset y command responde success temprano | `firmware/application/src/ble_main.c:1048-1057,1156-1165`, `firmware/application/src/app_cmd.c:1988-1992` |
| F-069 | Confirmado | P2 | Subscribe ATT error se descarta y global probe RSSI se inventa como cero | `firmware/application/src/ble_central.c:580-587,829-840,1529-1558` |
| F-070 | Sospecha | P2 | HID/NUS TX completion depende de HVN queue implícitamente igual a uno | `firmware/application/src/keyboard_hid.c:465-468`, `firmware/application/src/ble_main.c:408-413,467-473` |
| F-071 | Confirmado | P2 | Command dispatch busca linealmente en tabla completa | `firmware/application/src/app_cmd.c:3598-3908`; medir antes de cambiar |
| F-072 | Sospecha | P2 | Request/TX queues de dos entradas pueden limitar throughput bajo BLE/USB burst | `firmware/application/src/utils/dataframe.c:14-48`, transport queues; benchmark |

## Reutilización, build y cobertura

| ID | Estado | Prioridad | Hallazgo o sospecha | Evidencia / seguimiento |
|---|---|---|---|---|
| F-073 | Confirmado | P1 | RC522 mantiene dos transfer engines casi iguales y ya difieren en trace/WDT | `firmware/application/src/rfid/reader/hf/rc522.c:355-498,624-758` |
| F-074 | Confirmado | P1 | Cuatro parsers TLV/DOL y macros EMV con scope accidental compartido con DESFire | `firmware/application/src/app_cmd.c:2782-3562`, `firmware/application/src/rfid/reader/hf/emv_trace.c:269-879` |
| F-075 | Confirmado | P1 | LF readers repiten alloc/acquire/decode/timeout/cleanup | `firmware/application/src/rfid/reader/lf/lf_*_data.c` |
| F-076 | Confirmado | P1 | Metadata de tag/protocol está repartida entre varias autoridades manuales | `firmware/application/src/rfid/nfctag/tag_base_type.h:100-127`, `firmware/application/src/rfid/nfctag/tag_emulation.c:33-46,121-228`, `firmware/application/src/rfid/nfctag/lf/lf_tag_em.c:258-350` |
| F-077 | Confirmado | P1 | MIFARE auth está duplicado y auth trace sintetiza REQA distinto del WUPA real | `firmware/application/src/rfid/reader/hf/mf1_toolbox.c:97-244`, `firmware/application/src/app_cmd.c:2200-2354`, `firmware/application/src/rfid/reader/hf/rc522.c:46-53` |
| F-078 | Confirmado | P2 | Wire get/put endian/bounds se repite en app_cmd, BLE, keyboard y EMV | `firmware/application/src/app_cmd.h:18-49`, `firmware/application/src/app_cmd_keyboard.c:14-24`, `firmware/application/src/app_cmd_ble.c` |
| F-079 | Confirmado | P2 | CRC32 IEEE está duplicado | `firmware/application/src/keyboard_payload.c:64-72`, `firmware/application/src/rfid/reader/hf/emv_trace.c:881-889` |
| F-080 | Confirmado | P2 | ISO-DEP activation y tag serializer se repiten varias veces | `firmware/application/src/rfid/reader/hf/iso_dep_session.c:34-65`, `firmware/application/src/app_cmd.c:2625-2675`, `firmware/application/src/rfid/reader/hf/emv_trace.c:420-449` |
| F-081 | Confirmado | P1 | `app_cmd.c` tiene 4,002 líneas y 185 funciones | Inventario lexical; separar handlers sin mover dispatch ownership |
| F-082 | Probable | P2 | Varias APIs/archivos parecen dead o deberían ser `static` | `firmware/application/src/rfid/reader/lf/lf_gap.*`, APIs timeslot/BSP/central/RC522 listadas en `04-reuse-map.md` |
| F-083 | Confirmado | P2 | Reader-only ISO-DEP/EMV sources están en common list y Lite depende de GC-sections | `firmware/application/Makefile:54-61,356-390` |
| F-084 | Confirmado | P2 | Application Makefile incluye `Makefile.defs` dos veces | `firmware/application/Makefile:1,500` |
| F-085 | Confirmado | P1 | Solo 13 de ~66 translation units productivas entran en tests host directos | `firmware/tests/Makefile` |
| F-086 | Confirmado | P0 | No hay C harness para 208/149 dispatch rows, handlers, lengths/status/hooks Ultra/Lite | `firmware/tests/Makefile`; prioridad malformed-input |
| F-087 | Confirmado | P1 | No hay tests C para BLE main/scan/central, USB o HID event ordering real | `firmware/tests/Makefile:29-36` |
| F-088 | Parcial | P0 | Falta fault injection del backend FDS/settings y de migraciones/power-cut | `test_tag_persistence.c` inyecta fallos en la frontera FDS de `tag_emulation.c`; `utils/fds_util.c` todavía necesita fake backend |
| F-089 | Confirmado | P1 | EMV trace C, NFC emulator y la mayoría de LF codecs son compile/hardware-only | `firmware/tests/Makefile` |
| F-090 | Confirmado | P1 | CI no publica coverage, fuzz, static analyzer ni stack-usage | `.github/workflows/native-firmware-tests.yml`, build workflows |
| F-091 | Confirmado | P1 | No existe HIL automatizado Ultra/Lite para RF/BLE/USB/FDS/DFU/power | Hardware tests excluidos en `.github/scripts/run_hardware_free_python_tests.py:13-16` |
| F-092 | Confirmado | P2 | Host Makefile no genera header deps y puede ejecutar binaries stale | `firmware/tests/Makefile` |
| F-093 | Descartado | P2 | `make clean test` podía borrar y luego ejecutar binaries sin rebuild | La invocación actual reconstruyó y ejecutó los 10 binaries desde un directorio eliminado; se conservan comandos separados en CI por claridad, no por un fallo reproducido |
| F-094 | Confirmado | P1 | Artifact check genérico no separa regular RAM, NOINIT, heap y stack | `.github/scripts/check_firmware_artifacts.sh:68-93` |
| F-095 | Confirmado | P2 | EM4X05 scan se anuncia como command reconocido pero retorna NOT_IMPLEMENTED | `firmware/application/src/app_cmd.c`, `firmware/application/src/data_cmd.h`; capability != feature |
| F-096 | Confirmado (GCC 8.5) | P1 | Bootloader tiene un budget estrecho y el diagnóstico GCC 8.5 exageraba el riesgo con solo 44/108 B | Build canónico GCC 12.2: 43,436/43,372 B, headroom 1,620/1,684 B; mantener budget antes de trust migration |
| F-108 | Corregido | P1 | El enum Python de funciones de botón no podía representar Reader Keys `6` | `ButtonPressFunction.READERKEYS=6`, descripción CLI y regression en `test_device_settings.py` |
| F-109 | Confirmado | P1 | Los checks Python obligatorios del proyecto están rojos en el árbol actual | `ruff check .`: 70 errores; `pyrefly check`: 420 tras excluir `.venv` y `venv`; no son gates verdes de release |

## Hipótesis descartadas

| ID | Estado | Hipótesis descartada | Protección observada |
|---|---|---|---|
| D-001 | Descartado | Payload dataframe puede exceder 4,096 | Length/LRC se valida antes de completar: `firmware/application/src/utils/dataframe.c:112-148,311-332` |
| D-002 | Descartado | Handler devuelve pointer stack usado después del retorno | `data_frame_make` copia y cada transport vuelve a copiar |
| D-003 | Descartado | Request queue sobrescribe slot ocupado normalmente | Reservation usa critical section y backpressure; F-019 cubre reset race separado |
| D-004 | Descartado | FDS retiene pointer caller después del return | Wrapper copia a static protected write buffer |
| D-005 | Descartado | FDS read copia record mayor al destination | Word/byte capacity checks en `firmware/application/src/utils/fds_util.c:238-301` |
| D-006 | Descartado | Current flash/FDS/bootloader/NOINIT regions se solapan | Linker ranges y release validator encajan exactamente |
| D-007 | Descartado | App DFU acepta paquetes unsigned porque app signature check es cero | Update init packet signature es independiente; gap real es authenticated boot F-008 |
| D-008 | Descartado | Central BLE clobberiza NUS peripheral state | Role y connection-handle guards separan events |
| D-009 | Descartado | Scan snapshot se copia mientras ISR lo muta | Snapshot copy usa critical region; startup race F-066 es distinto |
| D-010 | Descartado | LF codecs auditados filtran memoria en exits normales | Paths normales/fallo liberan codecs; heap sizing/fragmentation siguen en F-020/F-038 |
| D-011 | Descartado | ISO-DEP reader block numbering asimétrico es bug | Tests existentes codifican `A2/A3` intencionalmente |
| D-012 | Descartado | In-place NFC parity unwrap pisa input no leído | Revisión de offsets no encontró overwrite |
