#!/usr/bin/env bash
#
# Build the ChameleonUltraGUI Android APK, install it on a connected device,
# and launch it. The Flutter GUI is expected in the sibling repository
# ../ChameleonUltraGUI/chameleonultragui (override with GUI_DIR).
#
# Usage:
#   ./build_apk.sh
#   ./build_apk.sh --debug
#   ./build_apk.sh --device SERIAL
#   ./build_apk.sh --clean-install
#   ./build_apk.sh --no-install
#
set -euo pipefail

usage() {
  printf '%s\n' \
    'Usage: ./build_apk.sh [options]' \
    '' \
    'Options:' \
    '  --release          Build a release APK (default).' \
    '  --debug            Build a debug APK for troubleshooting.' \
    '  --device SERIAL    Install on this adb device.' \
    '  --clean-install    Uninstall the existing app first (deletes app data).' \
    '  --no-install       Build only.' \
    '  --no-launch        Install without launching the app.' \
    '  -h, --help         Show this help.' \
    '' \
    'Environment:' \
    '  GUI_DIR            Path to the Flutter project.' \
    '  DEVICE_ID          Default adb device serial.' \
    '  ADB                Absolute path to adb.' \
    '  PACKAGE_ID         Android package to launch/uninstall.'
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${GUI_DIR:-$SCRIPT_DIR/../ChameleonUltraGUI/chameleonultragui}"
PACKAGE_ID="${PACKAGE_ID:-io.chameleon.ultra}"
MODE=release
INSTALL=1
LAUNCH=1
CLEAN_INSTALL=0
DEVICE="${DEVICE_ID:-}"

while [ "$#" -gt 0 ]; do
  case "$1" in
    --release)
      MODE=release
      ;;
    --debug)
      MODE=debug
      ;;
    --device)
      if [ "$#" -lt 2 ] || [ -z "$2" ]; then
        printf 'ERROR: --device requires an adb serial.\n' >&2
        exit 2
      fi
      DEVICE="$2"
      shift
      ;;
    --clean-install)
      CLEAN_INSTALL=1
      ;;
    --no-install)
      INSTALL=0
      LAUNCH=0
      ;;
    --no-launch)
      LAUNCH=0
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      printf 'ERROR: unknown option: %s\n' "$1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

if [ ! -d "$PROJECT_DIR" ]; then
  printf 'ERROR: GUI project not found at %s\n' "$PROJECT_DIR" >&2
  printf '       Set GUI_DIR=/path/to/chameleonultragui and retry.\n' >&2
  exit 1
fi
PROJECT_DIR="$(cd "$PROJECT_DIR" && pwd)"

FLUTTER="$(command -v flutter || true)"
[ -x "$FLUTTER" ] || FLUTTER="/opt/homebrew/bin/flutter"
if [ ! -x "$FLUTTER" ]; then
  printf 'ERROR: flutter not found in PATH or /opt/homebrew/bin/flutter.\n' >&2
  exit 1
fi

ADB_BIN="${ADB:-$(command -v adb || true)}"
if [ "$INSTALL" -eq 1 ] && [ ! -x "$ADB_BIN" ]; then
  for candidate in \
    "${ANDROID_SDK_ROOT:-}/platform-tools/adb" \
    "${ANDROID_HOME:-}/platform-tools/adb" \
    "$HOME/Library/Android/sdk/platform-tools/adb"; do
    if [ -x "$candidate" ]; then
      ADB_BIN="$candidate"
      break
    fi
  done
fi
if [ "$INSTALL" -eq 1 ] && [ ! -x "$ADB_BIN" ]; then
  printf 'ERROR: adb not found. Install Android platform-tools or set ADB.\n' >&2
  exit 1
fi

if [ "$INSTALL" -eq 1 ]; then
  "$ADB_BIN" start-server >/dev/null
  if [ -z "$DEVICE" ]; then
    DEVICES="$("$ADB_BIN" devices | awk 'NR > 1 && $2 == "device" {print $1}')"
    DEVICE_COUNT="$(printf '%s\n' "$DEVICES" | awk 'NF {count++} END {print count + 0}')"
    case "$DEVICE_COUNT" in
      0)
        printf 'ERROR: no authorised Android device is connected.\n' >&2
        printf '       Enable USB debugging, accept the RSA prompt, then run again.\n' >&2
        "$ADB_BIN" devices -l >&2
        exit 1
        ;;
      1)
        DEVICE="$(printf '%s\n' "$DEVICES" | awk 'NF {print; exit}')"
        ;;
      *)
        printf 'ERROR: multiple Android devices are connected:\n%s\n' "$DEVICES" >&2
        printf '       Select one with --device SERIAL or DEVICE_ID=SERIAL.\n' >&2
        exit 1
        ;;
    esac
  fi

  DEVICE_STATE="$("$ADB_BIN" -s "$DEVICE" get-state 2>/dev/null || true)"
  if [ "$DEVICE_STATE" != device ]; then
    printf 'ERROR: adb device %s is not ready (state: %s).\n' \
      "$DEVICE" "${DEVICE_STATE:-unknown}" >&2
    exit 1
  fi
  printf '==> Android device: %s\n' "$DEVICE"
fi

cd "$PROJECT_DIR"
printf '==> Building Android APK (%s)...\n' "$MODE"
"$FLUTTER" build apk --"$MODE"

APK="$PROJECT_DIR/build/app/outputs/flutter-apk/app-$MODE.apk"
if [ ! -f "$APK" ]; then
  printf 'ERROR: expected APK was not generated: %s\n' "$APK" >&2
  exit 1
fi
printf '==> APK built: %s\n' "$APK"

if [ "$INSTALL" -eq 1 ]; then
  if [ "$CLEAN_INSTALL" -eq 1 ]; then
    printf '==> Removing existing %s (app data will be deleted)...\n' "$PACKAGE_ID"
    "$ADB_BIN" -s "$DEVICE" uninstall "$PACKAGE_ID" >/dev/null 2>&1 || true
  fi

  printf '==> Installing APK...\n'
  if ! "$ADB_BIN" -s "$DEVICE" install -r -d "$APK"; then
    printf 'ERROR: APK installation failed.\n' >&2
    printf '       If signatures differ, retry with --clean-install.\n' >&2
    exit 1
  fi
  printf '==> Installed on %s.\n' "$DEVICE"

  if [ "$LAUNCH" -eq 1 ]; then
    printf '==> Launching %s...\n' "$PACKAGE_ID"
    if ! "$ADB_BIN" -s "$DEVICE" shell monkey \
      -p "$PACKAGE_ID" -c android.intent.category.LAUNCHER 1 >/dev/null; then
      printf 'WARNING: installed successfully, but automatic launch failed.\n' >&2
    fi
  fi
fi

printf '==> Done.\n'
