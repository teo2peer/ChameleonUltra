# Código excluido o separado

## Nordic SDK

`firmware/nrf52_sdk` contiene 1,308 archivos C/H/S y aproximadamente 620,070
líneas:

| Tipo | Archivos |
|---|---:|
| `.c` | 393 |
| `.h` | 903 |
| `.S` | 12 |

Es una dependencia vendorizada. No se incluye en el índice de firmas ni en las
métricas de reutilización. Sí se revisaron las APIs/configuraciones que afectan al
firmware: dispatch SoftDevice, FDS, boot validation, queue sizes, app_timer,
USB event queue y BLE security.

## Configuración generada

Los dos `sdk_config.h` suman 18,189 líneas y forman parte del scope C/H porque son
la configuración efectiva. Se separan de la complejidad arquitectónica: gran
parte es boilerplate Nordic, pero sus valores activos sí se auditan.

## Host tooling

`software/script` y `software/src` no forman parte del catálogo de funciones del
firmware. Se consultan para verificar wire contracts, command mirrors y tests.
La auditoría de rendimiento de la GUI vive en el repositorio separado.

## Artefactos

Se excluyen `firmware/objects`, `_objects_*`, `firmware/tests/build*`, ZIP/HEX/BIN,
core dumps y outputs temporales. Las métricas de imagen proceden de builds limpios
temporales y deben regenerarse con el toolchain release.

## Política

- No editar Nordic SDK para resolver un problema del wrapper si existe una API
  pública o una configuración equivalente.
- Pin y verificar el SDK/toolchain como supply-chain input.
- Reaplicar esta separación al medir coverage, duplication y static analysis.
- Tratar cambios en `sdk_config.h`, linker scripts y boot settings como cambios de
  arquitectura aunque no añadan funciones.
