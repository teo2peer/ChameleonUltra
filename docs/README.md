# Chameleon Ultra Guide

The main docs have moved! [Wiki](https://github.com/RfidResearchGroup/ChameleonUltra/wiki)

Docs for features in this fork not yet covered by the wiki:

- [BLE audit tools](ble-audit.md) — passive BLE scanning and directed GATT
  auditing (discover / read / subscribe / write / fuzz), own-radio identity and
  power control, and operator-authorised stress/broadcast tooling with per-command
  scope (single target / scan-buffer-wide / environment-wide), plus command reference.
- [HF reader additions](hf-additions.md) — `hf mf rdsc` (fast sector read),
  `hf 14a field`, `hf des enum` (fast DESFire enumeration), and the extended
  `emv scan`.
- [Autopwn optimizations](autopwn-optimizations.md) — MIFARE Classic key-recovery
  speedups (nested candidate intersection, likely-key ordering, RF08S 3-phase
  backdoor, batched dump, hardnested nonce batching) and the dictionary picker.