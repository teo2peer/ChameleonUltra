# Informe de rendimiento, memoria y correctness

## Resumen ejecutivo

El firmware no debe empezar por micro-optimizaciones. Los mayores riesgos son:

1. Trust/security y comandos BLE sin ACL suficiente.
2. Integridad de datos y owner de buffers HF/FDS.
3. Carreras entre main e ISR/SoftDevice/app_timer.
4. RAM Ultra con solo 3,568 bytes de gap estático.
5. Estados RF/ISO-DEP que reportan éxito sin aceptar la operación.
6. Polling/timers que consumen CPU y energía o bloquean el main loop.

La compilación actual cabe y los tests existentes pasan. Eso no demuestra que el
firmware sea seguro ante interleavings, flash failures, campos simultáneos o
hardware real. P0/P1 indica impacto; el orden ejecutable está en el roadmap.

## Metodología

- Revisión de todo C/H mantenido, linker scripts, Makefiles, bootloader, release
  scripts, CI y wire contracts host.
- Catálogo lexical de 1,422 definiciones.
- Build limpio Ultra/Lite con ARM GCC 12.2.rel1 y tests host Clang ASan/UBSan.
- Revisión independiente por runtime/FDS, RF, transports/security y tests/reuse.
- Sin RF/BLE/USB/FDS/DFU físico ni medición PPK2.
- `Confirmado` prueba el mecanismo estático, no su frecuencia real.

## P0: integridad, memoria y liveness

### P0.1 Guardar owner/tipo de shared tag storage

**Estado:** Corregido en command/button boundary el 2026-07-15. **Impacto original:** hard fault, disclosure y corrupción durable.

Todos los tipos HF reutilizan 4,500 bytes. MF1/MF0 conservan pointers tipados y
los command handlers no verifican el active type. Un comando MF1 contra NTAG,
HF14A-4 o un slot vacío puede dereferenciar `NULL`, interpretar otro layout,
leer bytes stale o persistir corrupción.

**Cambio mínimo:** before-hook central por command family y defensive owner check
en cada protocol API. Invalidar pointers al unload/type switch y limpiar el
buffer cuando el sense queda undefined.

**Validación:** matriz de cada emulator command contra cada tipo/slot vacío; todos
los incompatibles devuelven `STATUS_INVALID_SLOT_TYPE`, sin mutación ni reset.

**Implementado:** owner exacto `{slot,type}` expuesto como predicate read-only y
before-hooks separados para HF genérico, MF1, MF0/NTAG y HF14A-4. Los reader
commands y la telemetría retenida/global no heredan hooks de emulación. La carrera
NFCT durante mutación sigue separada en P0.2/F-015.

### P0.2 Serializar publicación y mutación frente a NFCT

**Estado:** Probable por interleaving estático. **Impacto:** RF frames torn,
pointer/layout mismatch y snapshots inconsistentes.

Main limpia/copia/reemplaza buffers y anticollision/config mientras el NFCT event
handler puede preemptar y leerlos. Detection log clear también puede intercalarse
con una auth de tres frames.

**Cambio mínimo:** quiesce sensing al reemplazar owner; para live updates, preparar
staging completo y swap/publicar generation en critical region o rechazar durante
field/session. Logs usan epoch, no solo count reset.

**Validación:** reader continuo mientras host cambia slots, UID, blocks, pages,
ATS, static responses y clear logs. Solo deben observarse estados old/new completos.

### P0.3 Hacer persistence truthful y retryable

**Estado:** Parcialmente corregido el 2026-07-15. **Impacto original:** pérdida silenciosa de datos.

Tag save actualiza CRC aunque `fds_write_sync` falle. Explicit save responde
success, slot switch carga el siguiente buffer y shutdown sigue aunque save falle.
Delete puede cambiar config aunque flash no se haya borrado.

**Cambio mínimo:** modelo `prepare -> persist -> commit baseline`. El dirty
generation/CRC cambia solo tras success. APIs retornan typed result. Una transición
normal de slot/sleep conserva RAM o aborta si no hay durabilidad; critical-battery
usa save best-effort con deadline y apaga aunque flash siga fallando.

**Validación:** fault injection de busy, no-space, timeout, close, GC y event late
en cada write/delete/switch/shutdown. Segundo save debe reintentar.

