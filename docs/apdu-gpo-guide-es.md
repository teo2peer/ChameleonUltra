# Guia de APDU, GPO y PDOL en EMV contactless

Esta guia explica las unidades APDU, el comando EMV GPO, la lista PDOL y su
transporte sobre ISO-DEP. Tambien describe como aparecen esas capas en el relay
ISO-DEP autorizado de este proyecto.

No es una especificacion de un kernel EMV certificado. Los ejemplos sirven para
analisis de interoperabilidad con tarjetas, Wallets y terminales propios o
expresamente autorizados, aislados de redes de pago de produccion.

## Resumen mental

- **APDU** es el mensaje logico utilizado para hablar con una aplicacion de
  tarjeta inteligente.
- **GPO** es un comando EMV concreto enviado dentro de una APDU.
- **PDOL** define los datos de terminal que la aplicacion quiere recibir en GPO.
- **ISO-DEP** transporta la APDU por NFC y puede dividirla en varios bloques.
- **WTX** es control de espera de ISO-DEP; no es una APDU ni una respuesta EMV.

Una analogia util es:

```text
APDU       = el sobre y su formato
GPO        = una pregunta concreta dentro del sobre
ISO-DEP    = el transporte que lleva el sobre por NFC
I/R/S block = los paquetes y controles usados por ese transporte
```

## 1. Que es una APDU

APDU significa **Application Protocol Data Unit**. Es la unidad logica de
comunicacion de ISO/IEC 7816-4 utilizada tambien por aplicaciones EMV sobre
contactless.

Hay dos direcciones:

- **C-APDU**: command APDU enviada por el lector o terminal.
- **R-APDU**: response APDU devuelta por la tarjeta o Wallet.

### 1.1 Estructura de una C-APDU

La forma general corta es:

```text
CLA INS P1 P2 [Lc] [Data] [Le]
```

| Campo | Tamano | Funcion |
|---|---:|---|
| `CLA` | 1 byte | Clase o espacio de instrucciones |
| `INS` | 1 byte | Instruccion concreta |
| `P1` | 1 byte | Primer parametro |
| `P2` | 1 byte | Segundo parametro |
| `Lc` | 0 o mas | Longitud de `Data` |
| `Data` | variable | Datos enviados a la aplicacion |
| `Le` | 0 o mas | Longitud maxima de respuesta esperada |

Casos cortos habituales:

| Caso | Estructura |
|---|---|
| 1 | `CLA INS P1 P2` |
| 2 | `CLA INS P1 P2 Le` |
| 3 | `CLA INS P1 P2 Lc Data` |
| 4 | `CLA INS P1 P2 Lc Data Le` |

En una APDU corta, `Le=00` normalmente solicita hasta 256 bytes. No significa
necesariamente que el terminal espere cero bytes.

Las APDU extendidas utilizan campos de longitud mayores. El modo Apple Transit
del relay de este proyecto acepta un GPO corto estricto y rechaza un GPO
extendido, porque la politica necesita una forma unica y verificable antes de
modificar bytes.

### 1.2 Ejemplo SELECT PPSE

```text
00 A4 04 00 0E 325041592E5359532E4444463031 00
```

| Bytes | Significado |
|---|---|
| `00` | CLA estandar |
| `A4` | Instruccion SELECT |
| `04` | Seleccion por nombre o AID |
| `00` | Parametro P2 |
| `0E` | `Lc`: 14 bytes |
| `325041592E5359532E4444463031` | ASCII `2PAY.SYS.DDF01` |
| `00` | `Le` corto maximo |

PPSE significa **Proximity Payment System Environment**. Permite descubrir las
aplicaciones de pago contactless anunciadas por el dispositivo.

### 1.3 Ejemplo SELECT AID

Despues de PPSE, el terminal puede seleccionar una aplicacion concreta:

```text
00 A4 04 00 07 A0000000031010 00
```

El valor `A0000000031010` es un ejemplo de AID. La respuesta SELECT contiene
normalmente FCI, **File Control Information**, con identidad, parametros y datos
solicitados por esa aplicacion.

### 1.4 Estructura de una R-APDU

La respuesta tiene esta forma:

```text
[Response Data] SW1 SW2
```

Los dos ultimos bytes siempre forman el status word.

