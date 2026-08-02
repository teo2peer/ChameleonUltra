# Relay ISO-DEP autorizado: procesos, datagramas y recuperación

## 1. Objetivo y límites

Este documento describe, byte por byte y estado por estado, el laboratorio de relay
ISO-DEP autorizado implementado en este fork. Está pensado para poder reconstruir
qué ocurrió después de una prueba usando solamente:

- el trace EMV retenido por el firmware;
- el informe `chameleon-authorized-relay-debug` copiado desde Android;
- el estado mostrado por el terminal;
- los estados externos de Android, BLE, USB y del ChameleonUltra.

La documentación cubre:

- activación ISO14443-A;
- ISO-DEP, I-block, R-block, S(WTX), S(DESELECT), CRC y números de bloque;
- framing binario Chameleon sobre USB CDC y BLE NUS;
- comandos `6007` a `6014`;
- APDU EMV y BER-TLV;
- Android HCE, MethodChannel y EventChannel;
- preparación, armado, rendezvous, intercambio y limpieza;
- diferencias entre estado RF, estado Chameleon, SW1/SW2 y resultado del terminal;
- diagnóstico y recuperación cuando el relay es lento o falla.

No se debe interpretar una animación positiva del terminal como autorización bancaria.
El laboratorio no contacta al emisor ni al adquirente y no conoce el resultado final
de una transacción real.

Los ejemplos de datos de tarjeta son sintéticos. No se incluyen PAN, fechas, Track 2,
criptogramas ni identificadores reales observados durante pruebas.

## 2. Modelo mental: cuatro enlaces distintos

El flujo completo no es un único enlace. Hay cuatro canales con estados y tiempos
independientes:

```text
                    Link A: NFC / ISO-DEP / HCE
  terminal  <------------------------------------>  Android con CU GUI
                                                         |
                                                         | MethodChannel y
                                                         | EventChannel
                                                         v
                                                  lógica Flutter
                                                         |
                    canal de control Chameleon           | USB CDC o BLE NUS
                                                         v
                                                  ChameleonUltra
                                                         |
                    Link B: NFC-A / ISO-DEP               |
                                                         v
                                                 tarjeta o Wallet backend
```

### 2.1 Link A

El terminal actúa como lector. Android actúa como tarjeta mediante
`AuthorizedRelayHostApduService`.

Android entrega APDU lógicas a `processCommandApdu()`. La API pública HCE no entrega
PCB, CRC, R-blocks ni S(WTX) de Link A a Flutter.

### 2.2 Puente Android-Flutter

Android publica cada APDU mediante EventChannel. Flutter responde mediante
MethodChannel después de consultar al backend.

El `armToken` y el `requestId` pertenecen a este puente. No son números de bloque
ISO-DEP ni el `session_id` del firmware.

### 2.3 Canal de control Chameleon

Flutter envía START `6011` o `6014`, EXCHANGE `6012` y STOP `6013` por BLE NUS
o USB CDC.
El framing y los estados de este canal son propios del protocolo Chameleon.
Un START exitoso liga la sesión a ese transporte. Mientras esté activa, todo
comando recibido por el otro transporte se rechaza con `0066h` sin tomar ownership
ni modificar la sesión. La pérdida del enlace USB/BLE owner la cierra.

### 2.4 Link B

ChameleonUltra actúa como lector ISO14443-A/ISO-DEP. La tarjeta física o el Wallet
backend actúa como PICC.

WTX y R(NAK) observados aquí no amplían automáticamente el plazo de Link A.

## 3. Convenciones

- Todos los enteros multibyte Chameleon se codifican big-endian.
- Los hexadecimales se muestran con bytes separados por espacios.
- `C-APDU` significa APDU de comando.
- `R-APDU` significa APDU de respuesta.
- `PCD` es el lector, en Link B el ChameleonUltra.
- `PICC` es la tarjeta, en Link B la tarjeta o Wallet backend.
- `RF status` es un estado de la capa de radio del firmware.
- `SW` es el status word lógico de una R-APDU, formado por SW1 y SW2.
- `CRC_A` son dos bytes calculados para ISO14443-A. Se transmiten en el orden que
  espera el frontend RF y aparecen incluidos en los records RF del trace.

## 4. Framing binario Chameleon

USB CDC y BLE NUS transportan exactamente el mismo frame:

```text
Offset  Tamaño  Campo
0       1       SOF = 11
1       1       LRC1
2       2       command, big-endian
4       2       status, big-endian
6       2       payload_length, big-endian
8       1       LRC2
9       N       payload
9+N     1       LRC3
```

Tamaño total:

```text
10 + payload_length
```

Cada LRC es el complemento a dos de 8 bits de la suma protegida:

```text
LRC = (100h - (suma_de_bytes AND FFh)) AND FFh
```

`LRC1` protege SOF. `LRC2` protege la cabecera desde SOF hasta longitud. `LRC3`
protege el payload.

### 4.1 Ejemplo: EXCHANGE de GET DATA 9F36

Sesión sintética `12`:

```text
session_id = 00 00 00 0C
C-APDU     = 80 CA 9F 36 00
payload    = 00 00 00 0C 80 CA 9F 36 00
```

Frame completo de request `6012 = 177Ch`:

```text
11 EF 17 7C 00 00 00 09 64 00 00 00 0C 80 CA 9F 36 00 D5
```

Separación:

```text
11          SOF
EF          LRC1
17 7C       command 6012
00 00       request status
00 09       payload length
64          LRC2
00 00 00 0C session_id
80 CA 9F 36 00 C-APDU
D5          LRC3
```

Respuesta correcta con R-APDU `90 00`:

```text
11 EF 17 7C 00 00 00 02 6B 90 00 70
```

Respuesta de fallo con estado exterior `STATUS_HF_TAG_NO = 0001h` y diagnóstico
`ISO_DEP_ERR_TIMEOUT=07`, `RF status=01`, `WTX count=00`:

