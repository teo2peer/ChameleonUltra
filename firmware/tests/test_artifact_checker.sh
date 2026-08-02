#!/usr/bin/env bash

set -euo pipefail

repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
fixture=$(mktemp -d -t chameleon-artifact-check.XXXXXXXX)
marker="/tmp/chameleon-artifact-check-pwned-$$"
trap 'rm -rf "$fixture"; rm -f "$marker"' EXIT

write_fixture() {
  local application_origin=$1
  printf 'FLASH %s 0xA0000\nRAM 0x20003AE8 0x34518\n' "$application_origin" \
    > "$fixture/application.map"
  printf 'FLASH 0xF3000 0xB000\nRAM 0x20005978 0x32688\n' \
    > "$fixture/bootloader.map"
  printf 'text data bss dec hex filename\n1 1 1 3 3 application.out\n1 1 1 3 3 bootloader.out\n' \
    > "$fixture/firmware-size.txt"
}

write_fixture 0x27000
bash "$repo_root/.github/scripts/check_firmware_artifacts.sh" "$fixture" >/dev/null

malicious_origin='0x27000+a[$(touch${IFS}'"$marker"')]'
write_fixture "$malicious_origin"
if bash "$repo_root/.github/scripts/check_firmware_artifacts.sh" "$fixture" >/dev/null 2>&1; then
  printf 'Artifact checker accepted a malformed map origin.\n' >&2
  exit 1
fi
if [[ -e $marker ]]; then
  printf 'Artifact checker evaluated an untrusted map origin.\n' >&2
  exit 1
fi
