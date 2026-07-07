#!/usr/bin/env bash
#
# build_ipa.sh — build the ChameleonUltraGUI iOS app, code-sign it with a
# development provisioning profile + p12 certificate, package an .ipa, and
# (by default) auto-install it onto a connected iPhone.
#
#   ./build_ipa.sh                     # build, sign, package, install
#   ./build_ipa.sh --no-install        # build, sign, package only
#   P12_PASSWORD=secret ./build_ipa.sh # pass the .p12 password non-interactively
#
# Config can also come from a .env file next to this script (see .env.example);
# real environment variables still override it.
#
# Signing assets (override via env PROFILE / P12):
#   PROFILE = Development.mobileprovision   (next to this script)
#   P12     = Development.p12                (next to this script)
# The .p12 password is read from $P12_PASSWORD (or the .env), or prompted.
#
# The provisioning profile here is bound to a specific app id, so the app's
# CFBundleIdentifier is rewritten to match it during signing (sideload flow).
#
# Lives in the ChameleonUltra repo; the Flutter GUI project is the sibling
# repo ../ChameleonUltraGUI/chameleonultragui.
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# --- load .env (KEY=value lines) --------------------------------------------
# Anything already set in the real environment wins over the .env file, so
# `P12_PASSWORD=… ./build_ipa.sh` still overrides. See .env.example.
ENV_FILE="${ENV_FILE:-$SCRIPT_DIR/.env}"
if [ -f "$ENV_FILE" ]; then
  while IFS= read -r line || [ -n "$line" ]; do
    line="${line%$'\r'}"                       # strip trailing CR
    case "$line" in ''|\#*) continue ;; esac   # skip blanks / comments
    key="${line%%=*}"
    val="${line#*=}"
    key="${key#"${key%%[![:space:]]*}"}"       # trim leading ws
    key="${key%"${key##*[![:space:]]}"}"       # trim trailing ws
    [ -z "$key" ] && continue
    case "$val" in                              # strip one layer of quotes
      \"*\") val="${val#\"}"; val="${val%\"}" ;;
      \'*\') val="${val#\'}"; val="${val%\'}" ;;
    esac
    [ -z "${!key:-}" ] && export "$key=$val"    # real env overrides .env
  done < "$ENV_FILE"
fi

# --- config (env-overridable) ------------------------------------------------
PROJECT_DIR="${GUI_DIR:-$SCRIPT_DIR/../ChameleonUltraGUI/chameleonultragui}"
DEST="${DEST:-$SCRIPT_DIR}"
IPA_NAME="ChameleonUltraGUI.ipa"
PROFILE="${PROFILE:-$SCRIPT_DIR/Development.mobileprovision}"
P12="${P12:-$SCRIPT_DIR/Development.p12}"
INSTALL=1

for arg in "$@"; do
  case "$arg" in
    --no-install) INSTALL=0 ;;
    -h|--help) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "Unknown argument: $arg" >&2; exit 2 ;;
  esac
done

# --- sanity checks -----------------------------------------------------------
if [ ! -d "$PROJECT_DIR" ]; then
  echo "ERROR: GUI project not found at $PROJECT_DIR" >&2
  echo "       Set GUI_DIR=/path/to/chameleonultragui and retry." >&2
  exit 1
fi
PROJECT_DIR="$(cd "$PROJECT_DIR" && pwd)"
[ -f "$PROFILE" ] || { echo "ERROR: provisioning profile not found: $PROFILE" >&2; exit 1; }
[ -f "$P12" ]     || { echo "ERROR: signing certificate (.p12) not found: $P12" >&2; exit 1; }

FLUTTER="$(command -v flutter || true)"
[ -x "$FLUTTER" ] || FLUTTER="/opt/homebrew/bin/flutter"
[ -x "$FLUTTER" ] || { echo "ERROR: flutter not found in PATH nor /opt/homebrew/bin/flutter" >&2; exit 1; }

# .p12 password (env or prompt).
if [ -z "${P12_PASSWORD:-}" ]; then
  read -r -s -p "Password for $(basename "$P12"): " P12_PASSWORD
  echo
fi

# --- 1) build the unsigned app ----------------------------------------------
cd "$PROJECT_DIR"
echo "==> Building iOS app (unsigned)…"
"$FLUTTER" build ios --release --no-codesign

APP="$PROJECT_DIR/build/ios/iphoneos/Runner.app"
[ -d "$APP" ] || { echo "ERROR: Runner.app not found at $APP" >&2; exit 1; }

WORK="$PROJECT_DIR/build/ios/iphoneos"

# --- 2) prepare a throwaway keychain with the signing identity ---------------
KEYCHAIN="$WORK/chameleon-signing.keychain-db"
KEYCHAIN_PW="chameleon-$$"
# Remember the current search list so we can restore it on exit. Keychain
# paths have no spaces, so a newline-separated string + word-splitting is
# enough (and works on macOS's stock bash 3.2, which lacks `mapfile`).
ORIG_KEYCHAINS="$(security list-keychains -d user | sed -e 's/^[[:space:]]*//' -e 's/"//g')"

