#!/usr/bin/env bash
#
# build_ipa.sh — build the (unsigned) ChameleonUltraGUI iOS IPA and drop it
# on the Desktop (or a folder you pass as the first argument).
#
#   ./build_ipa.sh                # -> ~/Desktop/ChameleonUltraGUI.ipa
#   ./build_ipa.sh /some/folder   # -> /some/folder/ChameleonUltraGUI.ipa
#
# Lives in the ChameleonUltra repo; the Flutter GUI project is the sibling
# repo ../ChameleonUltraGUI/chameleonultragui. The IPA is unsigned: sign it
# with your Apple ID (AltStore / SideStore / Sideloadly, or Xcode -> Devices)
# to install it on an iPhone.
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Flutter GUI project (sibling repo). Override with env GUI_DIR if you moved it.
PROJECT_DIR="${GUI_DIR:-$SCRIPT_DIR/../ChameleonUltraGUI/chameleonultragui}"
DEST="/Users/teo/projects/Chamaleon/ChameleonUltra"
IPA_NAME="ChameleonUltraGUI.ipa"

if [ ! -d "$PROJECT_DIR" ]; then
  echo "ERROR: GUI project not found at $PROJECT_DIR" >&2
  echo "       Set GUI_DIR=/path/to/chameleonultragui and retry." >&2
  exit 1
fi
PROJECT_DIR="$(cd "$PROJECT_DIR" && pwd)"

# Locate flutter (PATH first, then the common Homebrew path).
FLUTTER="$(command -v flutter || true)"
[ -x "$FLUTTER" ] || FLUTTER="/opt/homebrew/bin/flutter"
if [ ! -x "$FLUTTER" ]; then
  echo "ERROR: flutter not found in PATH nor at /opt/homebrew/bin/flutter" >&2
  exit 1
fi

cd "$PROJECT_DIR"
echo "==> Building iOS app (unsigned)…"
"$FLUTTER" build ios --release --no-codesign

APP="$PROJECT_DIR/build/ios/iphoneos/Runner.app"
[ -d "$APP" ] || { echo "ERROR: Runner.app not found at $APP" >&2; exit 1; }

echo "==> Packaging IPA…"
WORK="$PROJECT_DIR/build/ios/iphoneos"
rm -rf "$WORK/Payload" "$WORK/$IPA_NAME"
mkdir -p "$WORK/Payload"
cp -R "$APP" "$WORK/Payload/"
( cd "$WORK" && zip -qr "$IPA_NAME" Payload )

mkdir -p "$DEST"
mv -f "$WORK/$IPA_NAME" "$DEST/$IPA_NAME"
rm -rf "$WORK/Payload"

echo "==> Done."
ls -lh "$DEST/$IPA_NAME"