**Implementado:** CRC baseline se confirma solo tras `fds_write_sync` exitoso.
Si FDS rechaza una baseline `SHADOW_REQ`, el callback revierte la transición y el
segundo save vuelve a preparar; `SHADOW` solo queda activo tras success. Explicit
save devuelve flash failure. Switch/delete conservan owner/config al fallar.
Shutdown normal se difiere y rearma el timer si el save falla; low-battery sigue
best-effort para no mantener hardware crítico encendido. Quedan atomicidad
config+data y fault injection dentro de `fds_util.c`.

### P0.4 Activar integridad FDS con migración

**Estado:** Confirmado. **Impacto:** bit corruption silenciosa en settings, slots,
PIN y dumps.

`FDS_CRC_CHECK_ON_READ/WRITE` está desactivado, mientras el wrapper contiene una
ruta de migración para records con CRC cero.

**Cambio mínimo:** primero tests golden y migration image; después activar read
CRC y write verification en una release compatible. No borrar todos los records
ante una única corrupción.

**Validación:** imagen legacy zero-CRC, update, power cut y bit flips en header,
data y CRC; preservar último record bueno y records no relacionados.

### P0.5 Eliminar LF allocations mayores que el heap

**Estado:** Probable, condicionado por allocator/linker. **Impacto:** scan failure
o heap/stack collision.

HID/ioProx/PAC solicitan 12,290 bytes de sample storage con heap de 8,192 bytes,
antes de codec allocations. La RAM Ultra tampoco permite simplemente agrandar el
heap.

**Cambio mínimo:** no añadir un array estático de 12,290 bytes: no cabe en los
3,568 bytes libres. Reducir/streaming decode con ring bounded por protocolo o
reemplazar, no sumar, un arena existente después de medir heap/stack. Un único
owner y cero allocation mayor al presupuesto validado.

**Validación:** scans repetidos, alloc failures y todos los exits con stack/heap
watermarks; cero overlap y resources siempre liberados.

### P0.6 Corregir disclosure NTAG mirror y unaligned MF1

**Estado:** Corregido el 2026-07-15; fault alignment original dependía del codegen.

- UID+counter inicializa offsets incorrectos y transmite siete bytes stack sin
  inicializar.
- `&trailer[10]` se convierte a `uint64_t *` sin garantía de alignment.

**Cambio mínimo:** zero/init exacto de mirror (`UID_HEX + x + COUNTER_HEX`) y
`memcpy`/comparación bytewise para Key B.

**Validación:** stack poisoning y golden mirror; disassembly ARM y key check con
`UNALIGN_TRP` habilitado.

**Implementado:** render UID/counter exacto y overlay acotado por página en
`ntag_mirror_internal.h`; Key B se inspecciona y copia sin dereference
desalineado. `test_hf_memory_safety` cubre poison, canaries, offsets mirror 0..3 y
fuentes Key B offsets 0..7 bajo ASan/UBSan. Queda HIL con `UNALIGN_TRP` como
defensa adicional, no como condición de la corrección.

### P0.6b Eliminar stack residue y picos factory

**Estado:** Parcialmente corregido el 2026-07-15. **Impacto original:** disclosure durable y stack pressure.

MF1 crea aproximadamente 4.4 KiB en stack y antes persistía tails no
inicializados. El objeto completo ahora se limpia antes de poblarse; el pico de
stack permanece. HF14A-4 usa alrededor de 3.5 KiB y también lo limpia.

**Cambio mínimo:** zero-init obligatorio. Elegir scratch static/aliased solo tras
map y concurrency ownership; alternativamente construir/escribir por partes sin
mantener el objeto completo en stack. No añadir otro buffer a RAM Ultra.

**Validación:** stack poison, raw FDS inspection y high-water con BLE/interrupts.

### P0.7 Hacer decoder ownership atómico

**Estado:** Probable. **Impacto:** frame perdido, command cero/cross-session o
queue incorrecta al disconnect/reconnect.

BLE RX/reset puede modificar decoder desde SoftDevice event context mientras main
observa `complete` y copia a request queue. Generation reduce riesgo, pero no hace
atómica la transición observe/consume.

**Cambio mínimo:** producer publica un immutable complete frame en queue bajo
critical region, o main snapshot/consume y reset comparten lock/generation.

**Validación:** llenar request slots y alternar fragmented frame,
disconnect/reconnect/reset en cada boundary.

### P0.7b Hacer atómico el cancel de keyboard payload