```text
11 EF 17 7C 00 01 00 03 69 07 01 00 F8
```

El `0001` de la cabecera no es SW1/SW2. En este fallo no existe una R-APDU real.

## 5. Activación ISO14443-A de Link B

START y el simulador EMV realizan activación antes de intercambiar APDU.

### 5.1 WUPA

PCD a PICC:

```text
52
```

Son 7 bits. No lleva CRC_A.

PICC a PCD, ATQA de ejemplo:

```text
08 03
```

ATQA describe propiedades de anticollision. No es una identidad única.

### 5.2 Anticollision cascade level 1

PCD:

```text
93 20
```

PICC, ejemplo sintético de UID de 4 bytes más BCC:

```text
08 11 22 33 08
```

El BCC es XOR de los cuatro bytes de UID:

```text
08 XOR 11 XOR 22 XOR 33 = 08
```

### 5.3 SELECT

PCD:

```text
93 70 08 11 22 33 08 CRC_A
```

PICC:

```text
20 CRC_A
```

SAK `20h` indica que la aplicación seleccionada soporta ISO-DEP.

UID de 7 o 10 bytes requiere cascade levels adicionales y CT `88h` según
ISO14443-A. El trace registra cada frame por separado.

### 5.4 RATS

El firmware usa:

```text
E0 40 CRC_A
```

`40h` contiene FSDI 4 y CID 0. FSDI 4 anuncia FSD 48.

### 5.5 ATS

Ejemplo observado y no identificativo:

```text
05 78 80 74 00 CRC_A
```

Interpretación sin CRC:

```text
05  TL, longitud de ATS
78  T0: TA(1), TB(1), TC(1) presentes; FSCI=8
80  TA(1)
74  TB(1): FWI=7, SFGI=4
00  TC(1)
```

FSCI 8 representa FSC 256. El lector de este fork lo limita internamente a 64 bytes
por el buffer RF usado por RC522.

La fórmula base de FWT es aproximadamente:

```text
FWT = 302 microsegundos * 2^FWI
```

Para FWI 7 son unos 38,7 ms. El firmware aplica un mínimo práctico de 50 ms y un
máximo de 5000 ms.

## 6. Frames ISO-DEP de Link B

Una vez aceptado ATS, los frames contienen PCB, INF opcional y CRC_A.

### 6.1 I-block

Formato simplificado usado aquí:

```text
PCB [CID] INF CRC_A
```

PCB habituales sin CID:

```text
02  I-block, número de bloque 0, sin chaining
03  I-block, número de bloque 1, sin chaining
12  I-block, número de bloque 0, con chaining
13  I-block, número de bloque 1, con chaining
```

Ejemplo PPSE a nivel RF:

```text
02 00 A4 04 00 0E 32 50 41 59 2E 53 59 53 2E 44 44 46 30 31 00 CRC_A
```

`02` no forma parte de la APDU. Es el PCB que añade el firmware.

### 6.2 Números de bloque

PCD y PICC alternan el bit de número de bloque. El estado persiste entre llamadas
`6012` de la misma sesión.

Abrir un nuevo START reinicia la negociación. Repetir una APDU dentro de una sesión
sin conocer el resultado anterior puede desincronizar tanto ISO-DEP como EMV.

### 6.3 Chaining

Si la APDU no cabe en FSC, el firmware la fragmenta en I-blocks con chaining.
La tarjeta confirma bloques intermedios con R(ACK).

La respuesta puede llegar en varios I-blocks. El lector envía R(ACK) y concatena INF
hasta el último bloque.

Límites:

```text
32 bloques intermedios de comando más el bloque final
32 I-blocks de respuesta
512 bytes lógicos de respuesta
```

### 6.4 R(ACK) y R(NAK)

Sin CID:

```text
A2 / A3  R(ACK), según número esperado
B2 / B3  R(NAK), según número esperado
```

R(ACK) confirma recepción y solicita el siguiente bloque encadenado.

R(NAK) solicita retransmisión del bloque de respuesta esperado. No vuelve a enviar
la C-APDU.

El firmware usa como máximo dos R(NAK) de recuperación:

- cuando no llega ninguna respuesta al último I-block de comando;
- cuando no llega el bloque esperado después de responder a S(WTX).

Cada espera de recuperación consume el presupuesto acumulado de 5000 ms.

### 6.5 S(WTX)

La tarjeta pide más tiempo:

```text
F2 WTXM CRC_A
```

Ejemplo con multiplicador 1:

```text
F2 01 CRC_A
```

El lector responde exactamente con el mismo PCB y multiplicador:

```text
F2 01 CRC_A
```

Reglas del firmware:

```text
WTXM válido                  1..59
WTX por APDU                 máximo 64
espera individual            máximo 5000 ms
espera acumulada WTX/retry   máximo 5000 ms
```

El timeout solicitado es:

```text
min(frame_timeout_ms * WTXM, 5000 ms)
```

Un diagnóstico `WTX count = 0` significa que falló antes de aceptar el primer WTX.
No significa necesariamente que la C-APDU no se transmitiera.

Un diagnóstico `WTX count = N` significa que se aceptaron N solicitudes WTX antes
del fallo. Es evidencia de actividad del backend, no de éxito lógico.

### 6.6 S(DESELECT)

STOP intenta:

```text
C2 CRC_A
```

La tarjeta puede contestar:

```text
C2 CRC_A
```

La limpieza local continúa aunque no llegue la respuesta.

## 7. Comandos persistentes 6011-6014

### 7.1 START, comando 6011 / 177Bh

Request:

```text
payload_length = 0
```

Respuesta correcta, estado exterior `0000h`:

```text
Offset          Tamaño     Campo
0               4          session_id uint32 no cero
4               1          uid_len: 4, 7 o 10
5               uid_len    UID
5+uid_len       2          ATQA
7+uid_len       1          SAK
8+uid_len       1          ats_len
9+uid_len       ats_len    ATS sin CRC_A
```