| Estado | Significado habitual |
|---|---|
| `9000` | Comando procesado correctamente |
| `61xx` | Hay mas bytes disponibles |
| `6700` | Longitud incorrecta |
| `6982` | Estado de seguridad insuficiente |
| `6985` | Condiciones de uso no satisfechas |
| `6A80` | Datos de comando incorrectos |
| `6A82` | Archivo, aplicacion u objeto no encontrado |
| `6A83` | Registro no encontrado |
| `6A86` | P1/P2 incorrectos |
| `6Cxx` | `Le` incorrecto; `SW2` indica la longitud esperada |
| `6D00` | Instruccion no soportada |
| `6E00` | Clase no soportada |
| `6F00` | Fallo no especificado |

Una respuesta puede incluir datos antes de `SW1 SW2`:

```text
9F360200129000
```

Interpretacion:

```text
9F36 02 0012 9000
|    |  |    |
|    |  |    +-- exito
|    |  +------- valor del contador
|    +---------- longitud: 2 bytes
+--------------- Application Transaction Counter
```

Una respuesta tambien puede contener solo el estado:

```text
6A82
```

El significado exacto depende del comando y del estado de la aplicacion. El relay
debe devolver ese resultado exacto; no es seguro sustituirlo por una respuesta
cacheada de otra APDU o sesion.

## 2. APDU frente a ISO-DEP

La APDU es logica. La radio no tiene por que transportarla como una sola trama:

```text
APDU EMV
   |
   v
ISO-DEP / ISO 14443-4
   |
   v
I-blocks, R-blocks y S-blocks
   |
   v
Tramas NFC-A con control y CRC
```

### 2.1 I-block

Transporta fragmentos de la APDU. Una APDU grande puede necesitar chaining de
varios I-blocks. ISO-DEP conserva un numero de bloque para detectar duplicados y
mantener la secuencia.

### 2.2 R-block

Confirma bloques o solicita retransmision mediante R(ACK) o R(NAK). Una peticion
R(NAK) de la respuesta no autoriza a reenviar la APDU logica completa, porque esa
APDU podria haber avanzado estado EMV.

### 2.3 S-block y WTX

Un S(WTX), **Waiting Time Extension**, permite que la tarjeta solicite mas tiempo
para completar el procesamiento. El lector responde al S(WTX) y continua esperando
la misma R-APDU.

WTX no es:

- un comando EMV;
- una APDU adicional;
- una aprobacion de transaccion;
- una autorizacion para repetir el comando original.

Una Wallet puede generar muchos S(WTX) mientras calcula, consulta el secure
element o aplica su politica local. Reducir trabajo en Flutter o BLE no elimina
ese tiempo interno de la Wallet.

El relay tiene dos enlaces ISO-DEP independientes:

```text
Enlace A: terminal <-> Android HCE
Enlace B: ChameleonUltra <-> tarjeta o Wallet backend
```

Un WTX en el enlace B no se convierte automaticamente en WTX del enlace A.
Android `HostApduService` tampoco permite a Flutter ordenar directamente un WTX
hacia el terminal.

## 3. Flujo EMV simplificado

Un intercambio contactless comun sigue aproximadamente este orden:

```text
1. SELECT PPSE
2. SELECT AID
3. GPO
4. READ RECORD
5. Comprobaciones de terminal y tarjeta
6. GENERATE AC, cuando corresponde
7. Autorizacion online, cuando corresponde
```

Cada paso puede depender del estado creado por el anterior. Por esa razon el relay
mantiene una unica sesion ISO-DEP y no re-selecciona la tarjeta para cada APDU.

### 3.1 PPSE

La respuesta PPSE puede contener plantillas de aplicacion `61`, AIDs `4F` y
prioridades `87`. Descubrir un AID no demuestra que la aplicacion vaya a aceptar la
transaccion.

### 3.2 SELECT AID y FCI

La respuesta de la aplicacion puede contener:

- `84`: Dedicated File Name, normalmente el AID seleccionado;
- `50`: etiqueta de aplicacion;
- `87`: prioridad;
- `9F38`: PDOL;
- otros parametros y plantillas BER-TLV.

El modo Apple Transit del relay exige exactamente un `84` primitivo que coincida
con el AID y exactamente un PDOL valido antes de permitir la reescritura GPO.