Button/app_timer no debe escribir directamente un final state que main pueda
sobrescribir. Publicar cancel generation/request y volver a comprobar
inmediatamente antes del primer report HID.

**Estado:** Corregido el 2026-07-15. El botón publica una cancel request que el
main loop no pierde y consume antes de emitir HID. Una llegada tras el último
recheck puede observar RUNNING transitorio, pero la regression comprueba estado
CANCELLED sin reports HID.

### P0.8 Corregir framing/status RF antes de optimizar

**Estado:** Confirmado.

- Rechazar NFCT RX con error status.
- Corregido: parity unwrap exacto ya no inicia un grupo adicional y trailing
  data bits se copian sin parity; ASan/UBSan cubre 7/9/10/17/72/73 bits, buffers
  exactos e in-place.
- Corregir sniff partial length y definir parity-bearing vs stripped.
- Construir ATS scratch con TL igual al tamaño transmitido.
- Retornar invalid/busy/full cuando ISO-DEP response no fue aceptada.

**Validación:** 4/7/8/12/16-bit frames, FSDI 0..8, short/busy/full ISO-DEP,
parity/framing/overflow errors.

### P0.9 Hacer sleep/low-battery revalidable

**Estado:** Probable/confirmado. **Impacto:** apagado en sesión activa o batería
crítica mantenida despierta.

Sleep expiry solo publica un bool y system-off no revalida BLE/USB/HF/LF. Un bool
adicional mezcla HF y LF fields. Low battery llama al idle timer, que se niega a
arrancar si hay actividad.

**Cambio mínimo:** generation token, flags HF/LF separados y recheck antes de
system-off. Forced low-battery usa shutdown bounded: cierra transports/RF, intenta
save con deadline corto y apaga incluso si FDS falla para proteger hardware.

**Validación:** expiry simultáneo con connect, VBUS y ambos fields; zero percent
en advertising/connected/emulating; no WDT loop.

### P0.10 Command contract harness

**Estado:** Gap confirmado. **Impacto:** bloquea refactors seguros.

Las 208 rows Ultra/149 Lite y 185 funciones de `app_cmd.c` no tienen un harness C que
ejecute dispatch, before/after, lengths, status y respuesta por board/transport.

**Cambio mínimo:** stubs tipados, compilar Ultra/Lite y ejecutar para cada row:
NULL, zero, short, exact, long y max payload. Añadir security ACL expectations.

### P0.11 Resolver el presupuesto del bootloader

**Estado:** Recalculado con GCC 12.2 el 2026-07-15. **Impacto:** limita trust
migration, authenticated boot y hardening que aumente flash.

El diagnóstico GCC 8.5 dejaba 44 bytes Ultra y 108 bytes Lite. El build canónico
GCC 12.2 deja 1,620/1,684 bytes: ya no es un overflow inminente, pero sigue siendo
un budget estrecho para cambios criptográficos o de policy.

**Cambio mínimo:** clean canonical builds y per-section/symbol map. Optimizar código
ya incluido, features/config no necesarios o revisar partición antes de añadir
logic. No recortar validation/security para recuperar bytes.

**Validación:** GCC 12.2 Ultra/Lite, artifact/layout checks y full DFU/HIL; mantener
un budget mínimo explícito por release.

## P1: rendimiento, energía y robustez

### P1.1 Reemplazar tick BSP permanente

El timer 10 ms despierta la CPU 100 veces/s y recorre 10 slots. Además exhaustion
retorna OOB. Sustituir por deadlines wrap-safe de RTC/app_timer y programar solo
el expiry más cercano; detener cuando no haya timers.

Medir wake count y PPK2 idle con BLE advertising/connected y USB idle.

### P1.2 Acotar operaciones síncronas

- Raw HF permite timeout aproximado de 65 s.
- LF busy-waits consume main/CPU hasta recibir samples.
- FDS sync puede esperar 15 s y ejecutar GC.
- ISO-DEP emulation puede repetir WTX sin límite.
- RNG startup ocurre antes del WDT y sin deadline.

Crear deadlines monotónicos, cancel checks y budgets específicos. No llamar
callbacks/event APIs que solo se procesan desde el main loop durante un wait que
bloquea ese mismo loop.

### P1.3 Separar field state y live configuration

Unificar la fuente de verdad de reader antenna y derivar any tag field de HF/LF.
Active-slot sense enable/disable debe actualizar hardware inmediatamente. Slot
buttons en reader mode no deben arrancar emulation.