START apaga campo, reinicia RC522, activa, escanea, selecciona, ejecuta RATS,
inicializa estado ISO-DEP y asigna sesión.

Cada START aceptado desde el transporte owner aborta una sesión anterior antes de
activar la nueva. Un START del transporte no-owner se rechaza con `0066h` y no
afecta la sesión. El token es contador, no secreto.

Fallo de activación:

```text
Offset  Tamaño  Campo
0       1       fase: 01 scan/select, 02 ATS
1       1       HF status
```

### 7.1.1 START Apple Transit, comando 6014 / 177Eh

Request y respuesta son idénticos a 6011. Antes de WUPA, anticollision, SELECT y
RATS, el firmware configura temporalmente:

```text
frame ECP2 = 6A 02 C8 01 00 03 00 02 79 00 00 00 00 C2 D8
retries    = 30
delay      = 5 ms
timeout    = 2 ms
```

El polling alterna el frame ECP2 con WUPA. La anotación se limpia después de éxito,
no-card, colisión, fallo ATS o error interno. El timeout reader anterior se restaura
exactamente. Éxito exterior `0000h` confirma activación ISO-DEP, no que ECP2 causara
la presentación del Wallet ni que exista aprobación.

### 7.2 EXCHANGE, comando 6012 / 177Ch

Request:

```text
Offset  Tamaño   Campo
0       4        session_id
4       1..512   C-APDU lógica
```

No se incluyen PCB, CRC_A, CID, NAD ni chaining. Los añade firmware.

Respuesta correcta:

```text
estado exterior = 0000h
payload          = R-APDU lógica de 2..512 bytes, incluyendo SW1/SW2
```

`6A81`, `6A82`, `6400` o cualquier otro SW siguen siendo éxito de transporte si
llegaron como R-APDU desde la tarjeta.

Fallo RF/ISO-DEP:

```text
Offset  Tamaño  Campo
0       1       iso_dep_error
1       1       RF status
2       1       WTX count
```

Valores `iso_dep_error`:

| Valor | Significado |
|---:|---|
| 0 | Sin error de framing clasificado, pero respuesta lógica inválida |
| 1 | Parámetro interno inválido |
| 2 | Fallo de transporte RF |
| 3 | CRC inválido |
| 4 | Bloque ISO-DEP inválido |
| 5 | Secuencia o número de bloque inválido |
| 6 | Overflow de reensamblado |
| 7 | Timeout RF |

Tras cualquier fallo sincronizado de EXCHANGE, firmware:

1. invalida `session_id`;
2. borra estado de bloques;
3. apaga campo;
4. devuelve diagnóstico;
5. rechaza un STOP posterior de ese ID con `0060h`.

La GUI actual sabe que la sesión ya está cerrada y omite ese STOP redundante.

### 7.3 STOP, comando 6013 / 177Dh

Request:

```text
Offset  Tamaño  Campo
0       4       session_id
```

Respuesta correcta:

```text
status = 0068h STATUS_SUCCESS
payload_length = 0
```

Un ID inactivo o stale devuelve `0060h STATUS_PAR_ERR`.

Si el dispositivo ya no está en reader mode, un STOP bien formado devuelve `0066h`
después de asegurar que la sesión queda cerrada.

Después de un 6012 incierto, la GUI usa 6013 como reset idempotente y ordenado, no
como retry de la APDU. Sin un START intermedio, una respuesta vacía `0068h`, `0060h`
o `0066h` confirma que la sesión incierta está cerrada. La respuesta 6013 es una
barrera de orden: toda respuesta 6012 anterior ya fue consumida o quedó obsoleta,
por lo que el host puede limpiar solamente la cuarentena de 6012 y reutilizar la
misma conexión. Si no llega esa respuesta, el payload es inválido o aparece otro
status, el reset no está confirmado y se debe reconectar.

### 7.4 Sin timeout de inactividad

La sesión firmware no expira por tiempo ni por falta de EXCHANGE. El armado GUI/
native tampoco tiene lease ni expiración global, y la GUI ya no recicla sesiones
card-first. Solo cada APDU del terminal conserva su deadline nativo de 50..5000 ms.

La sesión termina por STOP/reset explícito, START de reemplazo del transporte owner,
fallo RF/ISO-DEP, salida de reader mode u otra invalidación segura iniciada por el
owner, o pérdida del enlace USB/BLE owner. Los comandos del transporte no-owner se
rechazan y no invalidan la sesión.

### 7.5 Mutación GPO Apple Transit

El modo está desactivado por defecto y requiere confirmación para una sesión. Tras
reenviar SELECT AID sin cambios y recibir `9000`, Flutter exige una FCI BER-TLV
completa con exactamente un `84` igual a la AID y un `9F38`. El PDOL se interpreta
como pares `tag,length`, nunca como TLV con valores.

En una C-APDU GPO short:

```text
80 A8 00 00 Lc 83 len valores_PDOL [Le]
```

solo se sustituyen las posiciones que el PDOL solicita:

```text
9F66 length 4 -> 33 80 40 00
9F35 length 1 -> 14
9F33 length 3 -> E0 08 00
```

`9F66` es obligatorio. Los otros dos tags son opcionales, pero si aparecen deben
tener longitud exacta. Importe, moneda, país, fecha, tipo de transacción, UN, Le,
los demás bytes y todas las R-APDU permanecen intactos. PDOL ausente, stale,
duplicado o malformado, GPO extendido, Lc/Le inconsistente o longitud tag 83 distinta
detiene el relay, no envía esa GPO al backend y entrega fallback `6400`. Al cerrar,
la GUI borra tarjeta/sesión preparada pero conserva el perfil Apple Transit para la
siguiente preparación y lo persiste localmente tras reiniciar la app hasta que el
operador lo desactive manualmente. No persiste AIDs preparados, tokens, APDU
pendientes ni sesiones firmware.

