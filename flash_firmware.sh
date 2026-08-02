#!/usr/bin/env bash
#
# flash_firmware.sh — build ChameleonUltra firmware and flash it over USB DFU.
# Supports a full bootloader migration and acknowledged app-only updates.
#
#   ./flash_firmware.sh --full  # first FDS-aware bootloader migration
#   ./flash_firmware.sh         # app-only update (default once migrated)
#   ./flash_firmware.sh --package-only  # build/sign app DFU without flashing
#
# Zero-config: signing key, app version, and the app-only acknowledgement all
# default automatically (see ~/.chameleon below), so a bare invocation works once
# the device is migrated. Any of DFU_SIGNING_KEY / APPLICATION_VERSION /
# ALLOW_APP_ONLY_DFU you export still override the defaults.
#   ~/.chameleon/chameleon.pem  external signing key (DFU_SIGNING_KEY default)
#   ~/.chameleon/migrated       created by --full; unlocks bare --app
#   ~/.chameleon/last_version   monotonic DFU app-version high-water mark
#   ~/.chameleon/flash.env      optional, sourced for overrides
#
# Toolchain: uses arm-none-eabi-gcc found via ARM_TOOLCHAIN_BIN (a bin dir),
# else the session toolchain, else whatever is on PATH.
#
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# --- Zero-config flashing state (this fork) ------------------------------------
# Optional per-user config (never committed) may preset DFU_SIGNING_KEY,
# APPLICATION_VERSION, CHAMELEON_STATE_DIR, etc. Everything below has a safe
# default, so once a device is migrated a bare `./flash_firmware.sh` just works.
CHAMELEON_FLASH_CONFIG="${CHAMELEON_FLASH_CONFIG:-$HOME/.chameleon/flash.env}"
# shellcheck disable=SC1090
[ -r "$CHAMELEON_FLASH_CONFIG" ] && . "$CHAMELEON_FLASH_CONFIG"
STATE_DIR="${CHAMELEON_STATE_DIR:-$HOME/.chameleon}"
VERSION_STATE="$STATE_DIR/last_version"   # highest DFU app-version used so far
MIGRATED_MARKER="$STATE_DIR/migrated"     # created by --full; unlocks bare --app

# DFU app-version base = MAJOR*1e9 + MINOR*1e6 + PATCH*1e3 + git-distance
# (identical to firmware/build.sh's derive_version).
git_base_version() {
  local tag distance maj mnr pat
  tag="$(git describe --tags --abbrev=0 --match 'v[0-9]*' 2>/dev/null || echo v0.0.0)"
  distance="$(git rev-list --count "${tag}..HEAD" 2>/dev/null || echo 0)"
  if [[ "$tag" =~ ^v?([0-9]+)\.([0-9]+)(\.([0-9]+))?$ ]]; then
    maj=${BASH_REMATCH[1]}; mnr=${BASH_REMATCH[2]}; pat=${BASH_REMATCH[4]:-0}
  else
    maj=0; mnr=0; pat=0
  fi
  printf '%s\n' "$(( maj*1000000000 + mnr*1000000 + pat*1000 + distance ))"
}

# Monotonic version: the git base, but never <= the last version we used — so
# repeated flashes without new commits still strictly increase (the bootloader
# rejects equal versions). Recorded before flashing so a macOS "false failure"
# can't reuse a number the device may already have accepted.
derive_app_version() {
  local base last next
  base="$(git_base_version)"
  last="$(cat "$VERSION_STATE" 2>/dev/null || echo 0)"
  next=$base
  (( next <= last )) && next=$(( last + 1 ))
  mkdir -p "$STATE_DIR"
  printf '%s\n' "$next" > "$VERSION_STATE"
  printf '%s\n' "$next"
}

FLASH_MODE="app"
PACKAGE_ONLY=0
case "${1:-}" in
  ""|--app) FLASH_MODE="app" ;;
  --package-only) FLASH_MODE="app"; PACKAGE_ONLY=1 ;;
  --full|--migrate) FLASH_MODE="full" ;;
  *)
    echo "Usage: $0 [--full|--app|--package-only]" >&2
    exit 2
    ;;
