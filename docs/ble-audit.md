# BLE audit tools

The ChameleonUltra's nRF52840 runs a BLE stack (Nordic SoftDevice **S140 v7.2.0**),
and this firmware exposes a set of **BLE security-research tools** on top of it, in
addition to the RFID/NFC features. They cover three jobs:

1. **Passive scanning** — listen for and inventory nearby BLE devices.
2. **Directed GATT auditing** — connect to **one** device you specify and read /
   subscribe / fuzz its GATT attributes.
3. **Stress / broadcast tooling** — operator-selected single-target,
   scan-buffer-wide, or environment-wide BLE stress commands for authorised lab
   use in this fork.

They are reachable from the Python CLI (`ble` command group in
`software/script/`) and from the Flutter GUI (*Ethical Hacking → Bluetooth (BLE)
→ BLE audit*).

## Scope / design constraints

Every BLE stress command takes explicit scope. There is no implicit downgrade or
upgrade between scopes:

- The passive scanner uses the SoftDevice **observer** role with `active=0` by
  default — it transmits **nothing**, it only receives advertisements devices
  already broadcast. (`ble scan --active` opts into standard active scanning,
  which sends scan-requests to collect scan-responses / full names — the normal
  BLE discovery exchange, not disruption.)
- The GATT tools (`connect`/`discover`/`read`/`subscribe`/`fuzz`/`ping`) operate
  over **one central connection to one address you provide**. Everything is
  scoped to that single connected target.
- `scope=single` stress commands require that central target link to already be
  connected. Single-target kick is one disconnect; repeated churn requires
  `scope=buffer`, which reconnects between cycles.
- `scope=buffer` stress commands use the latest passive-scan buffer, stop the
  scanner, filter to connectable peers, attempt them strongest-RSSI first, and
  skip peers that fail to connect instead of stalling the run.
- `scope=broadcast` uses non-connectable advertising payloads and does not need a
  central link or scan buffer.

The branch owner maintains this fork under the authorisation described in
`CLAUDE.md`; do not propagate these stress/broadcast tools to upstream without
that policy context.

## CLI reference (`ble …`)