## 8. Estados exteriores Chameleon y SW de tarjeta

Estados exteriores relevantes:

| Estado | Significado |
|---:|---|
| `0000` | Operación HF correcta |
| `0001` | No hay tarjeta o timeout RF |
| `0002` | Error RF/ISO-DEP genérico |
| `0003` | CRC RF inválido |
| `0004` | Colisión |
| `0005` | BCC UID inválido |
| `0007` | Paridad RF inválida |
| `0008` | ATS ausente o inválido |
| `0060` | Parámetro, ID o sesión inválida |
| `0066` | Modo de dispositivo incorrecto o comando desde transporte no-owner |
| `0068` | Comando de control correcto |

Separación obligatoria:

```text
status exterior 0000 + R-APDU 6A81 = transporte correcto, aplicación rechaza
status exterior 0001 + payload 07 01 00 = no existe R-APDU real
```

## 9. APDU EMV observadas en el flujo

### 9.1 Estructura C-APDU corta

```text
CLA INS P1 P2 [Lc Data] [Le]
```

### 9.2 SELECT PPSE

```text
00 A4 04 00 0E 32 50 41 59 2E 53 59 53 2E 44 44 46 30 31 00
```

ASCII del nombre:

```text
2PAY.SYS.DDF01
```

Respuesta típica sintética:

```text
6F ...
  84 0E 325041592E5359532E4444463031
  A5 ...
    BF0C ...
      61 ...
        4F 07 A0000000031010
        87 01 01
90 00
```

Tags:

```text
6F    FCI template
84    DF name
A5    FCI proprietary template
BF0C  issuer discretionary data
61    application template
4F    AID
87    application priority
```

### 9.3 SELECT AID

Ejemplo Visa estándar:

```text
00 A4 04 00 07 A0 00 00 00 03 10 10 00
```

La respuesta FCI puede anunciar PDOL en tag `9F38`.

### 9.4 GET PROCESSING OPTIONS

```text
80 A8 00 00 Lc 83 PDOL_length PDOL_values 00
```

Tag `83` contiene valores concatenados en el orden exigido por PDOL. No contiene
los tags, solo sus valores.

Respuesta format 2:

```text
77 ...
  82 02 AIP
  94 ... AFL
  [9F26 cryptogram]
  [9F10 issuer application data]
  [9F36 ATC]
  [otros tags]
90 00
```

Respuesta format 1 usa tag `80` con AIP y AFL compactados.

### 9.5 READ RECORD

```text
00 B2 record_number ((SFI << 3) OR 04) 00
```

Ejemplo:

```text
00 B2 01 1C 00
```

La respuesta suele usar template `70`. Puede incluir datos de aplicación sensibles.

### 9.6 GET DATA

Comandos frecuentes:

```text
80 CA 9F 36 00  Application Transaction Counter
80 CA 9F 13 00  Last Online ATC Register
80 CA 9F 17 00  PIN Try Counter
80 CA 9F 4F 00  Log Format
```

No todas las aplicaciones soportan todos los objetos. `6A81`, `6A88` o `6400` pueden
ser respuestas lógicas válidas del backend.

Un `RF timeout` durante `80 CA 9F 36 00` es diferente: no hubo SW real. El relay
envía fallback `6400`, cierra y no repite la C-APDU.

### 9.7 GENERATE AC

```text
80 AE P1 00 Lc CDOL1_values 00
```

P1 puede solicitar AAC, TC o ARQC. La respuesta puede contener `9F26`, `9F27`,
`9F10` y `9F36`.

Generar un criptograma puede avanzar estado aunque no se contacte a ningún banco.

## 10. AIP, DDA/CDA, RRP y datos legibles

Tag `82` es AIP. Primer byte EMV:

```text
bit 7  SDA
bit 6  DDA
bit 5  cardholder verification
bit 4  terminal risk management
bit 3  issuer authentication
bit 2  on-device CVM
bit 1  CDA
```

La numeración anterior usa bit 7 como máscara `40h` y bit 1 como `01h`.

RRP en AIP byte 2 bit 1 es una interpretación de Mastercard Kernel 2. No se debe
aplicar a una AID Visa `A000000003...` ni a otro esquema.

Ejemplo AIP `20 20`:

- anuncia DDA por `20h` en byte 1;
- no permite afirmar RRP sin conocer la AID;
- si la AID no es Mastercard, el indicador RRP de Mastercard no aplica;
- no dice si PAN o fecha son legibles;
- no prueba que DDA se haya verificado criptográficamente;
- no prueba aprobación, rechazo ni protección efectiva del terminal.

Son hallazgos independientes:

```text
visibilidad de PAN/expiry
autenticación dinámica DDA/CDA
soporte RRP específico de esquema
aplicación real de timing por el terminal
autorización online del emisor
```

## 11. Trace EMV retenido: comandos 6007-6009

### 11.1 START trace, comando 6007 / 1777h

Payload base de 25 bytes:

```text
Offset  Tamaño  Campo
0       1       version = 01
1       1       option flags
2       1       max_aids, 0..16
3       1       max_records, 0..64
4       2       max_apdus, 0..512
6       4       budget_ms, 0..30000
10      6       amount n12 BCD
16      2       country
18      2       currency
20      3       date YYMMDD BCD
23      1       transaction_type
24      1       cryptogram_type: FF, 00, 40 u 80
```

Flags:

| Bit | Valor | Significado |
|---:|---:|---|
| 0 | `01` | maximum processing |
| 1 | `02` | incluir RF |
| 2 | `04` | timestamps |
| 3 | `08` | record grid |
| 4 | `10` | transaction logs |
| 5 | `20` | PDOL fallback |
| 6 | `40` | express transit |
| 7 | `80` | extensión de perfil presente |

