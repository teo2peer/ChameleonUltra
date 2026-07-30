#!/usr/bin/env bash
#
# build_ipa.sh — one-shot pipeline: build the ChameleonUltraGUI iOS app,
# code-sign it, package an .ipa, and (if a device is reachable) auto-install it
# onto a connected iPhone.
#
# Signing + install are delegated to the FirmadorDeApps utility (zsign +
# libimobiledevice), which keeps its own certificate, provisioning profile and
# password in one place. This script only builds the unsigned .app and hands it
# to that pipeline.
#
#   ./build_ipa.sh                     # build, sign, install (if a device is near)
#   ./build_ipa.sh --no-install        # build, sign, package only
#   ./build_ipa.sh -N                  # also consider Wi-Fi devices for install
#   ./build_ipa.sh -D <UDID>           # sign + install onto a specific device
#
# Config can also come from a .env file next to this script (see .env.example);
# real environment variables still override it.
#
# Signing material lives in the FirmadorDeApps folder (FIRMADOR_DIR), not here:
#   FIRMADOR_DIR = the FirmadorDeApps checkout (has bin/zsign, sign-app.sh,
#                  a .p12, a .mobileprovision and password.txt).
# Override the certificate / profile / password per-run with P12 / PROFILE /
# P12_PASSWORD (also honoured as ZSIGN_P12_PASSWORD).
#
# The provisioning profile is bound to a specific app id, so the app's
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
# The FirmadorDeApps signing utility (zsign + libimobiledevice wrapper).
FIRMADOR_DIR="${FIRMADOR_DIR:-/Users/teo/PersonalData/Nextcloud/proyectos/Personales/2_Utilidades/FirmadorDeApps}"
# Optional per-run signing overrides (otherwise FirmadorDeApps auto-detects).
PROFILE="${PROFILE:-}"
P12="${P12:-}"
INSTALL=1
NETWORK=0
DEVICE_ID="${DEVICE_ID:-}"

while [ $# -gt 0 ]; do
  case "$1" in
    --no-install)        INSTALL=0; shift ;;
    -N|--network)        NETWORK=1; shift ;;
    -D|--device-id)      DEVICE_ID="${2:?"-D needs a UDID"}"; INSTALL=1; shift 2 ;;
    -D=*|--device-id=*)  DEVICE_ID="${1#*=}"; INSTALL=1; shift ;;
    -h|--help)           grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "Unknown argument: $1" >&2; exit 2 ;;
  esac
done

# --- sanity checks -----------------------------------------------------------
if [ ! -d "$PROJECT_DIR" ]; then
  echo "ERROR: GUI project not found at $PROJECT_DIR" >&2
  echo "       Set GUI_DIR=/path/to/chameleonultragui and retry." >&2
  exit 1
fi
PROJECT_DIR="$(cd "$PROJECT_DIR" && pwd)"

SIGN_APP="$FIRMADOR_DIR/sign-app.sh"
if [ ! -x "$SIGN_APP" ]; then
  echo "ERROR: FirmadorDeApps signer not found/executable: $SIGN_APP" >&2
  echo "       Set FIRMADOR_DIR=/path/to/FirmadorDeApps (must contain sign-app.sh" >&2
  echo "       and bin/zsign — run its ./install-zsign.sh once if missing)." >&2
  exit 1
fi
[ -z "$PROFILE" ] || [ -f "$PROFILE" ] || { echo "ERROR: PROFILE not found: $PROFILE" >&2; exit 1; }
[ -z "$P12" ]     || [ -f "$P12" ]     || { echo "ERROR: P12 not found: $P12" >&2; exit 1; }

FLUTTER="$(command -v flutter || true)"
[ -x "$FLUTTER" ] || FLUTTER="/opt/homebrew/bin/flutter"
[ -x "$FLUTTER" ] || { echo "ERROR: flutter not found in PATH nor /opt/homebrew/bin/flutter" >&2; exit 1; }

# A .p12 password in P12_PASSWORD (env or .env) is forwarded to sign-app.sh via
# ZSIGN_P12_PASSWORD; otherwise sign-app.sh falls back to its own password.txt.
if [ -n "${P12_PASSWORD:-}" ] && [ -z "${ZSIGN_P12_PASSWORD:-}" ]; then
  export ZSIGN_P12_PASSWORD="$P12_PASSWORD"
fi

# --- 1) build the unsigned app ----------------------------------------------
cd "$PROJECT_DIR"
echo "==> Building iOS app (unsigned)…"
"$FLUTTER" build ios --release --no-codesign

WORK="$PROJECT_DIR/build/ios/iphoneos"
APP="$WORK/Runner.app"
[ -d "$APP" ] || { echo "ERROR: Runner.app not found at $APP" >&2; exit 1; }
xattr -cr "$APP" 2>/dev/null || true