esac

APP_DIR="$SCRIPT_DIR/firmware/application"
OBJ_DIR="$SCRIPT_DIR/firmware/objects"
KEY="${DFU_SIGNING_KEY:-$STATE_DIR/chameleon.pem}"
if [ "$FLASH_MODE" = "full" ]; then
  ZIP="$OBJ_DIR/ultra-dfu-full.zip"
else
  ZIP="$OBJ_DIR/chameleon-dfu-app.zip"
fi
PYBIN="$SCRIPT_DIR/software/script/venv/bin/python3"
[ -x "$PYBIN" ] || PYBIN="$(command -v python3)"

# --- ARM toolchain resolution ---
# We need arm-none-eabi-gcc AND binutils (objcopy/size/ld). Those may live in
# one self-contained dir, or — with a Homebrew install — in two separate
# keg-only dirs (arm-none-eabi-gcc@8 + arm-none-eabi-binutils, neither on PATH).
# Put every candidate dir on PATH and leave GNU_INSTALL_ROOT empty so the
# Makefile resolves each tool via PATH, which works for both layouts.
[ -n "${ARM_TOOLCHAIN_BIN:-}" ] && [ -d "$ARM_TOOLCHAIN_BIN" ] && PATH="$ARM_TOOLCHAIN_BIN:$PATH"
for _keg in arm-none-eabi-gcc@8 arm-none-eabi-gcc arm-none-eabi-binutils; do
  _pref="$(brew --prefix "$_keg" 2>/dev/null)"
  [ -n "$_pref" ] && [ -d "$_pref/bin" ] && PATH="$_pref/bin:$PATH"
done
export PATH
if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
  echo "ERROR: arm-none-eabi-gcc not found. Set ARM_TOOLCHAIN_BIN to its bin dir," >&2
  echo "       or install it: brew install arm-none-eabi-gcc arm-none-eabi-binutils" >&2
  exit 1
fi
if ! command -v arm-none-eabi-objcopy >/dev/null 2>&1; then
  echo "ERROR: arm-none-eabi-objcopy not found — install arm-none-eabi-binutils" >&2
  echo "       (Homebrew keeps it in a separate keg from the gcc formula)." >&2
  exit 1
fi
export GNU_INSTALL_ROOT=""       # empty -> Makefile resolves arm-none-eabi-* via PATH
export GNU_VERSION="$(arm-none-eabi-gcc -dumpversion)"
export GNU_PREFIX="arm-none-eabi"
echo "==> Toolchain: $(command -v arm-none-eabi-gcc) (gcc ${GNU_VERSION})"

# App-only DFU is only safe on a device already carrying the enlarged FDS-aware
# bootloader. `--full` records that migration in $MIGRATED_MARKER; afterwards a
# bare `--app` is unlocked automatically. ALLOW_APP_ONLY_DFU=1 still forces it.
if [ "$FLASH_MODE" = "app" ] && [ "${ALLOW_APP_ONLY_DFU:-0}" != "1" ]; then
  if [ -f "$MIGRATED_MARKER" ]; then
    ALLOW_APP_ONLY_DFU=1
  else
    echo "ERROR: app-only DFU is disabled until the enlarged FDS-aware bootloader is installed." >&2
    echo "       Run '$0 --full' once to migrate (it records $MIGRATED_MARKER)," >&2
    echo "       or 'touch $MIGRATED_MARKER' if this device is already migrated." >&2
    exit 1
  fi
fi
if [ -z "$KEY" ] || [ ! -r "$KEY" ]; then
  echo "ERROR: set DFU_SIGNING_KEY to the external private key accepted by the bootloader." >&2
  exit 1
fi
export HW_VERSION=0
export DFU_SIGNING_KEY="$KEY"