Extensión a 30 bytes:

```text
25      1       terminal_profile
26      4       custom TTQ
```

Extensión a 35 bytes:

```text
30      1       polling_profile
31      1       behavior flags
32      1       poll_retries
33      1       poll_delay_ms
34      1       poll_timeout_ms
```

Respuesta START, 10 bytes:

```text
Offset  Tamaño  Campo
0       1       version
1       1       state
2       4       scan_id
6       4       trace flags
```

Estados:

```text
0 empty
1 running
2 complete
3 aborted
```

### 11.2 META, comando 6008 / 1778h

Request:

```text
01 scan_id_u32
```

Respuesta:

```text
Offset  Tamaño  Campo
0       1       version
1       1       state
2       2       result_status
4       4       flags
8       4       scan_id
12      4       stored_records
16      4       observed_records
20      4       stored_bytes
24      4       required_bytes
28      4       first_dropped o FFFFFFFF
32      4       CRC32 del stream almacenado
36      2       application_count
38      4       elapsed_ms
42      1       uid_len
43      uid_len UID
...     2       ATQA
...     1       SAK
...     1       ats_len
...     ats_len ATS
```

Flags META:

| Valor | Significado |
|---:|---|
| `00000001` | complete |
| `00000002` | timeout |
| `00000004` | logical log truncated |
| `00000008` | RF detail truncated |
| `00000010` | response truncated |
| `00000020` | application limit reached |
| `00000040` | timestamps valid |
| `00000080` | maximum processing |
| `00000100` | transport error |
| `00000200` | express transit |

Ejemplo `flags = 000000C1h`:

```text
00000001 complete
00000040 timing valid
00000080 maximum processing
```

No contiene timeout, truncación ni transport error.

### 11.3 GET pages, comando 6009 / 1779h

Request, 11 bytes:

```text
Offset  Tamaño  Campo
0       1       version
1       4       scan_id
5       4       start_record
9       2       max_payload
```

Respuesta header, 18 bytes:

```text
Offset  Tamaño  Campo
0       1       version
1       1       page flags
2       4       scan_id
6       4       start_record
10      4       next_record
14      2       returned_count
16      2       records_bytes
18      N       records atómicos completos
```

Page flags:

```text
01 hay más páginas
02 última página
04 log truncado
```

El cliente rechaza:

- cambio de `scan_id`;
- cursor inesperado;
- página sin progreso;
- record parcial;
- longitud incongruente;
- CRC32 final distinto;
- número de records distinto de META.

### 11.4 Formato común de record

```text
Offset  Tamaño  Campo
0       2       body_length
2       1       version
3       1       type
4       4       sequence
8       1       stage
9       1       application_index
10      1       attempt
11      1       flags
12      2       status
14      4       timestamp_ms
18      N       payload específico
```

Tipos:

```text
1 RF frame
2 logical APDU
3 application
4 summary
```

Stages:

| Valor | Stage |
|---:|---|
| 0 | activation |
| 1 | PPSE |
| 2 | SELECT application |
| 3 | GET DATA |
| 4 | GPO |
| 5 | READ AFL |
| 6 | record-grid scan |
| 7 | GET RESPONSE |
| 8 | GENERATE AC |
| 9 | transaction log |
| `FF` | summary |

### 11.5 Payload RF

```text
Offset  Tamaño  Campo
0       1       direction: 0 readerToCard, 1 cardToReader
1       2       bit_length
3       2       data_length
5       N       data, incluyendo PCB/CRC cuando corresponda
```

Record flag `01` indica que `bit_length` no es múltiplo de 8, por ejemplo WUPA de
7 bits. No significa fallo.

### 11.6 Payload APDU

```text
Offset  Tamaño  Campo
0       2       status_word o FFFF si no hay respuesta
2       2       command_length
4       2       response_length
6       C       C-APDU
6+C     R       R-APDU
```

Flags APDU:

```text
01 respuesta tiene al menos SW1/SW2
02 SW = 9000
04 error de transporte ISO-DEP
```

El campo `status` del record APDU contiene `iso_dep_error`, no SW1/SW2.

### 11.7 Payload application

```text
Offset  Tamaño  Campo
0       1       aid_length
1       N       AID
1+N     1       priority
```

### 11.8 Payload summary

```text
Offset  Tamaño  Campo
0       4       stored_records antes del summary
4       4       observed_records antes del summary
8       4       flags
```

El summary es el último record y usa stage `FFh`.

## 12. Debug counters 6010 / 177Ah

Request esperado sin payload. Respuesta con estado `0068h` y cuatro bytes:

```text
Offset  Tamaño  Campo
0       1       received I-block count
1       1       transmitted I-block count
2       1       last received PCB
3       1       last static response match
```

Estos counters pertenecen al emulador ISO-DEP y sirven como diagnóstico. No son el
contador WTX de una sesión reader `6012`.

## 13. Android HCE y datagramas de plataforma

### 13.1 Preparación

Flutter:

1. abre START;
2. envía PPSE al backend;
3. valida TLV y extrae AIDs `4F` bajo templates `61`;
4. registra PPSE y AIDs como categoría payment;
5. marca native `prepared=true`;
6. mantiene HCE desarmado hasta confirmación del operador.

Cuando la validación de UID rotatorio ejecuta SELECT PPSE en la nueva sesión
firmware, Flutter conserva esa respuesta exacta para un único APDU. Si el primer
APDU del terminal es el mismo SELECT PPSE, entrega esos mismos bytes sin repetir el
comando en el backend. La entrada se consume también si no coincide y nunca cruza
sesiones.

### 13.2 Armado

Native asigna `armToken` positivo. `setEnabled(true)` exige:

- HCE soportado;
- NFC encendido;
- móvil desbloqueado;
- CU GUI como servicio de pago predeterminado;
- PPSE y al menos una AID dinámica;
- deadline entre 50 y 5000 ms.

