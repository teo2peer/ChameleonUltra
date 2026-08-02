# Autopwn / MIFARE Classic key-recovery optimizations

Speed and UX improvements to the MIFARE Classic key-recovery pipeline used by the
Flutter GUI (`ChameleonUltraGUI`), plus the one firmware change that supports it.
The recovery engine lives in `lib/helpers/mifare_classic/recovery.dart`
(`MifareClassicRecovery`); the pure ordering helpers are in
`lib/helpers/mifare_classic/candidate_priority.dart` and are unit-tested in
`test/candidate_priority_test.dart`.

**All of these are order-/speed-only.** Every key is still confirmed on-card
before it is trusted, so none of them can produce a wrong key or a wrong dump —
in the worst case they fall back to the previous behaviour.

## The pipeline

`scan → checkKeys() (dictionary) → recoverKeys() (attacks) → dumpData()`

`checkKeys` and `recoverKeys` both skip sectors already resolved, so the work
shrinks as keys are found.

## Dictionary check (`checkKeys`)

- **Bounded multi-sector prepass.** Firmware command
  `MF1_CHECK_KEYS_OF_SECTORS` (2012) now selects the card once and uses fast
  UID-pinned reselection between failed candidates. Hosts never send the old
  unbounded all-sector workload: Flutter limits each call to 16 authentication
  attempts over BLE or 48 over USB, while the Python CLI uses 48. The budget
  reserves the hardware-auth/readable-Key-B overhead. Every returned key is
  authenticated again before it is trusted, card identity is checked around
  each GUI batch, and unsupported firmware falls back to command 2015 per sector.
- **Dedup + build-once.** The candidate key list (selected dictionary + default
  keys) is de-duplicated and built a single time instead of being rebuilt per
  sector.
- **Likely keys first.** For large candidate lists, `checkKeysOnSector` reorders
  them so the keys most likely to hit are tested first — the default keys and any
  key already recovered on another sector (key reuse). Since the on-card check
  breaks on the first hit, this turns an average half-list scan into an early hit
  in the common case. (`prioritiseCandidates`)
- **Back-propagation.** When a key is found it is immediately tried on **every**
  still-unknown sector (`recheckKey`) through the same bounded masks, so a
  reused key resolves the whole card without one host round-trip per target.
- **Readable Key B confirmation.** When Key A opens a sector trailer and bytes
  10..15 expose a candidate Key B, Autopwn now authenticates with that candidate
  before saving or propagating it. This recovers a cheap missed key without
  trusting trailer bytes that may be configured as data.

## Weak-PRNG nested — candidate-set intersection

A single nonce pair fed to the nested solver yields on the order of ~26 000
candidate keys, which were previously all tested on-card one chunk at a time.
Instead, candidate sets from successive nonce collections are **intersected**:
the real key is present in every set, so two or three rounds shrink ~26 000
candidates to a handful before any on-card test. (`narrowCandidates`)

Safety: the intersection is never allowed to become empty — if a pair produces a
disjoint set it is kept instead of intersected, so the true key can't be dropped,
and the last try always tests whatever remains.

## NT distance measured once per card

The nested attack needs the card's PRNG "distance", which depends only on the
reference key/block (a card-level property), **not** on the target sector. It is
now measured once and reused across all sectors instead of once per sector,
saving ~N−1 device round-trips. If a sector fails, the distance is refreshed for
the next one to tolerate PRNG timing drift.

## RF08S backdoor (static-encrypted nested) — 3 phases

Static-encrypted nonces are fixed, so intersection can't help; the filtered
candidate list (often thousands per sector) must be brute-forced on-card. The
autopwn backdoor path uses a 3-phase flow tuned for both speed and a clean UI:

1. **Collect** every sector's filtered A/B candidate lists, showing a single
   global progress bar (blocks are not all flashed to "checking" at once), and
   skipping sectors already resolved by the dictionary pass.
2. **Prioritise** — count how often each candidate appears across sectors and put
   cross-sector duplicates + default keys first, most-frequent first
   (`prioritiseByFrequency`). A key shared across sectors (reuse) is far likelier
   to be the real key.
3. **Confirm** on-card per sector, with the normal per-block animation, using the
   `nt(A)==nt(B) ⇒ same key` shortcut and `findMatchingKeys` for the B key.

Nonce-list accesses are bounds-guarded, so an acquire that returns fewer nonces
than there are sectors can't crash the run.

For random, unique per-sector keys the bulk brute force is unavoidable (inherent
to RF08S — the same as the Proxmark/Doegox recovery); the win is on the common
real-world case of reused/default keys.

## Batched sector dump

`dumpData` reads each sector with a single authenticate-once `MF1_READ_BLOCKS`
(command 2018) instead of authenticating per block — roughly 6× fewer round-trips
on a 4K dump. Any block the batch doesn't return falls back to a per-block read
(key A then key B), so partial-access sectors still dump correctly.

## Dictionary picker

Before starting, Autopwn shows a dropdown (when saved dictionaries exist) to pick
which saved key dictionary to seed the run with. The default keys are always
tried on top, and "empty" reproduces the previous behaviour.

## Firmware: hardnested nonce batching

`MF1_HARDNESTED_ACQUIRE` now collects up to ~254 nonces per call (was 110). The
count is returned in a single leading byte (max 255), so the buffer is sized just
under that. Gathering the ~1400 nonces hardnested needs takes roughly half as many
host round-trips and card re-selections; the on-card per-nonce collection is
unchanged.
