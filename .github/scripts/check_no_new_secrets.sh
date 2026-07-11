#!/usr/bin/env bash

set -euo pipefail

repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_root"

allowed_dfu_files=$'resource/dfu_key/chameleon.pem\nresource/dfu_key/dfu_key.zip.bak\nresource/dfu_key/warning.txt'
tracked_dfu_files=$(git ls-files 'resource/dfu_key/*')
if [[ $tracked_dfu_files != "$allowed_dfu_files" ]]; then
  echo "Unexpected tracked file under resource/dfu_key; no new signing material may be committed." >&2
  diff -u <(printf '%s\n' "$allowed_dfu_files") <(printf '%s\n' "$tracked_dfu_files") || true
  exit 1
fi

if git grep -n -I -E \
  'BEGIN ([A-Z0-9]+ )?PRIVATE KEY|BEGIN OPENSSH PRIVATE KEY|AKIA[0-9A-Z]{16}' \
  -- . ':!resource/dfu_key/chameleon.pem' ':!resource/dfu_key/dfu_key.zip.bak' \
  ':!.github/scripts/check_no_new_secrets.sh'; then
  echo "Potential newly tracked private key or credential found." >&2
  exit 1
fi

echo "No new tracked private keys or high-confidence credentials found."