### 3.3 READ RECORD

Despues de GPO, el AFL indica los registros que debe leer el terminal. Una APDU
READ RECORD tiene forma similar a:

```text
00 B2 <record> <(SFI << 3) | 04> 00
```

Los registros pueden contener datos de aplicacion, certificados, CDOL y otros
objetos EMV. Que READ RECORD sea una lectura no significa que toda la sesion sea
repetible: comandos anteriores o posteriores pueden haber cambiado estado.

### 3.4 GENERATE AC

Cuando el flujo lo requiere, el terminal puede solicitar un criptograma:

```text
80 AE <tipo> 00 Lc <valores CDOL1> 00
```

Tipos solicitados por los bits altos de P1:

| Valor | Resultado solicitado |
|---|---|
| `00` | AAC, rechazo offline |
| `40` | TC, aprobacion offline |
| `80` | ARQC, solicitud de autorizacion online |

GENERATE AC puede avanzar ATC y estado de aplicacion. No debe repetirse tras un
timeout incierto.

## 4. Que es PDOL

PDOL significa **Processing Options Data Object List**. La aplicacion lo devuelve
normalmente en el tag `9F38` de su FCI SELECT.

PDOL no contiene los valores. Contiene una secuencia de pares:

```text
tag, longitud requerida
```

Ejemplo:

```text
9F6604
9F0206
9F0306
9F1A02
9505
5F2A02
9A03
9C01
9F3704
9F3501
9F3303
```

| Tag | Longitud | Significado |
|---|---:|---|
| `9F66` | 4 | Terminal Transaction Qualifiers, TTQ |
| `9F02` | 6 | Importe autorizado |
| `9F03` | 6 | Importe adicional |
| `9F1A` | 2 | Pais del terminal |
| `95` | 5 | Terminal Verification Results, TVR |
| `5F2A` | 2 | Moneda de transaccion |
| `9A` | 3 | Fecha de transaccion |
| `9C` | 1 | Tipo de transaccion |
| `9F37` | 4 | Numero impredecible |
| `9F35` | 1 | Tipo de terminal |
| `9F33` | 3 | Capacidades del terminal |

La suma es 37 bytes:

```text
4 + 6 + 6 + 2 + 5 + 2 + 3 + 1 + 4 + 1 + 3 = 37 = 0x25
```

Los offsets resultantes son:

| Tag | Offset | Longitud |
|---|---:|---:|
| `9F66` | 0 | 4 |
| `9F02` | 4 | 6 |
| `9F03` | 10 | 6 |
| `9F1A` | 16 | 2 |
| `95` | 18 | 5 |
| `5F2A` | 23 | 2 |
| `9A` | 25 | 3 |
| `9C` | 28 | 1 |
| `9F37` | 29 | 4 |
| `9F35` | 33 | 1 |
| `9F33` | 34 | 3 |

Los valores se concatenan exactamente en ese orden. Dentro del valor GPO `83` no
se vuelven a incluir los tags. Por eso no es correcto buscar los bytes `9F66` o
`9F35` dentro del GPO: normalmente no estan presentes alli.

## 5. Que es GPO

GPO significa **Get Processing Options**. Es una instruccion EMV con `INS=A8`.

La aplicacion ya seleccionada recibe datos y capacidades del terminal, decide la
ruta de procesamiento y devuelve las opciones necesarias para continuar.

Forma comun:

```text
80 A8 00 00 Lc 83 L <valores PDOL> 00
```

GPO puede influir en el estado de tarjeta o Wallet. No debe clasificarse
automaticamente como una consulta inocua o repetible.

### 5.1 Desglose byte a byte

Con el PDOL anterior, un GPO tiene esta cabecera:

```text
80 A8 00 00 27 83 25 <37 bytes> 00
```

| Campo | Valor | Explicacion |
|---|---|---|
| CLA | `80` | Clase propietaria usada por EMV |
| INS | `A8` | GET PROCESSING OPTIONS |
| P1/P2 | `0000` | Parametros GPO |
| Lc | `27` | 39 bytes de command data |
| Tag | `83` | Command Template |
| Longitud | `25` | 37 bytes de valores PDOL |
| Valor | 37 bytes | Valores concatenados sin tags |
| Le | `00` | Respuesta corta maxima |