| Command | What it does |
|---|---|
| `ble scan [-t <sec>] [-v] [--active] [--min-rssi <dbm>] [--name <substr>] [--out <file.json>]` | Passive (default) or active scan. Lists address, type, RSSI, name and a decode of the advertising data (flags, service UUIDs with names, manufacturer/company, TX power, appearance). Sorts by RSSI; `-v` prints the full breakdown; `--out` exports JSON. |
| `ble connect -a <MAC> [--type <0-3>]` | Connect to one target. `--type`: 0 public, 1 random, 2 random-RPA, 3 random-NRPA. |
| `ble status` | Show connection, discovery, fuzz, flood, read/write and notification state when supported by the firmware. |
| `ble discover [--out <file.json>]` | Enumerate the target's GATT characteristics (handle, UUID + SIG name, properties), grouped under their primary services. |
| `ble descriptors` | List all GATT descriptors of the connected target (handle + UUID + name). |
| `ble info` | Read the connected target's standard device information — Generic Access name/appearance, Device Information Service (manufacturer, model, serial, hardware/firmware/software revision, system ID, PnP ID) and battery level — via read-only GATT reads. Run after `ble discover`. |
| `ble read --handle <hex>` | Read a characteristic value from the target. |
| `ble write --handle <hex> --data <hex>` | Write a value to a characteristic (write-with-response; shows the target's ATT status). |
| `ble subscribe --handle <hex> [--cccd <hex>] [--indicate] [--off] [-t <sec>]` | Subscribe to notifications/indications and stream incoming values. The CCCD descriptor is auto-discovered (override with `--cccd`). |
| `ble fuzz --handle <hex> [-n <count>] [-i <ms>] [--out <file.json>]` | Write mutated payloads to a characteristic to exercise its input parsing, detecting when the target drops the link. Boundary-case corpus first, then random mutation. |
| `ble ping [--addr <MAC> --type <0-3>\|--all]` | Probe the current link by default, connect to and probe one address, or probe all connectable devices from the last scan. |
| `ble disconnect` | Disconnect from the target, freeing it to reconnect to its normal source. |
| `ble advertise [on\|off\|toggle\|status] [--erase-bonds]` | Control the device's **own** advertising (discoverable state). |
| `ble spoof-mac [show\|restore\|private\|nonresolv\|static <addr>]` | Change the device's **own** BLE GAP address/identity (static-random, random private resolvable/non-resolvable, or restore the FICR default). Mutates our radio only. |
| `ble radio [on\|off\|toggle\|status]` | Turn the device's **own** BLE radio on/off. `off` = stealth: stops advertising + passive scan and drops any active central link. |
| `ble flood-ping --scope single\|buffer\|broadcast ...` | WRITE_CMD flood for single/buffer scopes, or non-connectable advertising spam for broadcast scope (`--fill`, `--interval-units 1..102`). Buffer scope uses the scan buffer and defaults an omitted count to a bounded per-peer run. |
| `ble kick [cycles] --scope single\|buffer` | One disconnect for the current central link (`scope=single` requires `cycles=1`), or repeated connect/disconnect cycles for connectable peers in the scan buffer. |
| `ble broadcast [--fill hex] [--interval-units 1..102] [--stop]` | Environment-wide non-connectable advertising broadcast. |

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

*Ethical Hacking → Bluetooth (BLE)* is a three-tab hub: **Audit**, **Radio &
ID**, and **Stress & broadcast**. The Audit page has two inner tabs:

- **Passive scan** — duration, active-scan toggle, name/RSSI filters, tap a
  device for a full advertising-data dialog, "Fuzz this" to target it.
- **Directed fuzz** — target address + type, Connect / Discover / Disconnect /
  Ping (single-target or scan-buffer liveness). After Discover, characteristics are listed
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
| 7031–7032 | Device info: read standard GAP/DIS/battery fields / get |
| 7040–7043 | Own-radio address / radio power |
| 7044–7047 | Stress: flood start/stop/count, kick |
| 7050–7051 | Environment-wide advertising flood start/stop |

There is no async command-event push channel; continuous data (scan results,
fuzz log, notifications) is buffered in firmware and paged out by record index.
The CLI drains all pages and rejects malformed/truncated records.

The central-state response retains its original 12-byte prefix. Current firmware
appends flood state/count, read state, write state, and notification count, for a
21-byte response. Older 10/12-byte responses remain accepted by the clients.

## Reliability and current limits

- One peripheral host link and one central audit link can be active at once.
- Scanning and initiating use the 1M PHY and legacy 31-byte advertisements.
- One GATT client procedure can be active at a time; conflicting operations are
  rejected rather than silently replacing pending state.
- Explicit writes are limited to the negotiated `ATT_MTU - 3` and are never
  silently truncated. The fuzzer/flood payload remains limited to 20 bytes.
- Disconnects and GATT timeouts terminate pending discovery/read/write/CCCD/info
  operations, so host polling cannot remain pending forever.
- NUS command responses are copied to a bounded queue and advanced by
  `BLE_NUS_EVT_TX_RDY`; a connected client that has not enabled notifications can
  no longer trap the firmware in a busy loop.
- Responses are sent back over the transport that supplied the request. A USB
  connection no longer steals a response to a BLE NUS command.
- Full unknown 128-bit target UUIDs, long reads/reliable writes, central-role
  pairing, extended advertising, Coded PHY and multiple central links remain
  future work.

## Firmware notes

- `ble_main.c` — BLE peripheral (advertising, NUS command transport, battery,
  pairing), own identity/privacy and radio ownership.
- `ble_scan.c` — passive/active observer scan state, advertisement/scan-response
  merge, fixed result cache and connectable-target snapshots.
- `ble_central.c` — the central-role harness (connect, primary-service /
  characteristic / descriptor discovery, CCCD lookup, read, write, subscribe,
  link probe, the directed fuzzer, and scan-buffer-wide stress iteration). MTU is negotiated by `nrf_ble_gatt`
  (`nrf_ble_gatt_att_mtu_central_set` in `ble_main.c`'s `gatt_init`), so read /
  explicit write scales to the negotiated ATT MTU (up to 244 bytes) instead of
  the 20-byte default. Registers its own SoftDevice observer; the peripheral handler
  in `ble_main.c` is role/handle-guarded so it ignores the central link.
- Enabling the central role required `NRF_SDH_BLE_CENTRAL_LINK_COUNT=1` /
  `TOTAL=2` in `sdk_config.h` and a **RAM-origin bump** in `application.ld`. The
  origin there is an estimate — if `nrf_sdh_ble_enable` asserts `NRF_ERROR_NO_MEM`
  on first boot, the SoftDevice logs the exact required RAM start; set `ORIGIN`
  to it and shrink `LENGTH` accordingly.
- Application flash ends at `0xC7000`; `0xC7000–0xF3000` is reserved for the 22
  FDS virtual pages used by Peer Manager/settings, and the bootloader begins at
  `0xF3000`.
- The outside-to-center LED effect is `rgb_marquee_ble_test_loop()`, driven from
  the main loop while `rgb_marquee_is_ble_test_anim()`; it is auto-enabled while a
  fuzz runs.
