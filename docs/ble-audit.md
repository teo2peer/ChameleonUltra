# BLE audit tools

The ChameleonUltra's nRF52840 runs a BLE stack (Nordic SoftDevice **S140 v7.2.0**),
and this firmware exposes a set of **BLE security-research tools** on top of it, in
addition to the RFID/NFC features. They cover two jobs:

1. **Passive scanning** — listen for and inventory nearby BLE devices.
2. **Directed GATT auditing** — connect to **one** device you specify and read /
   subscribe / fuzz its GATT attributes.

They are reachable from the Python CLI (`ble` command group in
`software/script/`) and from the Flutter GUI (*Ethical Hacking → Bluetooth (BLE)
→ BLE audit*).

## Scope / design constraints

These tools are deliberately **receive-only or point-to-point against a single
operator-specified target**. This is enforced in the design, not just by policy:

- The passive scanner uses the SoftDevice **observer** role with `active=0` by
  default — it transmits **nothing**, it only receives advertisements devices
  already broadcast. (`ble scan --active` opts into standard active scanning,
  which sends scan-requests to collect scan-responses / full names — the normal
  BLE discovery exchange, not disruption.)
- The GATT tools (`connect`/`discover`/`read`/`subscribe`/`fuzz`/`ping`) operate
  over **one central connection to one address you provide**. Everything is
  scoped to that single connected target.

There is intentionally **no** broadcast/flood/jam capability, no "probe/attack all
nearby devices" batch mode, and no port of crash-exploit suites. The directed
fuzzer exercises a device you own/control; it is not a mass-disruption tool.

## CLI reference (`ble …`)

| Command | What it does |
|---|---|
| `ble scan [-t <sec>] [-v] [--active] [--min-rssi <dbm>] [--name <substr>] [--out <file.json>]` | Passive (default) or active scan. Lists address, type, RSSI, name and a decode of the advertising data (flags, service UUIDs with names, manufacturer/company, TX power, appearance). Sorts by RSSI; `-v` prints the full breakdown; `--out` exports JSON. |
| `ble connect -a <MAC> [--type <0-3>]` | Connect to one target. `--type`: 0 public, 1 random, 2 random-RPA, 3 random-NRPA. |
| `ble status` | Show connection / discovery / fuzz state. |
| `ble discover [--out <file.json>]` | Enumerate the target's GATT characteristics (handle, UUID + SIG name, properties), grouped under their primary services. |
| `ble descriptors` | List all GATT descriptors of the connected target (handle + UUID + name). |
| `ble read --handle <hex>` | Read a characteristic value from the target. |
| `ble write --handle <hex> --data <hex>` | Write a value to a characteristic (write-with-response; shows the target's ATT status). |
| `ble subscribe --handle <hex> [--cccd <hex>] [--indicate] [--off] [-t <sec>]` | Subscribe to notifications/indications and stream incoming values. The CCCD descriptor is auto-discovered (override with `--cccd`). |
| `ble fuzz --handle <hex> [-n <count>] [-i <ms>] [--out <file.json>]` | Write mutated payloads to a characteristic to exercise its input parsing, detecting when the target drops the link. Boundary-case corpus first, then random mutation. |
| `ble ping` | One-shot BLE link liveness probe of the connected target (a single connection-parameter-update round-trip). |
| `ble disconnect` | Disconnect from the target, freeing it to reconnect to its normal source. |
| `ble advertise [on\|off\|toggle\|status] [--erase-bonds]` | Control the device's **own** advertising (discoverable state). |

Typical directed-audit session:
```
hw connect
ble scan -t 8 --active
ble connect -a AA:BB:CC:DD:EE:FF
ble discover
ble read --handle 0x0012
ble subscribe --handle 0x0015 -t 10
ble fuzz --handle 0x0012 -n 500 -i 30 --out fuzz.json
ble disconnect
```

When a fuzz batch finishes (or is cancelled), the target is disconnected so it
can reconnect to its normal source. While a BLE test runs, the device shows an
**outside-to-center LED animation**.

## GUI

*Ethical Hacking → Bluetooth (BLE) → BLE audit* has two tabs:

- **Passive scan** — duration, active-scan toggle, name/RSSI filters, tap a
  device for a full advertising-data dialog, "Fuzz this" to target it.
- **Directed fuzz** — target address + type, Connect / Discover / Disconnect /
  Ping (single-target liveness). After Discover, characteristics are listed
  **grouped under their primary services**, each with Read / Write / Notify /
  Select actions; a header button lists **all descriptors** in a dialog. The
  live status panel shows connection / discovery / fuzz state, the negotiated
  **ATT MTU**, and link-probe result. Fuzz handle/count/interval with a log and
  copy/export; a Cancel button stops the batch; the AppBar has a
  local-advertising toggle. The target is released to reconnect when the batch
  ends.

  These GUI controls only appear once connected and after Discover — they are
  conditional on the central connection state.

## Protocol / command IDs

BLE commands occupy the **7000** block of the request/response command protocol
(see `firmware/application/src/data_cmd.h` ↔ `software/script/chameleon_enum.py`
↔ GUI `lib/helpers/definitions.dart`; kept in sync manually):

| ID range | Group |
|---|---|
| 7000–7003 | Passive scanner (start/stop/count/results) |
| 7004–7005 | Local advertising set/get |
| 7006 | Link probe |
| 7010–7017 | Central: connect, disconnect, state, discover, get-chars, fuzz start/stop/log |
| 7018–7019 | GATT read / get-read |
| 7020–7021 | Subscribe / get-notifications |
| 7022–7023 | Find CCCD / get-CCCD |
| 7024–7025 | GATT write / get-write-result |
| 7026 | Get effective ATT MTU |
| 7027–7028 | Descriptor discover / get |
| 7029–7030 | Primary-service discover / get |

There is no async push channel; continuous data (scan results, fuzz log,
notifications) is buffered in firmware and paged out by index by the host.

## Firmware notes

- `ble_main.c` — BLE peripheral (advertising, NUS command transport, battery,
  pairing) **and** the passive observer scanner.
- `ble_central.c` — the central-role harness (connect, primary-service /
  characteristic / descriptor discovery, CCCD lookup, read, write, subscribe,
  link probe, and the directed fuzzer). MTU is negotiated by `nrf_ble_gatt`
  (`nrf_ble_gatt_att_mtu_central_set` in `ble_main.c`'s `gatt_init`), so read /
  write / fuzz scale to the negotiated ATT MTU (up to ~244 bytes) instead of the
  23-byte default. Registers its own SoftDevice observer; the peripheral handler
  in `ble_main.c` is role/handle-guarded so it ignores the central link.
- Enabling the central role required `NRF_SDH_BLE_CENTRAL_LINK_COUNT=1` /
  `TOTAL=2` in `sdk_config.h` and a **RAM-origin bump** in `application.ld`. The
  origin there is an estimate — if `nrf_sdh_ble_enable` asserts `NRF_ERROR_NO_MEM`
  on first boot, the SoftDevice logs the exact required RAM start; set `ORIGIN`
  to it and shrink `LENGTH` accordingly.
- The outside-to-center LED effect is `rgb_marquee_ble_test_loop()`, driven from
  the main loop while `rgb_marquee_is_ble_test_anim()`; it is auto-enabled while a
  fuzz runs.