La relacion de longitudes es:

```text
1 byte de tag 83
+ 1 byte de longitud 25
+ 37 bytes de valores
= 39 bytes = 0x27
```

### 5.2 Respuesta GPO

Una respuesta clasica contiene:

- **AIP**, Application Interchange Profile;
- **AFL**, Application File Locator.

AIP describe funciones de procesamiento disponibles. AFL indica que registros y
SFI debe leer el terminal.

Formato 1:

```text
80 <length> <2-byte AIP> <AFL> 9000
```

Formato 2:

```text
77 <length>
   82 02 <AIP>
   94 <length> <AFL>
   ...
9000
```

Algunos perfiles contactless pueden devolver tambien datos de transaccion o
criptograma. El parser debe respetar el formato realmente recibido y no asumir que
todo GPO contiene solo AIP y AFL.

## 6. GPO en el modo Apple Transit del relay

El modo transparente reenvia la C-APDU sin modificarla. El modo Apple Transit
aplica una politica estricta y modifica unicamente valores de perfil de terminal:

| Tag | Valor aplicado |
|---|---|
| `9F66` | `33804000` |
| `9F35` | `14` |
| `9F33` | `E00800` |

`9F66` es obligatorio y debe tener cuatro bytes. `9F35` y `9F33` son opcionales,
pero si el PDOL los pide deben tener longitudes de uno y tres bytes respectivamente.

Permanecen sin cambios:

- importe autorizado y adicional;
- moneda y pais;
- fecha y tipo de transaccion;
- TVR;
- numero impredecible;
- `Le`;
- cualquier otro valor PDOL;
- todas las R-APDU devueltas por la tarjeta.

Proceso:

```text
SELECT AID del terminal
          |
          v
Respuesta FCI exacta de la tarjeta
          |
          v
Validar 84, 9F38, orden, longitudes y duplicados
          |
          v
Guardar offsets PDOL para esa aplicacion
          |
          v
Recibir GPO corto con un unico tag 83
          |
          v
Verificar que la longitud 83 coincide con PDOL
          |
          v
Reemplazar solo 9F66, 9F35 y 9F33
          |
          v
Enviar la GPO resultante al backend
```

La politica falla cerrada y devuelve el fallback `6400` sin enviar GPO cuando
detecta, entre otros casos:

- SELECT AID o FCI mal formado;
- AID `84` ausente, duplicado o distinto;
- PDOL ausente, duplicado, excesivo o mal formado;
- tag de perfil relevante duplicado;
- `9F66` ausente o con longitud distinta de cuatro;
- `9F35` o `9F33` con longitud incorrecta;
- GPO extendido;
- GPO sin un unico tag `83` corto valido;
- longitud del valor `83` distinta de la longitud PDOL;
- GPO recibida sin un SELECT/PDOL actual valido.

El `6400` de ese caso es generado por el relay. No es una R-APDU obtenida de la
tarjeta, porque la GPO rechazada no se envia al backend.

## 7. Capas del relay autorizado

El flujo contiene tres protocolos distintos:

```text
Terminal EMV
    |
    | C-APDU, por ejemplo GPO
    v
Android HostApduService / Flutter
    |
    | trama Chameleon, comando 6012
    | data = session_id || C-APDU
    v
Firmware ChameleonUltra
    |
    | I/R/S-blocks ISO-DEP por NFC-A
    v
Tarjeta o Wallet backend
```

La respuesta recorre el camino inverso:

```text
Tarjeta
    |
    | R-APDU exacta
    v
Firmware 6012
    |
    | status de dispositivo + payload APDU
    v
Flutter
    |
    | Uint8List binario exacto
    v
Android sendResponseApdu()
    |
    v
Terminal
```

El comando Chameleon `6012` no es una APDU EMV. Es una operacion del protocolo
host-dispositivo que transporta una APDU dentro de su payload.

La sesion persistente no tiene timeout de inactividad. Queda ligada al transporte
USB o BLE que envio START; el otro transporte no puede enviar comandos mientras la
sesion esta activa. STOP/reset explicito, START de reemplazo del owner, fallo RF,
salida de reader mode u otra invalidacion segura, y perdida del enlace owner cierran
la sesion.