El handler de enable/disable ya actualiza sensing solo para el slot activo, no
cambia de slot al modificar uno inactivo y evita rearmar tag sensing en reader
mode. La fuente de verdad RF duplicada y el ciclo físico en reader mode siguen
pendientes.

### P1.4 Endurecer BLE state machines

- Publish scan STARTING antes del SVC.
- Advertising lab usa generation token para configure/finish.
- Radio off desconecta peripheral y central y espera transition outcome.
- Corregido: USB STOPPED/POWER_REMOVED limpia connected/open y queues.
- Peer delete devuelve accepted/busy/error sin reset.
- HIDS/NUS cuentan notifications si HVN queue deja de ser uno.

### P1.5 Reducir FDS wear y stalls

Comparar nickname/settings/tag payload antes de update. Registrar live/dirty words,
GC count/duration y write amplification. Rate limit no sustituye idempotencia.

### P1.6 Revisar heap, stack y static RAM con datos runtime

Ultra deja 3,568 bytes de gap pero reserva 8 KiB/8 KiB arbitrarios. Añadir stack
canary/high-water, heap peak/failures y top RAM symbols. No reducir stack/heap ni
mover datos a NOINIT hasta observar máximos con BLE+USB+RF+FDS simultáneos.

### P1.7 Corregir wire/setting drift

- Corregido: settings v6 devuelve 14 bytes y firmware, Python y GUI comparten
  golden responses; GUI conserva lectura legacy de 13 bytes.
- Corregido: el sleep timeout guardado gobierna button-wakeup runtime.
- Corregido en el mirror Python: Reader Keys `6` vuelve a ser representable.
- Button values firmware todavía necesitan validación por range y capability Ultra/Lite.
- Persisted records necesitan explicit encoding, no compiler bitfields.
- Darkside no-NAK debe usar payload status, no transport status.

### P1.8 PRNG hard separado de weak/static

Hard mode usa libc `rand()` con seed de 32 bits. Mantener weak/static para research,
pero hard debe usar hardware RNG pool o DRBG con seed suficiente y sin compartir
state con otras features.

### P1.9 Corregir validación semántica RF/LF

- Validar UID size, ATS layout, enums MF1 y static response lengths al cargar FDS.
- Aplicar la polaridad ioProx al raw frame persistido/emulado.
- Rechazar Wiegand/HID fields fuera de rango y format IDs desconocidos.
- Actualizar command status para representar invalid input, no truncar/success.

### P1.10 Rendimiento de dispatch y queues: medir primero

Linear command dispatch sobre unas 208 rows y queues de dos slots son simples y
bounded. No hay evidencia aún de que dominen latencia. Instrumentar p50/p95/p99 y
queue depth bajo USB/BLE 0/64/244/4096 B antes de añadir hash tables o más RAM.

### P1.11 Preservar identidad durante EMV reacquire

Los sweeps de terminal profile vuelven a activar la tarjeta. Cada reacquire ahora
aborta si la activación falla o el UID cambia, evitando traces complete tras
perder el target y APDUs mezclados. El helper puro tiene regression; random UID y
comportamiento real de wallets todavía requieren una matriz HIL explícita.

## P2: tamaño y mantenibilidad

- Mover reader-only translation units fuera del Lite common list; no depender de
  GC-sections como feature gate.
- Partir `app_cmd.c` por dominio manteniendo una tabla/ACL central.
- Internalizar/remover APIs huérfanas solo tras link/call graph de ambos boards.
- Evaluar LTO, logging y crypto backends solo con diff de map, boot/RF tests y
  reproducibilidad. Tamaño flash no justifica aumentar riesgo en RAM/timing.
- Añadir per-symbol size budget; la app Ultra usa ~54.6% de la región flash, pero
  dual-bank no cabe y RAM es más urgente que flash.

## Riesgos al optimizar

- Critical sections largas dañan NFCT/BLE timing.
- Copiar 4.5 KiB para staging puede empeorar la RAM crítica.
- Mover todo a heap aumenta fragmentation/failure nondeterminista.
- Una abstracción genérica de RF puede ocultar timeouts y hardware order.
- Cambiar persisted structs exige migración byte-exacta.
- Cambiar command status/length rompe GUI/CLI si no se versiona sincronizadamente.