if [ "$FLASH_MODE" = "full" ]; then
  echo "==> Building and packaging full FDS-aware migration DFU…"
  # The full package also needs a strictly-increasing DFU version, or the
  # bootloader rejects a re-flash with FwVersionFailure when no new commit has
  # bumped the git-derived base. Reuse the app-only path's monotonic high-water
  # mark so repeated `--full` runs always increase past what the device holds.
  if [ -z "${APPLICATION_VERSION:-}" ]; then
    APPLICATION_VERSION="$(derive_app_version)"
    echo "==> Auto APPLICATION_VERSION=$APPLICATION_VERSION (export APPLICATION_VERSION to override)"
  fi
  export APPLICATION_VERSION
  export BOOTLOADER_VERSION="${BOOTLOADER_VERSION:-$APPLICATION_VERSION}"
  CURRENT_DEVICE_TYPE=ultra "$SCRIPT_DIR/firmware/build.sh" package
else
  if [ -z "${APPLICATION_VERSION:-}" ]; then
    APPLICATION_VERSION="$(derive_app_version)"
    echo "==> Auto APPLICATION_VERSION=$APPLICATION_VERSION (export APPLICATION_VERSION to override)"
  fi
  if ! [[ "${APPLICATION_VERSION:-}" =~ ^[1-9][0-9]*$ ]]; then
    echo "ERROR: set APPLICATION_VERSION to a monotonic positive integer." >&2
    exit 1
  fi
  # Export so the validate/package subprocesses see it (auto-derived values are
  # otherwise plain shell vars; env-provided ones are already exported).
  export APPLICATION_VERSION
  export BOOTLOADER_VERSION="${BOOTLOADER_VERSION:-$APPLICATION_VERSION}"
  "$SCRIPT_DIR/.github/scripts/validate_firmware_release.sh" --device ultra --package

  echo "==> Building application…"
  make -C "$APP_DIR" -j4 >/dev/null
  echo "    application.hex: $(ls -lh "$OBJ_DIR/application.hex" | awk '{print $5}')"

  echo "==> Packaging app-only DFU…"
  rm -f "$ZIP"
  nrfutil nrf5sdk-tools pkg generate --hw-version 0 --key-file "$KEY" \
    --application "$OBJ_DIR/application.hex" --application-version "$APPLICATION_VERSION" \
    --sd-req 0x0100 "$ZIP" >/dev/null
fi
[ -r "$ZIP" ] || { echo "ERROR: DFU package was not created: $ZIP" >&2; exit 1; }
echo "    $(basename "$ZIP")"
if [ "$PACKAGE_ONLY" = 1 ]; then
  echo "==> Done. Signed app-only DFU package prepared without flashing."
  exit 0
fi

# --- Free the serial port (close the GUI if it's holding it) ---
pkill -f "Chameleon Ultra GUI" 2>/dev/null || true
sleep 2  # give macOS time to release the serial port before we reopen it

# --- Enter the DFU bootloader ---
# Preferred path: ask the running app to reboot into DFU (GPREGRET buttonless
# trigger; the raw enter_dfu.py DTR method is flaky on macOS). This requires the
# app to be alive — a boot-looping/unresponsive device can only reach DFU via the
# hardware button, so we also poll for a manual entry and guide it below.
echo "==> Entering DFU bootloader…"

dfu_present() { nrfutil device list 2>/dev/null | grep -q nordicDfu; }

if dfu_present; then
  echo "    already in DFU bootloader"
else
  # The CDC port may take a moment to appear (or reappear if the device is
  # resetting), so retry the buttonless trigger a few times.
  for _try in 1 2 3; do
    "$PYBIN" - "$SCRIPT_DIR" <<'PY' || true
import sys
sys.path.insert(0, sys.argv[1] + "/software/script")
import serial.tools.list_ports as lp
from chameleon_com import ChameleonCom
from chameleon_cmd import ChameleonCMD
port = next((c.device for c in lp.comports()
             if c.vid == 0x6868 and c.pid == 0x8686), None)
if port is None:
    print("   (no app-mode device this attempt)")
    sys.exit(0)
try:
    com = ChameleonCom()
    com.open(port)
    ChameleonCMD(com).enter_bootloader()
    # enter_bootloader queues an asynchronous write; don't exit before the
    # serial worker has actually sent the frame.
    com.send_data_queue.join()
    print("   enter_bootloader sent to", port)
