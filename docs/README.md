# Chameleon Ultra Guide

The main docs have moved! [Wiki](https://github.com/RfidResearchGroup/ChameleonUltra/wiki)

Docs for features in this fork not yet covered by the wiki:

- [Deep analysis del firmware](deep%20analysis/README.md) - auditoría completa de
  rendimiento, memoria, concurrencia, RF, BLE/USB, FDS, boot/DFU, seguridad,
  reutilización, firmas y plan de optimización Ultra/Lite.
- [Referencia completa de comandos](protocol-command-reference.md) - todos los
  IDs 1000-7999 definidos por el firmware, payloads, respuestas, estados,
  disponibilidad Ultra/Lite, persistencia y rangos no asignados.
- [Atomic active-slot snapshots](active-slot-snapshot.md) - command 1050 v2,
  stable loaded-owner generation, transport ownership, MIFARE freeze/read
  allowlist, bounded FDS commit timing, and durable GUI monitor history.
- [BLE audit tools](ble-audit.md) — passive BLE scanning and directed GATT
  auditing (discover / read / subscribe / write / fuzz), own-radio identity and
  power control, and operator-authorised stress/broadcast tooling with per-command
  scope (single target / scan-buffer-wide / environment-wide), plus command reference.
- [BLE advertising lab](ble-advertising-lab.md) - connectable profiles, rotating
  names, pairing-discovery sheets, and a validated legacy AD/scan-response editor.
- [HF reader additions](hf-additions.md) — `hf mf rdsc` (fast sector read),
  `hf 14a field`, `hf des enum` (fast DESFire enumeration), and the extended
  `emv scan`.
- [Keyboard payloads](keyboard-payloads.md) - RAM-only Ducky-style scripts with
  USB/BLE upload and run control plus USB/BLE HID keyboard output.
- [Keyboard command reference](keyboard-command-reference.md) - complete
  DuckyScript syntax, keys, modifiers, CLI, GUI, protocol, states, and limits.
- [Authorized ISO-DEP payment relay lab](authorized-iso-dep-relay.md) - complete
  real-card relay setup, Android default-payment configuration, operator workflow,
  HCE and firmware protocols, session state, WTX/timing, errors, and diagnostics.
- [Relay datagrams and recovery, Spanish](authorized-relay-datagrams-es.md) -
  byte-level NFC-A, ISO-DEP, Chameleon, HCE and EMV reference plus a complete
  failure, slow-path and safe-resumption runbook.
- [EMV purchase simulator internals](emv-purchase-simulator.md) - ECP, activation,
  PPSE/AID selection, PDOL/GPO, records, cryptograms, and retained evidence.
- [Guia de APDU, GPO y PDOL](apdu-gpo-guide-es.md) - explicacion conceptual en
  espanol, estructura byte a byte, ISO-DEP/WTX, flujo EMV y capas del relay.
- [APDU command reference](apdu-command-reference.md) - APDU format, common EMV and
  private-lab commands, status words, tags, and command IDs 6000-6014.
- [GUI data sync](data-sync.md) - encrypted direct app-to-app synchronization and
  password-protected bundles for cards, keys, scripts, and safe GUI settings.
