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
DEFAULT_TC="/private/tmp/claude-501/-Users-teo-projects-Chamaleon-ChameleonUltra/5e9562ef-ee36-41c0-94cb-8d92012ae58a/scratchpad/armtc/bin"
TC="${ARM_TOOLCHAIN_BIN:-$DEFAULT_TC}"
if [ -x "$TC/arm-none-eabi-gcc" ]; then
  export GNU_INSTALL_ROOT="$TC/"
elif command -v arm-none-eabi-gcc >/dev/null 2>&1; then
  export GNU_INSTALL_ROOT="$(dirname "$(command -v arm-none-eabi-gcc)")/"
else
  echo "ERROR: arm-none-eabi-gcc not found. Set ARM_TOOLCHAIN_BIN to its bin dir." >&2
  exit 1
fi
export GNU_VERSION="$("${GNU_INSTALL_ROOT}arm-none-eabi-gcc" -dumpversion)"
export GNU_PREFIX="arm-none-eabi"
echo "==> Toolchain: ${GNU_INSTALL_ROOT} (gcc ${GNU_VERSION})"

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
sleep 1

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
nrfutil device program --firmware "$ZIP" --traits nordicDfu >/dev/null
echo "    programmed OK"

# --- Verify it rebooted and advertises commands (best-effort; never fatal) ---
set +e
sleep 3
PORT="$(nrfutil device list 2>/dev/null | grep -oE '/dev/tty.usbmodem[A-Za-z0-9]+' | head -1)"
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
