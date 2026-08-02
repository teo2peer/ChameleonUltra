# BLE Advertising Lab

The advertising lab creates bounded legacy Bluetooth Low Energy advertisements
for operator-authorised interoperability testing. It provides four tools:

1. A custom connectable or non-connectable profile builder.
2. A deterministic rotating-name simulator.
3. A raw legacy advertisement and scan-response AD-structure editor.
4. Apple proximity-pairing and Android Fast Pair discovery-sheet profiles.

The discovery-sheet profiles emulate only the public or reverse-engineered
advertisement. They cannot force a phone or computer to connect, and selecting
Connect cannot complete pairing: the Chameleon does not implement the accessory
GATT characteristics, anti-spoofing keys, BR/EDR flow, or account-key state.
Generic connectable mode exposes the Chameleon's existing NUS, battery, and HID
GATT services; it does not create arbitrary GATT services.

## GUI

Open **Ethical Hacking > Bluetooth > Advertising lab**. The page has three
editors:

- **Custom profile** builds flags, one optional 16-bit service UUID, optional
  16-bit Service Data, one optional operator-supplied company identifier and
  manufacturer value, and up to 32 names. Enter one name per line to enable
  rotation. Neutral connectable-accessory and sensor-beacon templates populate
  editable starting values without product identifiers.
- **Raw AD editor** accepts complete hexadecimal AD structures independently
  for the advertisement and scan response.
- **Pairing sheets** selects an Apple proximity-pairing accessory model or an
  Android Fast Pair discovery model. The generated packet remains visible in
  the preview and is bounded by the same duration/event controls.

Both editors support:

- Connectable and scannable advertising, with a minimum interval of 20 ms.
- Non-connectable scannable advertising, with a minimum interval of 100 ms.
- Non-connectable non-scannable advertising, with no scan response.
- An interval up to 10.24 seconds.
- A finite duration up to 655.35 seconds, or unlimited.
- A finite limit of 1-255 advertising events, or unlimited.
- Packet previews and independent `0/31` byte counts.
- Start, authoritative status refresh, and stop controls.

Start requires USB. Status and stop remain available over either command
transport. The nRF52840 SoftDevice provides one advertising set, so replacing
it from the BLE command connection would strand that control session. Starting
also rejects active scanning, central connections or
connection attempts, keyboard execution/arming, the older advertising flood,
and an existing peripheral connection.

## CLI

The command group is `ble adv-lab`:

```text
ble adv-lab start [options]
ble adv-lab status
ble adv-lab stop
```

Custom connectable profile:

```text
ble adv-lab start --mode connectable --name "Lab sensor" \
  --service-uuid 0x180F --company-id 0x1234 \
  --manufacturer-data "01 02 03" --interval-ms 250
```

Generic service-data sensor:

```text
ble adv-lab start --name "Lab Sensor" --service-uuid 0x181A \
  --service-data-uuid 0x181A --service-data "00 00" --interval-ms 500
```

Pairing discovery sheets:

```text
ble adv-lab start --pairing-profile apple --model-id 0x0E20 \
  --interval-ms 100 --duration-ms 10000
ble adv-lab start --pairing-profile android --model-id 0x2D7A23 \
  --interval-ms 100 --duration-ms 10000
```

Rotating names:

```text
ble adv-lab start --name "Lab A" --name "Lab B" --name "Lab C" \
  --name-target adv --interval-ms 250 --rotation-ms 2000
```

Raw complete AD structures:

```text
ble adv-lab start --raw --mode scannable \
  --adv "02 01 06 03 03 0F 18" \
  --scan-response "05 FF 34 12 01 02"
```

Relevant options:

| Option | Meaning |
|---|---|
| `--mode connectable` | Connectable and scannable |
| `--mode scannable` | Non-connectable and scannable |
| `--mode non-scannable` | Non-connectable and non-scannable |
| `--pairing-profile apple\|android` | Build a discovery-sheet advertisement |
| `--model-id VALUE` | Override the selected profile's model code/ID |
| `--name NAME` | Add a name; repeat to rotate |
| `--name-target adv\|scan` | Put generated names in the advertisement or scan response |
| `--flags VALUE` | GAP flags byte, default `0x06` |
| `--service-uuid VALUE` | Optional 16-bit service UUID |
| `--service-data-uuid VALUE` | Optional UUID for a 16-bit Service Data structure |
| `--service-data HEX` | Service Data value bytes |
| `--company-id VALUE` | Optional operator-supplied company identifier |
| `--manufacturer-data HEX` | Optional manufacturer bytes |
| `--interval-ms N` | Advertising interval |
| `--rotation-ms N` | Name rotation interval |
| `--duration-ms N` | `0` for unlimited, otherwise a 10 ms multiple |
| `--max-events N` | `0` for unlimited, otherwise `1-255` |

Company identifiers, service UUIDs, and values are never selected implicitly.
Use identifiers assigned to your organization or private lab values permitted by
your test environment.

## Raw AD Validation

Raw input is a sequence of Bluetooth AD structures:

```text
length:u8 | type:u8 | value[length - 1]
```

The host and firmware both enforce these rules:

- Each advertisement and scan response is at most 31 bytes.
- Every structure has a nonzero length and ends inside its packet.
- Flags type `0x01` is rejected in scan-response data.
- Duplicate local-name fields are rejected.
- Generated rotating names require the base packets to contain no local-name
  field and must fit every selected packet.
- Non-scannable mode rejects scan-response data.
- Names are 1-26 valid UTF-8 bytes without control characters.

The editor accepts raw AD structures, not malformed link-layer packets.

## Command Protocol

All command-level multibyte fields are big-endian. UUID and company identifier
bytes inside Bluetooth AD structures use Bluetooth little-endian order.

| ID | Name | Request | Response |
|---:|---|---|---|
| 7052 | `BLE_ADV_LAB_START` | Versioned configuration below | 20-byte status |
| 7053 | `BLE_ADV_LAB_STATUS` | Empty | 20-byte status |
| 7054 | `BLE_ADV_LAB_STOP` | Empty | 20-byte status |

### Start Request

```text
version:u8 = 1
profile:u8                 1 custom, 2 rotating names, 3 raw
mode:u8                    0 connectable, 1 scannable, 2 non-scannable
name_target:u8             0 none, 1 advertisement, 2 scan response
interval_units:u16 BE      0.625 ms units
rotation_ms:u16 BE         0 unless multiple names are supplied
duration_units:u16 BE      10 ms units; 0 means unlimited
max_adv_events:u8          0 means unlimited
adv_length:u8              0..31
scan_response_length:u8    0..31
name_count:u8              0..32
advertisement[adv_length]
scan_response[scan_response_length]
repeated names:
  name_length:u8
  name_utf8[name_length]
```

### Status Response

```text
version:u8 = 1
state:u8                   0 idle, 2 advertising, 3 connected, 4 error
profile:u8                 0 none, 1 custom, 2 rotating, 3 raw
mode:u8
last_reason:u8             0 none, 1 host stop, 2 duration,
                           3 event limit, 4 peer connected, 7 error
active_name_index:u8       0xFF when no generated name
name_count:u8
effective_adv_length:u8
effective_scan_length:u8
interval_units:u16 BE
rotation_ms:u16 BE
duration_units:u16 BE
max_adv_events:u8
rotation_count:u32 BE
```

There is no asynchronous push channel. Poll command 7053 for status. Name
rotation uses the application-timer RTC counter, but payload replacement occurs
in the main loop using alternating persistent SoftDevice buffers. Delayed main
loops skip directly to the name corresponding to elapsed rotation periods.

## Lifecycle

The lab temporarily owns the SoftDevice's single advertising handle. It stops
normal Chameleon advertising before start and remembers whether normal
advertising should be restored. Stop, duration expiry, event-limit termination,
radio shutdown, and the physical emergency-stop button clean up rotation state
and advertising buffers. A connectable advertisement ends when a peer connects;
normal advertising resumes after that peripheral link disconnects when it was
previously enabled.

Radio-off disconnects a peer that connected through a lab advertisement. The
radio-off command therefore requires USB so its response is delivered before
the asynchronous peripheral disconnect; status and lab stop remain available
over BLE.