El armado no tiene deadline global. Ese rango se aplica por separado a cada APDU
terminal recibida.

### 13.3 Evento APDU

EventChannel:

```json
{
  "type": "apdu",
  "armToken": 5,
  "id": 37,
  "apduHex": "80CA9F3600",
  "receivedUs": 100000000,
  "expiresAtUs": 100001000
}
```

`receivedUs` y `expiresAtUs` usan reloj monotónico Android, no hora UTC.

### 13.4 Respuesta

MethodChannel:

```json
{
  "armToken": 5,
  "id": 37,
  "responseHex": "9000"
}
```

Native acepta solo token e ID activos antes del deadline.

### 13.5 Eventos de cierre

Expired:

```json
{"type":"expired","armToken":5,"id":37}
```

Deactivated:

```json
{"type":"deactivated","armToken":5,"reason":0}
```

Razones Android:

```text
0 LINK_LOSS
1 DESELECTED
```

Después de una o más respuestas entregadas, `LINK_LOSS` suele significar que el
lector apagó campo al finalizar. Es cierre normal de transporte, no prueba de error,
aprobación ni rechazo.

Antes de la primera respuesta, LINK_LOSS sí indica que el flujo no completó una APDU.

## 14. Rendezvous y secuencia completa

Se permiten dos órdenes.

### 14.1 Backend primero

```text
Prepare/START -> backend activo -> Arm -> esperar terminal -> APDU -> EXCHANGE
```

Es el orden con menos latencia dentro del deadline HCE.

### 14.2 Terminal primero

```text
Arm -> APDU terminal pendiente -> buscar backend -> START -> EXCHANGE
```

START y EXCHANGE deben terminar dentro del deadline nativo. Es más frágil.

### 14.3 Por cada APDU

```text
1 Android recibe C-APDU
2 native valida readiness y deadline
3 native publica EventChannel
4 Flutter valida armToken e id
5 Flutter une terminal y backend
6 Flutter llama isPending
7 política Apple observa SELECT/FCI o reescribe GPO; transparente no cambia bytes
8 Flutter envía 6012, salvo rechazo fail-closed de GPO
9 firmware mantiene números de bloque ISO-DEP
10 backend devuelve R-APDU o fallo RF
11 Flutter responde por MethodChannel
12 native llama sendResponseApdu
13 terminal continúa o apaga campo
```

Solo hay una APDU terminal pendiente. No hay transaction ID en protocolo Chameleon.
Native vuelve a comparar `expiresAtUs` atómicamente dentro de `respond`, no solo en
`isPending`. Si deadline o deactivación sucede después de iniciar 6012, el estado
del backend se marca incierto y esa APDU no se debe reintentar.

## 15. Status words generados por el relay

| SW | Origen |
|---|---|
| `6700` | APDU terminal nula o fuera de 4..512 bytes |
| `6985` | Ya existe otra APDU pendiente |
| `6400` | Relay no disponible o fallo backend sincronizado |
| `6401` | Deadline nativo venció |
| otro SW | Respuesta real del backend, reenviada sin cambios |

`responseSource=relayFallback` en el debug JSON identifica `6400` sintético.

Una animación de terminal puede aparecer después de `6400`. Significa que el
terminal terminó su UX NFC, no que la R-APDU backend existiera.

## 16. Informe `chameleon-authorized-relay-debug`

Cabecera:

```json
{
  "protocol": "chameleon-authorized-relay-debug",
  "version": 2,
  "generatedAt": "UTC ISO-8601"
}
```

Estado:

```text
connected              canal Chameleon conectado
transport              BLE o USB
prepared               backend y AIDs preparados
selectedBackendMode     transparent o appleTransit
preparedBackendMode     modo usado durante preparación
armedBackendMode        modo fijado para la sesión armada
rotatingBackendUid      modo Wallet móvil
armed/arming/busy       estado GUI
relayPhase              espera o intercambio
armToken                token native actual
terminalDeadlineMs      deadline configurado
deliveredApdus          respuestas reales entregadas en sesión actual
backendApduInFlight      6012 iniciado y todavía sin entrega confirmada
error/notice            cierre o fallo más reciente
```

Después de cleanup es normal ver:

```text
prepared=false
armed=false
registeredAids=[]
backend=null
```

Record por APDU:

```text
armToken
requestId
receivedUs
expiresAtUs
backendSessionId
terminalBudgetUs
backendMode             transparent o appleTransit
command                 CLA INS P1 P2 y longitud
status                  SW enviado al terminal
elapsedUs               tiempo Flutter total, incluido isPending y entrega native
backendExchangeUs       tiempo de 6012; cero para backendPrefetch
nativeDeliveryUs        tiempo de MethodChannel/respond, incluido fallback
delivered               native aceptó sendResponseApdu
responseSource          backend, backendPrefetch o relayFallback
deviceStatus            estado exterior 6012
isoDepError
rfStatus
wtxCount
firmwareSessionClosed
transitRewrite          AID, PDOL y before/after solo de 9F66/9F35/9F33
transitPolicy           razón bounded de rechazo/FCI inválida, si existe
error
```

`transitRewrite` no contiene la GPO completa, importe, UN, R-APDU ni valor Le.

`delivered=true` no significa aprobado. Solo confirma que native aceptó la respuesta
para la APDU pendiente.

## 17. Interpretación de casos comunes

### 17.1 Trace completo con muchos WTX

Indicadores:

```text
state=complete
resultStatus=0
storedRecords=observedRecords
storedBytes=requiredBytes
firstDropped=FFFFFFFF
flags incluye COMPLETE y no TRANSPORT_ERROR
```

Conclusión: activación, PPSE y flujo configurado completaron transporte. SW distintos
de `9000` siguen siendo resultados lógicos del backend.

### 17.2 `RF timeout`, `device status 01`, `RF status 01`, `0 WTX`

