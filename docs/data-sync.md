# GUI Data Sync

The GUI can merge data directly with another GUI installation or through an
encrypted `.cusync` file. Sync is local and does not require an account, cloud
service, or a connected Chameleon device.

## Included Data

- Saved cards and their complete dumps and metadata.
- Key dictionaries. Distinct keys are unioned and never discarded.
- All saved keyboard scripts, including source, layout, output, and precompiled
  bytecode.
- Safe GUI settings such as theme, locale, scan preferences, and confirmation
  behavior.

Debug logs, debug/emulator state, acknowledgements, transient navigation state,
BLE bonds, credentials, and physical Chameleon configuration are excluded.

## Direct Nearby Sync

Open **Settings > Data Sync** on both devices. Both devices must be on the same
Wi-Fi network, or one can provide a hotspot.

1. Select **Host nearby sync** on one app. It starts a foreground, one-client
   server and displays a one-time QR code.
2. Select **Join nearby sync** on the other app and scan the QR code. Desktop
   clients can paste the pairing code.
3. The joining app merges both snapshots. Unique records and dictionary keys
   merge automatically.
4. Resolve card, script, and setting conflicts. Card dumps provide a selector
   for every differing block. Script conflicts can keep either copy or both.
5. Send the result to the host. The host sees a final summary and must approve
   before either app applies the result.

The QR code contains a random 256-bit session key. Every framed message is
encrypted and authenticated with AES-256-GCM and a fresh nonce. The server
accepts one client and then closes. Pairing codes expire when hosting stops.

MIFARE Classic keys found in conflicting sector trailers are copied from both
card versions into the **Synced card keys** dictionary. The per-block choice
therefore does not discard the unselected trailer's keys.

## Encrypted Files

Select **Create and share file** to protect the current snapshot with a password
and open the operating system share sheet. The file can be sent through AirDrop,
Quick Share, Files, messaging, removable storage, or another trusted channel.

The receiver selects **Import sync file**, enters the password, reviews the same
merge/conflict screen, and applies the result. Files use PBKDF2-HMAC-SHA256 with
600,000 iterations and AES-256-GCM. A wrong password or modified file is
rejected before any data is changed.

## Merge Rules

- Exact duplicate cards and scripts are deduplicated.
- Independent card records are retained. Same-ID card changes require a
  metadata choice and per-block choices.
- Dictionaries with the same ID, or the same name and key width, are merged by
  key value. Every distinct key is retained.
- Same-ID script changes can keep local, remote, or both copies.
- Differing safe settings are selected individually.
- Deletions are not propagated. Sync is additive unless a conflict choice
  explicitly replaces the same record.

Imported data is validated against version, count, nesting, and 16 MiB aggregate
limits before merge. Legacy duplicate IDs are normalized deterministically so
older libraries remain syncable.
