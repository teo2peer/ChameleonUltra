# Seguridad, boot y release

## Threat model mínimo

- Peer BLE cercano sin bond.
- Peer bonded pero sin MITM/LESC.
- Host USB autorizado/no autorizado.
- Acceso físico con SWD.
- Source/release pipeline comprometido.
- Power loss durante FDS/DFU.
- Firmware instalado modificado después de validación.

La LRC del command frame detecta errores accidentales; no autentica comandos.

## Critical: trust root DFU comprometido

La private key versionada en `resource/dfu_key/chameleon.pem` corresponde al
public point compilado en `firmware/bootloader/src/dfu_public_key.c`. Cualquier
persona con el historial del repositorio puede firmar una imagen con versión alta
aceptada por bootloaders existentes. Anti-rollback no revoca una key conocida.

### Migración segura

1. Generar nueva P-256 key offline y protegerla fuera del checkout/CI general.
2. Crear bootloader de migración con new public key, firmado por old key.
3. Antes de añadir logic, presupuestar capacidad: GCC 12.2 deja 1,620 B Ultra y
   1,684 B Lite; optimizar/reparticionar sin quitar validaciones.
4. Validar Ultra/Lite, interrupted update y recovery.
5. Desplegar full migration a cada device y registrar serial/chip ID/version.
6. Confirmar que new bootloader rechaza old key y acepta new key.
7. Cambiar release validator/CI al new root y retirar old private material del
   árbol actual. El historial sigue comprometido y debe tratarse como revocado.
8. Bloquear app-only DFU por query real de bootloader/layout, no marker global.

No generar ni almacenar la nueva private key como parte de esta auditoría.

## Command ACL BLE

Pairing está off por defecto y NUS queda `SEC_OPEN`. La autorización global solo
se activa si el setting de pairing está on. Esto deja disponibles settings,
emulator data, FDS wipe, RF/BLE active operations y bootloader entry.

### Policy propuesta

| Clase | USB | BLE open | BLE encrypted | BLE MITM+LESC |
|---|---:|---:|---:|---:|
| Version/capabilities/battery | Sí | Sí | Sí | Sí |
| Read public status | Sí | Según threat model | Sí | Sí |
| Tag/settings mutation | Solo host confiable + policy local | No | No | Sí |
| Pairing secret/read/rotate | Sí + confirm | No | No | No/confirm |
| FDS wipe/reset/DFU | Sí + confirm | No | No | Sí + physical confirm |
| RF/BLE transmit/stress | Host confiable + explicit scope | No | No | Sí + explicit scope |
| Keyboard payload | Host confiable + device unlocked/confirm | No | No | Sí + explicit arm |

La tabla final requiere decisión de producto, pero debe existir como ACL central
y testearse por cada dispatch row. USB no autentica criptográficamente al host:
un deployment unattended debe tratar USB físico como trust boundary o exigir
confirmación local/device unlock para mutation, RF transmit, wipe y DFU.

## Pairing e identidad

Problemas actuales:

- PIN conocido `123456`.
- IO capability DISPLAY_ONLY no es Numeric Comparison.
- Rotation actualiza settings pero SoftDevice option solo se aplica al boot.
- Whitelist no controla admission general.
- Repairing siempre permitido.
- General command gate comprueba encryption, no MITM+LESC.

Corrección: secret único por device provisionado por USB, rotation inmediata o
status `REBOOT_REQUIRED`, disconnect/invalidate old bonds, MITM+LESC para ACL
sensible y policy explícita de repairing/whitelist.

## Physical access

`NRF_BL_DEBUG_PORT_DISABLE=0` deja SWD abierto. Production profile debería activar
APPROTECT después de definir un recovery destructivo y separar claramente dev
images unlocked. Validar que signed DFU/recovery sigue funcionando.

## Authenticated boot

El bootloader implementa validación criptográfica de paquetes DFU, pero el trust
root actual está comprometido y por tanto no autentica al publisher en devices
actuales. Además `NRF_BL_APP_SIGNATURE_CHECK_REQUIRED=0` y factory settings no
fuerzan ECDSA boot validation. CRC no prueba autenticidad y algunos reset paths
pueden omitirlo.

Después de rotar key, production boot debe exigir P-256/SHA-256 app validation en
cold, system-off y GPREGRET resets. Alterar un byte debe llevar a recovery DFU.

## Availability de DFU

Single-bank borra la app antes de completar update. Dual-bank no cabe actualmente
en los 640 KiB con una app Ultra de ~349 KiB. Las opciones son:

- Autenticar DFU entry y garantizar bootloader-only recovery.
- Reducir imagen de forma importante.
- External staging o repartition/hardware distinto.

Debe probarse power loss en cada erase/write/activate boundary.

## Build isolation

`firmware/objects` compartido no codifica board/toolchain/config. `build.sh` limpia,
pero helper app-only usa direct make. Todo release usa output key por:

```text
device + toolchain digest + optimization + SDK validation + source hash
```

No firmar desde dirty tree salvo override explícito con provenance.

## Reproducibilidad y supply chain

Fortalezas existentes:

- Base Docker y archives principales tienen hashes.
- Ultra/Lite build matrix.
- Layout/trust-root correspondence checks.
- Mecanismo de signed update y downgrade prevention presente; publisher trust no
  es efectivo hasta completar la migración de F-001.

Gaps:

- apt/plugin/action tags mutables.
- Compose `:main` mutable.
- Sin `SOURCE_DATE_EPOCH` ni ZIP metadata normalization.
- Sin doble build reproducible ni signed provenance manifest.
- Toolchain local 8.5 difiere del canónico 12.2.
- Los checks declarados de Python no están verdes: `ruff` reporta 70 errores y
  `pyrefly` 420 en el árbol actual.

El workflow privilegiado `firmware-artifact-checks.yml` usa scripts del default
branch sin credenciales persistidas, valida nombre/path/status/repository del run
y restringe dispatch manual al default branch. Los campos map no confiables deben
ser hexadecimales exactos antes de entrar en aritmética Bash.

Gate objetivo: dos clean environments producen HEX/BIN y normalized DFU package
idénticos; manifest incluye source, dirty state, toolchain/SDK/action digests,
board, versions, partitions y artifact hashes.

## Secret/data exposure adicional

- Los guards de comandos/button corrigen shared HF type confusion; NFCT live
  mutation y type change owner replacement siguen abiertos en F-015/F-017.
- Clone nickname y NTAG mirror ya no exponen stack residue; mirror/parity tienen
  poison/canary tests bajo ASan/UBSan.
- FDS sin CRC permite corrupción no detectada.
- Raw persisted bitfields complican migrations y forensic verification.
- NOINIT evidence se borra con reset classification/hardcoded addresses.

Estas correcciones no sustituyen BLE ACL ni boot trust.