# Stage an unsigned IPA (Payload/Runner.app) for zsign to re-sign. zsign refuses
# to package a bare .app into an .ipa ("Can't find payload directory"), so we
# hand it a proper Payload/ archive instead.
UNSIGNED_IPA="$WORK/ChameleonUltraGUI-unsigned.ipa"
STAGE="$WORK/_ipa_stage"
cleanup() { rm -rf "$STAGE" "$UNSIGNED_IPA"; }
trap cleanup EXIT
rm -rf "$STAGE" "$UNSIGNED_IPA"
mkdir -p "$STAGE/Payload"
cp -R "$APP" "$STAGE/Payload/"
( cd "$STAGE" && zip -qry "$UNSIGNED_IPA" Payload )
rm -rf "$STAGE"

# --- 2) derive the bundle id from the signing profile ------------------------
# The profile FirmadorDeApps will use (an explicit override, or its auto-detected
# one). If the profile is app-id-specific we rewrite CFBundleIdentifier to match;
# a wildcard profile needs no rewrite.
find_first() {  # $1=dir $2=glob -> first existing match
  local d="$1" pat="$2" f
  for f in "$d"/$pat; do [ -e "$f" ] && { printf '%s' "$f"; return 0; }; done
  return 1
}
RESOLVED_PROFILE="$PROFILE"
if [ -z "$RESOLVED_PROFILE" ]; then
  RESOLVED_PROFILE="$(find_first "$FIRMADOR_DIR/certs" '*.mobileprovision' || true)"
  [ -n "$RESOLVED_PROFILE" ] || RESOLVED_PROFILE="$(find_first "$FIRMADOR_DIR" '*.mobileprovision' || true)"
fi

BUNDLE_ID=""
if [ -n "$RESOLVED_PROFILE" ] && [ -f "$RESOLVED_PROFILE" ]; then
  PROF_PLIST="$PROJECT_DIR/build/ios/iphoneos/_firmador_profile.plist"
  if security cms -D -i "$RESOLVED_PROFILE" > "$PROF_PLIST" 2>/dev/null; then
    APPID="$(/usr/libexec/PlistBuddy -c "Print :Entitlements:application-identifier" "$PROF_PLIST" 2>/dev/null || true)"
    rm -f "$PROF_PLIST"
    APPID="${APPID#*.}"                    # strip "TEAMID." prefix
    case "$APPID" in
      *\*) BUNDLE_ID="" ;;                 # wildcard profile -> keep app's id
      "")  BUNDLE_ID="" ;;
      *)   BUNDLE_ID="$APPID" ;;
    esac
  fi
  echo "    profile:  $RESOLVED_PROFILE"
  [ -n "$BUNDLE_ID" ] && echo "    bundle id -> $BUNDLE_ID"
else
  echo "    profile:  (FirmadorDeApps auto-detect)"
fi

# --- 3) decide whether a device is reachable for install ---------------------
device_reachable() {
  command -v idevice_id >/dev/null 2>&1 || return 1
  [ -n "$(idevice_id -l 2>/dev/null)" ] && return 0   # USB
  [ -n "$(idevice_id -n 2>/dev/null)" ] && return 0   # Wi-Fi (paired + on-net)
  return 1
}

# --- 4) sign (and optionally install) via FirmadorDeApps ---------------------
SIGN_ARGS=(-o "$DEST/$IPA_NAME")
[ -n "$BUNDLE_ID" ] && SIGN_ARGS+=(-b "$BUNDLE_ID")
[ -n "$PROFILE" ]   && SIGN_ARGS+=(-m "$PROFILE")
[ -n "$P12" ]       && SIGN_ARGS+=(-c "$P12")
[ "$NETWORK" -eq 1 ] && SIGN_ARGS+=(-N)

DID_INSTALL=0
if [ "$INSTALL" -eq 1 ]; then
  if [ -n "$DEVICE_ID" ]; then
    echo "==> Signing + installing onto device $DEVICE_ID…"
    SIGN_ARGS+=(-D "$DEVICE_ID"); DID_INSTALL=1
  elif device_reachable; then
    echo "==> Signing + installing onto the nearby device…"
    SIGN_ARGS+=(--install); DID_INSTALL=1
  else
    echo "==> No connected device found — signing only (IPA will be ready)."
  fi
else
  echo "==> Signing only (--no-install)…"
fi

set +e
"$SIGN_APP" "${SIGN_ARGS[@]}" "$UNSIGNED_IPA"
status=$?
set -e

if [ "$status" -ne 0 ]; then
  echo "ERROR: signing/install pipeline failed (exit $status)." >&2
  [ -f "$DEST/$IPA_NAME" ] && echo "       A signed IPA may still be at $DEST/$IPA_NAME." >&2
  exit "$status"
fi

# --- 5) report ---------------------------------------------------------------
[ -f "$DEST/$IPA_NAME" ] && { echo "    -> $DEST/$IPA_NAME"; ls -lh "$DEST/$IPA_NAME"; }
if [ "$DID_INSTALL" -eq 1 ]; then
  echo "==> Installed. If iOS refuses to launch it, trust the developer cert:"
  echo "    Settings > General > VPN & Device Management."
fi
echo "==> Done."