except Exception as exc:
    print("   enter_bootloader attempt failed:", exc)
PY
    sleep 2
    if dfu_present; then break; fi
  done
fi

# --- Wait for the bootloader (long window so a manual DFU is picked up too) ---
echo "==> Waiting for DFU bootloader…"
ok=0
manual_hinted=0
for i in $(seq 1 45); do
  if dfu_present; then
    ok=1
    echo "    ready (${i}s)"
    break
  fi
  # If nothing shows after ~10s the app probably is not rebooting (e.g. a boot
  # loop): guide the hardware DFU. The loop keeps polling, so the bootloader is
  # picked up automatically as soon as the user does it — no re-run needed.
  if [ "$i" -ge 10 ] && [ "$manual_hinted" = 0 ]; then
    manual_hinted=1
    echo "    no reboot yet — if the device is unresponsive, force DFU by hardware:" >&2
    echo "      unplug USB, hold button B, plug USB back in, then release button B." >&2
    echo "      (waiting up to 45s; the DFU bootloader is detected automatically)" >&2
  fi
  sleep 1
done
[ "$ok" = 1 ] || {
  echo "ERROR: DFU bootloader not detected." >&2
  echo "       Force DFU by hardware: unplug USB, hold button B, replug USB, release —" >&2
  echo "       then re-run $0. Also disconnect any BLE client (disable phone Bluetooth) first." >&2
  exit 1
}

echo "==> Programming $FLASH_MODE DFU…"
# Retry: a transient port contention (e.g. the GUI still holding the port) can
# fail one attempt but leaves the device in DFU, so re-programming succeeds.
prog_ok=0
for attempt in 1 2 3; do
  if nrfutil device program --firmware "$ZIP" --traits nordicDfu >/dev/null 2>&1; then
    prog_ok=1
    break
  fi
  echo "    program attempt $attempt failed — retrying in 2s…"
  sleep 2
done
if [ "$prog_ok" != 1 ]; then
  echo "ERROR: programming failed after 3 attempts. The device is likely still" >&2
  echo "       in the DFU bootloader — just re-run ./flash_firmware.sh." >&2
  exit 1
fi
echo "    programmed OK"

# A successful --full leaves the device on the FDS-aware bootloader: record that
# so future bare `--app` runs are unlocked, and seed the version state so the
# next auto app-version is strictly above what this migration just wrote.
if [ "$FLASH_MODE" = "full" ]; then
  mkdir -p "$STATE_DIR"
  touch "$MIGRATED_MARKER"
  # Keep the monotonic high-water mark at/above the version just flashed so the
  # next flash strictly increases (never lower it back to the git base).
  cur="$(cat "$VERSION_STATE" 2>/dev/null || echo 0)"
  if (( APPLICATION_VERSION > cur )); then printf '%s\n' "$APPLICATION_VERSION" > "$VERSION_STATE"; fi
  echo "    migration recorded ($MIGRATED_MARKER) — app-only DFU now enabled"
fi

# --- Verify it rebooted and advertises commands (best-effort; never fatal) ---
set +e
PORT=""
for _v in 1 2 3 4 5 6; do
  sleep 1
  PORT="$(nrfutil device list 2>/dev/null | grep -oE '/dev/tty.usbmodem[A-Za-z0-9]+' | head -1)"
  [ -n "$PORT" ] && break
done
echo "==> Verifying (${PORT:-?})…"
if [ -n "$PORT" ]; then
  "$PYBIN" - "$SCRIPT_DIR" "$PORT" <<'PY'
import sys
sys.path.insert(0, sys.argv[1] + "/software/script")
from chameleon_com import ChameleonCom
from chameleon_cmd import ChameleonCMD
com = ChameleonCom()
com.open(sys.argv[2])
ids = set(ChameleonCMD(com).get_device_capabilities())
com.close()
print(f"   {len(ids)} commands advertised")
PY
fi
set -e
echo "==> Done. Firmware flashed successfully."
