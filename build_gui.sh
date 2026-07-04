#!/usr/bin/env bash
#
# build_gui.sh — rebuild the ChameleonUltraGUI macOS desktop app and relaunch it.
#
#   ./build_gui.sh             # debug build + relaunch (default)
#   ./build_gui.sh --release   # release build + relaunch
#   ./build_gui.sh --no-run    # build only, don't relaunch
#
# The Flutter GUI project is the sibling repo ../ChameleonUltraGUI/chameleonultragui
# (override with env GUI_DIR).
#
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${GUI_DIR:-$SCRIPT_DIR/../ChameleonUltraGUI/chameleonultragui}"
if [ ! -d "$PROJECT_DIR" ]; then
  echo "ERROR: GUI project not found at $PROJECT_DIR (set GUI_DIR)" >&2
  exit 1
fi
PROJECT_DIR="$(cd "$PROJECT_DIR" && pwd)"

FLUTTER="$(command -v flutter || true)"
[ -x "$FLUTTER" ] || FLUTTER="/opt/homebrew/bin/flutter"
if [ ! -x "$FLUTTER" ]; then
  echo "ERROR: flutter not found in PATH nor /opt/homebrew/bin" >&2
  exit 1
fi

MODE=debug
RUN=1
for a in "$@"; do
  case "$a" in
    --release) MODE=release ;;
    --no-run)  RUN=0 ;;
    *) echo "Unknown option: $a" >&2; exit 1 ;;
  esac
done

cd "$PROJECT_DIR"
echo "==> flutter build macos --$MODE …"
"$FLUTTER" build macos --"$MODE"

if [ "$MODE" = release ]; then SUB=Release; else SUB=Debug; fi
APP="$PROJECT_DIR/build/macos/Build/Products/$SUB/Chameleon Ultra GUI.app"
echo "==> built: $APP"

if [ "$RUN" = 1 ]; then
  pkill -f "Chameleon Ultra GUI" 2>/dev/null || true
  sleep 1
  open "$APP"
  echo "==> relaunched"
fi
echo "==> Done."