cleanup() {
  security delete-keychain "$KEYCHAIN" >/dev/null 2>&1 || true
  # shellcheck disable=SC2086
  [ -n "$ORIG_KEYCHAINS" ] && security list-keychains -d user -s $ORIG_KEYCHAINS >/dev/null 2>&1 || true
  rm -rf "$WORK/Payload"
}
trap cleanup EXIT

echo "==> Importing signing identity…"
security delete-keychain "$KEYCHAIN" >/dev/null 2>&1 || true
security create-keychain -p "$KEYCHAIN_PW" "$KEYCHAIN"
security set-keychain-settings -lut 21600 "$KEYCHAIN"
security unlock-keychain -p "$KEYCHAIN_PW" "$KEYCHAIN"
if ! security import "$P12" -k "$KEYCHAIN" -P "$P12_PASSWORD" \
       -T /usr/bin/codesign -T /usr/bin/security >/dev/null 2>&1; then
  echo "ERROR: failed to import $P12 — wrong password?" >&2
  exit 1
fi
security set-key-partition-list -S apple-tool:,apple:,codesign: \
  -s -k "$KEYCHAIN_PW" "$KEYCHAIN" >/dev/null 2>&1
# Prepend our keychain to the search list so codesign can find the identity.
# shellcheck disable=SC2086
security list-keychains -d user -s "$KEYCHAIN" $ORIG_KEYCHAINS >/dev/null

IDENTITY="$(security find-identity -v -p codesigning "$KEYCHAIN" \
             | awk 'match($0, /[0-9A-F]{40}/){print substr($0, RSTART, RLENGTH); exit}')"
[ -n "$IDENTITY" ] || { echo "ERROR: no codesigning identity found in $P12" >&2; exit 1; }
echo "    identity: $IDENTITY"

# --- 3) derive entitlements + app id from the provisioning profile -----------
security cms -D -i "$PROFILE" > "$WORK/profile.plist"
/usr/libexec/PlistBuddy -x -c "Print :Entitlements" "$WORK/profile.plist" > "$WORK/entitlements.plist"
FULL_APPID="$(/usr/libexec/PlistBuddy -c "Print :Entitlements:application-identifier" "$WORK/profile.plist")"
BUNDLE_ID="${FULL_APPID#*.}"   # strip the "TEAMID." prefix
echo "    app id:   $FULL_APPID  (bundle id -> $BUNDLE_ID)"

# --- 4) re-sign the app ------------------------------------------------------
echo "==> Signing…"
xattr -cr "$APP"
# Bundle id must match the profile's app id.
/usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier $BUNDLE_ID" "$APP/Info.plist"
# Embed the provisioning profile.
cp "$PROFILE" "$APP/embedded.mobileprovision"

# Sign nested code first (frameworks/dylibs get no app entitlements), then the
# app extensions (if any), then the main bundle last.
if [ -d "$APP/Frameworks" ]; then
  find "$APP/Frameworks" -maxdepth 1 \( -name "*.framework" -o -name "*.dylib" \) -print0 |
    while IFS= read -r -d '' item; do
      codesign --force --timestamp=none --sign "$IDENTITY" --keychain "$KEYCHAIN" "$item"
    done
fi
if [ -d "$APP/PlugIns" ]; then
  find "$APP/PlugIns" -maxdepth 1 -name "*.appex" -print0 |
    while IFS= read -r -d '' ext; do
      codesign --force --timestamp=none --sign "$IDENTITY" --keychain "$KEYCHAIN" \
        --entitlements "$WORK/entitlements.plist" "$ext"
    done
fi
codesign --force --timestamp=none --sign "$IDENTITY" --keychain "$KEYCHAIN" \
  --entitlements "$WORK/entitlements.plist" "$APP"
codesign --verify --verbose=2 "$APP"

# --- 5) package the IPA ------------------------------------------------------
echo "==> Packaging IPA…"
rm -rf "$WORK/Payload" "$WORK/$IPA_NAME"
mkdir -p "$WORK/Payload"
cp -R "$APP" "$WORK/Payload/"
( cd "$WORK" && zip -qr "$IPA_NAME" Payload )
mkdir -p "$DEST"
rm -f "$DEST/$IPA_NAME"
mv -f "$WORK/$IPA_NAME" "$DEST/$IPA_NAME"
echo "    -> $DEST/$IPA_NAME"
ls -lh "$DEST/$IPA_NAME"

# --- 6) install onto a connected device --------------------------------------
if [ "$INSTALL" -eq 1 ]; then
  DEVICE_ID="${DEVICE_ID:-$(xcrun devicectl list devices 2>/dev/null \
    | grep -Eo '[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}' \
    | head -n1)}"
  if [ -z "$DEVICE_ID" ]; then
    echo "==> No connected device found — skipping install (IPA is ready)."
  else
    echo "==> Installing onto device $DEVICE_ID…"
    if xcrun devicectl device install app --device "$DEVICE_ID" "$APP"; then
      echo "==> Installed. Trust the developer cert on the iPhone if prompted:"
      echo "    Settings > General > VPN & Device Management."
    else
      echo "ERROR: install failed. The IPA is still at $DEST/$IPA_NAME." >&2
      echo "       You can also sideload it with AltStore/Sideloadly." >&2
      exit 1
    fi
  fi
fi

echo "==> Done."