La C-APDU fue emitida, pero no se aceptó ningún S(WTX) ni I-block de respuesta.

Firmware actualizado solicita hasta dos retransmisiones con R(NAK). Si ambas fallan,
cierra sesión y devuelve diagnóstico.

### 17.3 Timeout con WTX mayor que cero

El backend seguía activo y pidió tiempo, pero no entregó el bloque esperado antes de
agotar espera/recovery.

### 17.4 Cierre después de 11 respuestas

Si no existe record `relayFallback` ni error, Android LINK_LOSS tras 11 respuestas
es normalmente el terminal apagando campo. No es motivo para repetir la transacción.

### 17.5 Simulator extrae PAN/expiry y AIP no anuncia RRP

Leer datos y evaluar RRP son procesos independientes. Para una AID no Mastercard,
el bit RRP de Mastercard no aplica. DDA/CDA tampoco oculta PAN/expiry: describe
autenticación dinámica, no confidencialidad.

## 18. Recuperación segura: regla principal

Nunca repetir automáticamente una APDU si existe posibilidad de que el backend la
haya procesado.

Un 6012 incierto nunca se repite. Si framing y conexión siguen válidos, se envía
6013 en orden con el ID conocido. Una respuesta vacía `0068h`, `0060h` o `0066h`
confirma el cierre y permite limpiar solo la cuarentena 6012 y abrir una sesión nueva
en la misma conexión. Se reconecta únicamente si el reset no puede confirmarse o si
framing, write o transporte ya quedaron invalidados. No se puede asumir que repetir
GPO, GENERATE AC u otra APDU stateful sea inocuo.

## 19. Runbook general para continuar después de un fallo

### Paso 1: conservar evidencia

Antes de pulsar Prepare otra vez:

1. copiar `chameleon-authorized-relay-debug`;
2. anotar resultado visual exacto del terminal;
3. guardar trace EMV si se estaba usando simulador;
4. anotar qué Ultra estaba encendido;
5. no publicar PAN, expiry, Track 2, UID, criptogramas ni APDU completas.

### Paso 2: clasificar certeza

Fallo sincronizado:

```text
6012 respondió con deviceStatus/isoDepError/rfStatus
```

Firmware ya cerró sesión. Se puede preparar una nueva sesión, pero no repetir la
APDU fallida como si nada.

Fallo incierto:

```text
timeout de respuesta 6012 con framing intacto
respuesta tardía 6012 posible
```

No repetir 6012. Enviar 6013 ordenado. Si responde vacío con `0068`, `0060` o
`0066`, la barrera confirma cierre y se reutiliza la conexión después de limpiar
solo la cuarentena 6012.

Transporte invalidado:

```text
write timeout o fallo de escritura
frame parcial o framing inválido
desconexión/reemplazo de communicator
6013 sin confirmación de reset
```

Desconectar y reconectar Chameleon antes de otra operación. En estos casos no hay
una barrera confiable sobre la conexión anterior.

### Paso 3: restaurar estado externo

1. dejar un solo Ultra encendido;
2. verificar que es el firmware correcto;
3. conservar la conexión si la barrera 6013 quedó confirmada; cerrar/reconectar
   solo si el reset o el transporte quedaron inciertos;
4. desbloquear Android;
5. verificar NFC encendido;
6. verificar CU GUI como payment default;
7. abrir de nuevo el Wallet backend si dejó de responder;
8. colocar backend estable sobre antena;
9. ejecutar Prepare;
10. comprobar PPSE y AIDs;
11. Arm;
12. acercar Android al terminal una sola vez.

### Paso 4: interpretar el cierre

```text
notice de cierre normal + solo backend responses = no retry automático
relayFallback 6400/6401 = fallo de esa APDU
timeout 6012 = resultado incierto, no retry; confirmar reset 6013 o reconectar
terminal animation = UX, no evidencia protocolaria suficiente
```

## 20. Recuperación por síntoma

### 20.1 Prepare espera tarjeta indefinidamente

Causa probable: START devuelve `0001` de forma limpia.

Acciones:

1. alinear tarjeta/Wallet sobre la antena HF;
2. desbloquear Wallet;
3. mantener posición;
4. dejar que Prepare reintente `STATUS_HF_TAG_NO`;
5. cancelar explícitamente si no se usará.

No convertir otros estados en retry automático.

### 20.2 UID cambia

Tarjeta física: tratar como backend distinto.

Wallet móvil: activar rotating UID solo si ATQA, SAK, ATS y respuesta PPSE completa
coinciden con preparación.

No aceptar solamente por AID o tipo de tarjeta.

### 20.3 Relay lento pero todavía correcto

Usar los `elapsedUs` del debug JSON.

Acciones ordenadas:

1. preparar backend antes de presentar terminal;
2. mantener ambos acoplamientos NFC estables;
3. usar BLE cifrado y sin reconexiones;
4. cerrar escaneos BLE/radio concurrentes;
5. usar firmware actual con WTX y R(NAK);
6. aumentar deadline de 1000 a 1500 o 2000 ms solo si el terminal tolera esa espera;
7. no superar 5000 ms;
8. recordar que Android HCE no garantiza WTX hacia el terminal.

Si el terminal apaga campo antes, aumentar el deadline GUI no ayuda.

### 20.4 `6401`

El watchdog Android venció antes de recibir respuesta.

Acciones:

1. no repetir la APDU;
2. esperar a que termine/sea descartada la respuesta backend tardía;
3. dejar que cleanup envíe 6013 ordenado;
4. reutilizar la conexión si 6013 responde vacío con `0068`, `0060` o `0066`;
5. reconectar solo si no se confirma ese reset o se invalidó framing/transporte;
6. preparar sesión nueva;
7. reducir latencia o aumentar deadline con cautela.

### 20.5 `6400` con `responseSource=relayFallback`

No es SW real de tarjeta.

