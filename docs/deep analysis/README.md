# Deep analysis del firmware

Auditoría estática y reproducible del firmware nRF52840 de ChameleonUltra/Lite.
El alcance principal es `firmware/application`, `firmware/common`,
`firmware/bootloader` y `firmware/tests`. El Nordic SDK se trata como dependencia
vendorizada, no como código mantenido por este proyecto.

## Documentos

- [`00-suspicion-ledger.md`](00-suspicion-ledger.md): registro de cada hallazgo,
  sospecha y descarte.
- [`01-codebase-metrics.md`](01-codebase-metrics.md): líneas, funciones, memoria,
  binario, tests y hotspots.
- [`02-function-signatures.md`](02-function-signatures.md): todas las definiciones
  de funciones C mantenidas, generadas de forma reproducible.
- [`02-excluded-code-summary.md`](02-excluded-code-summary.md): SDK/configuración y
  otros elementos que no deben contaminar las métricas de arquitectura.
- [`03-performance-correctness-report.md`](03-performance-correctness-report.md):
  análisis priorizado de rendimiento, memoria, liveness y correctness.
- [`04-reuse-map.md`](04-reuse-map.md): duplicación útil, límites y abstracciones
  que conviene evitar.
- [`05-optimization-roadmap.md`](05-optimization-roadmap.md): orden canónico para
  corregir y optimizar sin mezclar riesgos.
- [`06-validation-playbook.md`](06-validation-playbook.md): tests host, HIL,
  profiling, memoria, RF, potencia y DFU.
- [`07-security-build-report.md`](07-security-build-report.md): trust root, BLE,
  boot, SWD, release y reproducibilidad.
- [`08-test-coverage.md`](08-test-coverage.md): mapa de cobertura y gaps.
- [`09-implementation-log.md`](09-implementation-log.md): cambios aplicados,
  contratos coordinados y evidencia de validación.

## Interpretación

- **Confirmado**: el mecanismo existe en el código actual.
- **Probable**: el mecanismo existe, pero el fallo o coste depende del scheduling,
  toolchain, hardware o carga.
- **Sospecha**: requiere instrumentación o pruebas físicas para confirmarse.
- **Descartado**: la revisión encontró una protección que invalida la hipótesis.
- **Corregido**: el mecanismo fue reemplazado y tiene una regression automatizada;
  las limitaciones HIL restantes se documentan por separado.
- **Parcial**: una ruta concreta fue corregida, pero el hallazgo conserva rutas
  bloqueadas o no cubiertas que impiden cerrarlo.

P0/P1/P2 indica impacto. El orden de implementación lo define exclusivamente
`05-optimization-roadmap.md`, porque algunas optimizaciones dependen de tests o
migraciones previas.

Las correcciones de integridad/protocolo están registradas en
`09-implementation-log.md`. Los builds diagnósticos y de release usan el GCC
12.2 canónico; hardware Ultra/Lite sigue siendo obligatorio para release.