La distincion de estados tambien importa:

- el status del comando Chameleon describe firmware, parametros y RF;
- `SW1 SW2` dentro del payload describen la respuesta de la aplicacion de tarjeta;
- un timeout del host puede dejar incierto si la tarjeta proceso el comando;
- un S(WTX) solo describe espera ISO-DEP del backend.

## 8. Por que no se deben repetir o cachear APDU a ciegas

La secuencia EMV conserva estado en varias capas:

- aplicacion seleccionada;
- PDOL asociado al ultimo SELECT AID exitoso;
- numero de bloque ISO-DEP;
- ATC y otros contadores;
- criptogramas y nonces;
- estado interno de una Wallet;
- una unica APDU nativa pendiente en Android.

Una respuesta `6A82` repetida en una captura no demuestra que sea seguro responder
localmente sin consultar la tarjeta. El mismo comando puede producir otro resultado
despues de un SELECT, GPO, GENERATE AC, cambio de aplicacion o nueva activacion.

Tras timeout de escritura o respuesta no se reintenta la misma APDU. El transporte
puede haber terminado la escritura o la tarjeta puede haber avanzado aunque el host
no haya recibido la R-APDU.

Si solo la respuesta 6012 es incierta y framing/conexion siguen validos, el host
envia 6013 en orden. Una respuesta vacia con status `0068`, `0060` o `0066` confirma
el cierre y actua como barrera para limpiar solo la cuarentena de 6012; la misma
conexion se puede reutilizar. Se reconecta solo si ese reset no se confirma o si el
framing/write/transporte ya fue invalidado.

## 9. Que demuestra y que no demuestra `9000`

`9000` demuestra que esa instruccion fue aceptada y procesada segun el estado local
de la aplicacion. No demuestra:

- autorizacion del emisor;
- aceptacion del adquirente;
- clearing o settlement;
- aprobacion economica;
- autenticidad criptografica verificada por esta herramienta;
- que una animacion de Wallet equivalga a un pago completado.

Una GPO con `9000` solo permite continuar el flujo que indique su respuesta.

## 10. Ejemplos de APDU relacionadas

| Operacion | Ejemplo o plantilla |
|---|---|
| SELECT PPSE | `00 A4 04 00 0E 325041592E5359532E4444463031 00` |
| SELECT AID | `00 A4 04 00 Lc <AID> 00` |
| GPO | `80 A8 00 00 Lc 83 L <PDOL values> 00` |
| READ RECORD | `00 B2 <record> <SFI-control> 00` |
| GET DATA | `80 CA <tag high> <tag low> 00` |
| GET RESPONSE | `00 C0 00 00 <Le>` |
| GENERATE AC | `80 AE <type> 00 Lc <CDOL1 values> 00` |

## 11. Implementacion relacionada

Firmware:

- `firmware/application/src/rfid/reader/hf/iso_dep_reader.c`: bloques ISO-DEP,
  chaining, R(ACK/NAK), WTX, CRC y reensamblado.
- `firmware/application/src/rfid/reader/hf/iso_dep_session.c`: sesion persistente
  usada por 6011-6014.
- `firmware/application/src/rfid/reader/hf/emv_trace.c`: flujo EMV investigativo,
  PDOL, GPO, registros y evidencia retenida.

GUI:

- `lib/helpers/authorized_relay.dart`: parser BER-TLV, estado SELECT/PDOL y politica
  de reescritura GPO.
- `lib/gui/menu/hacking/authorized_relay_lab.dart`: rendezvous, deadlines,
  forwarding y cleanup fail-closed.
- `lib/bridge/authorized_relay_platform.dart`: APDU binaria entre Flutter y Android.
- `android/app/src/main/kotlin/io/chameleon/ultra/AuthorizedRelayHostApduService.kt`:
  APDU pendiente, arm token, deadline monotono y `sendResponseApdu()`.

Referencias complementarias:

- [Referencia ISO-DEP y APDU](apdu-command-reference.md)
- [Relay ISO-DEP autorizado](authorized-iso-dep-relay.md)
- [Datagramas y recuperacion del relay](authorized-relay-datagrams-es.md)
- [Internals del simulador EMV](emv-purchase-simulator.md)
