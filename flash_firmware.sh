#!/usr/bin/env bash
#
# flash_firmware.sh — build the ChameleonUltra application firmware and flash it
# over USB DFU (app-only, signed with the repo key). Reusable.
#
#   ./flash_firmware.sh
#
# Toolchain: uses arm-none-eabi-gcc found via ARM_TOOLCHAIN_BIN (a bin dir),
# else the session toolchain, else whatever is on PATH.
#
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

APP_DIR="$SCRIPT_DIR/firmware/application"
OBJ_DIR="$SCRIPT_DIR/firmware/objects"
KEY="$SCRIPT_DIR/resource/dfu_key/chameleon.pem"
ZIP="$OBJ_DIR/chameleon-dfu-app.zip"
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

# --- Build ---
echo "==> Building application…"
make -C "$APP_DIR" -j4 >/dev/null
echo "    application.hex: $(ls -lh "$OBJ_DIR/application.hex" | awk '{print $5}')"

# --- Package signed app-only DFU ---
echo "==> Packaging DFU…"
rm -f "$ZIP"
nrfutil nrf5sdk-tools pkg generate --hw-version 0 --key-file "$KEY" \
  --application "$OBJ_DIR/application.hex" --application-version 1 \
  --sd-req 0x0100 "$ZIP" >/dev/null
echo "    $(basename "$ZIP")"

# --- Free the serial port (close the GUI if it's holding it) ---
pkill -f "Chameleon Ultra GUI" 2>/dev/null || true
sleep 2  # give macOS time to release the serial port before we reopen it

# --- Enter the DFU bootloader via the client (raw enter_dfu.py is flaky on macOS) ---
echo "==> Entering DFU bootloader…"
"$PYBIN" - "$SCRIPT_DIR" <<'PY'
import sys
sys.path.insert(0, sys.argv[1] + "/software/script")
import serial.tools.list_ports as lp
from chameleon_com import ChameleonCom
from chameleon_cmd import ChameleonCMD
port = next((c.device for c in lp.comports()
             if c.vid == 0x6868 and c.pid == 0x8686), None)
if port is None:
    print("   (device not in app mode — assuming already in DFU)")
    sys.exit(0)
com = ChameleonCom()
com.open(port)
ChameleonCMD(com).enter_bootloader()
print("   enter_bootloader sent to", port)
PY

# --- Wait for the bootloader, then program ---
echo "==> Waiting for DFU bootloader…"
ok=0
for i in $(seq 1 15); do
  if nrfutil device list 2>/dev/null | grep -q nordicDfu; then
    ok=1
    echo "    ready (${i}s)"
    break
  fi
  sleep 1
done
[ "$ok" = 1 ] || { echo "ERROR: DFU bootloader not detected." >&2; exit 1; }

echo "==> Programming…"
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
