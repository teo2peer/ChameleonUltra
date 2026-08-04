# Métricas estáticas y de build

Fecha: 2026-07-15. Scope mantenido: application, common, bootloader y tests.
`firmware/nrf52_sdk` se mide aparte como dependencia vendorizada.

## Código mantenido

| Partición | Archivos C/H | Líneas físicas |
|---|---:|---:|
| Application | 138 | 47,171 |
| Common | 6 | 484 |
| Bootloader | 5 | 6,350 |
| Tests/stubs | 33 | 2,115 |
| **Total** | **182** | **56,120** |

Los dos `sdk_config.h` generados/configurados suman 18,189 líneas. Sin ellos, el
scope mantenido queda en 37,931 líneas. Hay 81 `.c` con 33,623 líneas y 101 `.h`
con 22,497 líneas.

## Funciones

El generador lexical reproducible encuentra 1,422 definiciones en 86 archivos:

| Grupo | Funciones |
|---|---:|
| Application | 1,272 |
| Common | 17 |
| Bootloader | 7 |
| Tests/stubs | 126 |
| Static | 649 |
| Exportadas | 647 |
| Test/stub | 126 |

Archivos con más definiciones:

| Archivo | Funciones |
|---|---:|
| `firmware/application/src/app_cmd.c` | 185 |
| `firmware/application/src/ble_main.c` | 68 |
| `firmware/application/src/rfid/nfctag/lf/protocols/wiegand.c` | 68 |
| `firmware/application/src/ble_central.c` | 64 |
| `firmware/application/src/rfid/reader/hf/rc522.c` | 51 |
| `firmware/application/src/rfid/nfctag/hf/nfc_mf1.c` | 49 |
| `firmware/application/src/rfid/nfctag/hf/nfc_mf0_ntag.c` | 45 |
| `firmware/application/src/app_cmd_ble.c` | 45 |
| `firmware/application/src/rfid/reader/hf/emv_trace.c` | 39 |
| `firmware/application/src/rfid/nfctag/tag_emulation.c` | 39 |

## Build actual de diagnóstico

Compilación limpia del árbol dirty actual con el container canónico ARM GCC
12.2.rel1, `-O3` y output aislado por board. Es una baseline diagnóstica con el
toolchain de release, pero no provenance release porque el source snapshot no
está committeado y el container tag externo no está fijado por digest.

| Métrica | Ultra | Lite |
|---|---:|---:|
| `.text` map | 355,612 B (`0x56d1c`) | 287,620 B (`0x46384`) |
| `.data` map | 1,280 B (`0x500`) | 1,168 B (`0x490`) |
| `.text` + `.data` map | 356,892 B | 288,788 B |
| `arm-none-eabi-size` text+data | 357,712 B | 289,520 B |
| `.bss` regular | 193,040 B | 135,900 B |
| Heap reservado | 8,192 B | 8,192 B |
| Stack reservado | 8,192 B | 8,192 B |
| Gap heap-limit -> stack-limit | **3,568 B** | **60,816 B** |
| `.noinit` | 18,144 / 32,768 B | 18,144 / 32,768 B |

Nota: `.text` no incluye `.data` load image. El tamaño binario real observado por
`arm-none-eabi-size` fue 357,712 B Ultra y 289,520 B Lite; es la suma relevante para la imagen.
SHA-256 application: Ultra
`79afd73b4c13a07c10fa3d63f8516fbf885ce48c2852e90f3eae04a1bd82e49a`;
Lite `737deb17db497909b2be58057d36ec656150b0898a7ab5021868e0161f55b666`.
Maps/ELFs se retuvieron solo en output temporal de verificación. Las cifras no
son una baseline reproducible hasta fijar source snapshot e image digest.

Regiones:

- Application flash: 640 KiB, `0x27000..0xC7000`.
- FDS: 176 KiB, `0xC7000..0xF3000`.
- Bootloader: 44 KiB, `0xF3000..0xFE000`.
- Regular RAM: 214,296 B desde `0x20003ae8` hasta `0x20038000`.
- Retained NOINIT: 32 KiB, `0x20038000..0x20040000`.

La RAM Ultra es el presupuesto crítico: static regular + heap + stack deja solo
3,568 B antes del límite. No existe todavía high-water runtime que demuestre si
8 KiB de heap y stack son correctos.

## Bootloader de diagnóstico

Build limpio actual con ARM GCC 12.2.rel1:

| Métrica | Ultra | Lite |
|---|---:|---:|
| `arm-none-eabi-size` text | 43,256 B | 43,192 B |
| data | 180 B | 180 B |
| Flash image sections | **43,436 B** | **43,372 B** |
| Región bootloader | 45,056 B | 45,056 B |
| Headroom | **1,620 B** | **1,684 B** |
| BSS | 29,956 B | 29,924 B |
| Stack reservado | 16,384 B | 16,384 B |

El `.bin` raw no representa tamaño por sus address holes/UICR; usar secciones
ELF/map. GCC 12.2 recupera aproximadamente 1.5 KiB frente al diagnóstico GCC 8.5,
pero el margen sigue siendo estrecho para trust-root migration y authenticated
boot. Mantener budget y medir cada cambio; repartition puede seguir siendo
necesario.

## Indicadores

- 209 command IDs numéricos asignados; 208 dispatch rows Ultra y 149 Lite. ID
  3032 está reservado/definido pero no dispatched.
- Request queue global: 2 frames; TX queue USB: 2; TX queue BLE: 2.
- Max protocol payload: 4,096 bytes.
- FDS: 22 páginas virtuales de 8 KiB; CRC read/write desactivado.
- Shared HF storage: 4,500 bytes; shared LF storage: 20 bytes.
- MF1 retained auth log: hasta 1,000 records de 18 bytes.
- Heap API aparece en 55 call sites de alloc/free productivos.
- Timer BSP: 10 slots, tick fijo de 10 ms.
- SoftDevice: 1 peripheral + 1 central, MTU 247.
- WDT efectivo: aproximadamente 5 s.

## Tests actuales

- 10 binaries C host con Clang, ASan y UBSan: todos pasaron.
- 13 translation units productivas enlazadas directamente en tests, frente a
  unas 66 productivas: aproximadamente 20% de reach por archivo, no coverage.
- Hardware-free Python: framing, IDs/status, BLE host, keyboard, EMV, ISO-DEP,
  Crypto1, Reader Keys y Jablotron pasaron.
- Static Python: `ruff` permanece rojo con 70 errores y `pyrefly` con 420; estos
  checks declarados no pueden presentarse como gates verdes.
- Layout validator Ultra y Lite: pasó.
- Builds application Ultra y Lite actuales: pasaron con GCC 12.2.rel1.
- Builds bootloader Ultra y Lite actuales: pasaron con GCC 12.2.rel1, con
  headroom de 1,620/1,684 bytes.
- Artifact/layout validator Ultra y Lite: pasó sobre maps y size reports.
- No se ejecutó HIL físico en esta auditoría.

## Métricas faltantes

- Line/branch/function coverage.
- Stack watermark e interrupt nesting peak.
- Heap high-water, fragmentation y allocation failures.
- FDS live/dirty words, write amplification y GC latency.
- Boot time, command p50/p95/p99 y transport throughput.
- ISO-DEP WTX/APDU timing, RF error rate y LF decode CPU.
- PPK2 current/energy por system-off, BLE, USB, HF, LF, FDS y LED.