Acciones:

1. leer `command`, `deviceStatus`, `isoDepError`, `rfStatus`, `wtxCount`;
2. si `firmwareSessionClosed=true`, no enviar STOP manual;
3. no repetir automáticamente `command`;
4. reabrir Wallet/tarjeta backend;
5. preparar una sesión nueva;
6. repetir el procedimiento completo solo con autorización y entendiendo posibles
   efectos de la APDU anterior.

### 20.6 RF timeout con cero WTX

Acciones:

1. confirmar firmware con recovery inicial R(NAK);
2. mejorar acoplamiento backend;
3. identificar la APDU exacta en `command`;
4. comprobar si era una consulta opcional GET DATA o parte transaccional;
5. abrir sesión nueva, nunca replay ciego.

### 20.7 RF timeout después de varios WTX

Acciones:

1. confirmar que WTX count está bajo 64;
2. confirmar que espera acumulada no superó 5000 ms;
3. mantener Wallet despierto;
4. reducir interferencia RF/BLE;
5. usar recovery R(NAK) actual;
6. si persiste siempre en la misma APDU, considerar política/estado del Wallet, no
   solamente timing.

### 20.8 STOP devuelve `0060`

Después de fallo sincronizado de EXCHANGE es esperado porque firmware ya abortó.
En cleanup normal la GUI puede omitirlo. Tras un 6012 host-incierto, en cambio,
`0060` es una respuesta válida de reset y su recepción actúa como barrera de orden.

Si aparece tras un flujo sin fallo previo, el ID era stale o la sesión ya estaba
cerrada; no existe expiración por inactividad.

### 20.9 LINK_LOSS tras varias respuestas reales

Si el terminal muestra resultado y no existe fallback:

```text
es cierre normal del campo
no es error por sí solo
no repetir
```

Si ocurrió antes de la primera respuesta, revisar default payment, AIDs, posición y
deadline.

### 20.10 Simulator funciona pero relay falla

El simulador y el relay no tienen el mismo pacing:

- simulador controla directamente Link B;
- relay añade EventChannel, Flutter, BLE/USB y HCE;
- terminal decide APDU y tiempos;
- Wallet puede responder distinto por perfil/estado.

Comparar la última APDU correcta y primera APDU fallida, no solo PAN/expiry final.

### 20.11 BLE se desconecta

1. desarmar;
2. considerar cerrada por firmware la sesión cuyo owner era ese enlace BLE;
3. no conservar `session_id` anterior;
4. reconectar BLE y aceptar pairing;
5. esperar capability initialization;
6. Prepare de nuevo;
7. Arm de nuevo.

### 20.12 Se flasheó el Ultra equivocado

Con varios Ultras:

1. apagar todos menos uno;
2. identificar serial USB;
3. flashear paquete correcto;
4. reiniciar;
5. verificar 6012, 6013 y START 6011 normal o 6014 Apple Transit;
6. conectar por BLE solamente ese equipo.

## 21. Qué mecanismos recuperan automáticamente

El sistema sí recupera de forma acotada:

- START reintenta solo no-card limpio durante preparación;
- command I-block puede retransmitirse hasta dos veces si PICC devuelve R(NAK);
- respuesta ausente puede solicitarse hasta dos veces con R(NAK);
- WTX permite espera backend hasta límites definidos;
- DFU helper reintenta programación transitoria;
- cleanup intenta desarmar native, confirmar reset 6013 si 6012 quedó incierto,
  detener sesión si sigue activa y limpiar AIDs.

El sistema no recupera automáticamente:

- APDU cuyo resultado es incierto;
- timeout de write;
- resultado/APDU de un timeout host de 6012; cleanup puede confirmar el cierre,
  pero no recuperar la R-APDU;
- identidad backend distinta;
- PPSE distinto en modo UID rotatorio;
- deadline terminal expirado;
- cambio de payment default;
- proceso Android muerto.

## 22. Checklist antes de reanudar

- [ ] Evidencia copiada y redacted.
- [ ] Resultado del terminal anotado sin asumir aprobación.
- [ ] Solo un Ultra encendido.
- [ ] Firmware actual instalado en ese Ultra.
- [ ] BLE/USB estable; reset 6013 confirmado tras 6012 incierto o communicator
      nuevo si no pudo confirmarse.
- [ ] Android desbloqueado y NFC activo.
- [ ] CU GUI sigue como payment default.
- [ ] Wallet backend abierto y colocado estable.
- [ ] Prepare muestra backend y AIDs correctos.
- [ ] Deadline basado en latencias observadas, máximo 5000 ms.
- [ ] No se está repitiendo ciegamente una APDU stateful.
- [ ] Arm pertenece a una sesión nueva.

## 23. Archivos fuente relacionados

Firmware:

```text
firmware/application/src/rfid/reader/hf/iso_dep_reader.c
firmware/application/src/rfid/reader/hf/iso_dep_session.c
firmware/application/src/rfid/reader/hf/emv_trace.c
firmware/application/src/app_cmd.c
firmware/application/src/data_cmd.h
firmware/application/src/app_status.h
```

Android/Flutter:

```text
android/app/src/main/kotlin/io/chameleon/ultra/AuthorizedRelayHostApduService.kt
android/app/src/main/kotlin/io/chameleon/ultra/MainActivity.kt
lib/bridge/authorized_relay_platform.dart
lib/bridge/chameleon.dart
lib/gui/menu/hacking/authorized_relay_lab.dart
lib/helpers/authorized_relay.dart
lib/helpers/emv.dart
lib/helpers/emv_trace.dart
```

Host Python:

```text
software/script/chameleon_com.py
software/script/chameleon_cmd.py
software/script/emv_trace.py
```

Documentación complementaria:

```text
docs/authorized-iso-dep-relay.md
docs/apdu-command-reference.md
docs/emv-purchase-simulator.md
```
