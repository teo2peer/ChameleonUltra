#!/usr/bin/env bash

set -euo pipefail

if [[ ${BASH_SOURCE[0]} = */* ]]; then
  cd -- "${BASH_SOURCE[0]%/*}/" || exit
fi

softdevice=s140
softdevice_version=7.2.0
softdevice_id=0x0100

build_mode=${1:-compile}
case $build_mode in
  compile|package) ;;
  *) echo "Usage: $0 [compile|package]" >&2; exit 2 ;;
esac

device_type=${CURRENT_DEVICE_TYPE:-ultra}
case $device_type in
  ultra) hw_version=0 ;;
  lite) hw_version=1 ;;
  *) echo "Unknown CURRENT_DEVICE_TYPE '${CURRENT_DEVICE_TYPE:-}', aborting." >&2; exit 1 ;;
esac

derive_version() {
  local release tag distance major minor patch version

  release=${RELEASE_VERSION:-}
  if [[ -n $release ]]; then
    tag=$release
    distance=0
  else
    tag=$(git describe --tags --abbrev=0 --match 'v[0-9]*' 2>/dev/null) || {
      echo "No release tag found; set APPLICATION_VERSION and BOOTLOADER_VERSION explicitly." >&2
      return 1
    }
    distance=$(git rev-list --count "$tag"..HEAD)
  fi

  if [[ ! $tag =~ ^v?([0-9]+)\.([0-9]+)(\.([0-9]+))?$ ]]; then
    echo "Release version '$tag' must be vMAJOR.MINOR or vMAJOR.MINOR.PATCH." >&2
    return 1
  fi
  major=${BASH_REMATCH[1]}
  minor=${BASH_REMATCH[2]}
  patch=${BASH_REMATCH[4]:-0}
  if ((major > 4 || minor > 999 || patch > 999 || distance > 999)); then
    echo "Release version '$tag' or commit distance '$distance' exceeds DFU encoding bounds." >&2
    return 1
  fi

  version=$((10#$major * 1000000000 + 10#$minor * 1000000 + 10#$patch * 1000 + 10#$distance))
  if ((version < 1 || version > 4294967295)); then
    echo "Derived DFU version '$version' is outside uint32 range." >&2
    return 1
  fi
  printf '%s\n' "$version"
}

echo "Building firmware for $device_type (hw_version=$hw_version, mode=$build_mode)"

HW_VERSION=$hw_version ../.github/scripts/validate_firmware_release.sh --device "$device_type"

if [[ $build_mode == package ]]; then
  application_version=${APPLICATION_VERSION:-}
  bootloader_version=${BOOTLOADER_VERSION:-}
  if [[ -z $application_version || -z $bootloader_version ]]; then
    derived_version=$(derive_version)
    application_version=${application_version:-$derived_version}
    bootloader_version=${bootloader_version:-$derived_version}
  fi

  export APPLICATION_VERSION="$application_version"
  export BOOTLOADER_VERSION="$bootloader_version"
  export HW_VERSION="$hw_version"
  ../.github/scripts/validate_firmware_release.sh --device "$device_type" --package
  DFU_SIGNING_KEY=$(cd -- "$(dirname -- "$DFU_SIGNING_KEY")" && pwd -P)/$(basename -- "$DFU_SIGNING_KEY")
  export DFU_SIGNING_KEY
fi

rm -rf objects

(
  cd bootloader
  make -j
)

(
  cd application
  make -j
)

cp objects/application.out objects/application.elf
cp objects/bootloader.out objects/bootloader.elf
cp "nrf52_sdk/components/softdevice/${softdevice}/hex/${softdevice}_nrf52_${softdevice_version}_softdevice.hex" objects/softdevice.hex

size_tool="${GNU_INSTALL_ROOT:-}${GNU_PREFIX:-arm-none-eabi}-size"
if [[ -x $size_tool ]] || command -v "$size_tool" >/dev/null 2>&1; then
  "$size_tool" objects/application.out objects/bootloader.out > objects/firmware-size.txt
fi

if [[ $build_mode == compile ]]; then
  echo "Unsigned compile complete. Run '$0 package' with DFU_SIGNING_KEY set to create DFU packages."
  exit 0
fi

(
  cd objects

  nrfutil nrf5sdk-tools pkg generate \
    --hw-version "$hw_version" \
    --bootloader bootloader.hex --bootloader-version "$bootloader_version" --key-file "$DFU_SIGNING_KEY" \
    --application application.hex --application-version "$application_version" \
    --softdevice softdevice.hex \
    --sd-req "$softdevice_id" --sd-id "$softdevice_id" \
    "${device_type}-dfu-full.zip"

  if [[ ${ALLOW_APP_ONLY_DFU:-0} == 1 ]]; then
    nrfutil nrf5sdk-tools pkg generate \
      --hw-version "$hw_version" --key-file "$DFU_SIGNING_KEY" \
      --application application.hex --application-version "$application_version" \
      --sd-req "$softdevice_id" \
      "${device_type}-dfu-app.zip"
  else
    echo "App-only DFU suppressed until deployed devices have the enlarged FDS-aware bootloader."
  fi

  nrfutil nrf5sdk-tools settings generate \
    --family NRF52840 \
    --application application.hex --application-version "$application_version" \
    --softdevice softdevice.hex \
    --bootloader-version "$bootloader_version" --bl-settings-version 2 \
    settings.hex
  # Merged full-image hex + binaries.zip are only for direct SWD/J-Link
  # programming; the DFU package above is what USB/BLE DFU flashing uses. Skip
  # gracefully when 'mergehex' (Nordic nRF Command Line Tools) is absent so a
  # DFU-only workflow still succeeds.
  if command -v mergehex >/dev/null 2>&1; then
    mergehex \
      --merge \
      settings.hex \
      application.hex \
      --output application_merged.hex

    mergehex \
      --merge \
      bootloader.hex \
      application_merged.hex \
      softdevice.hex \
      --output fullimage.hex

    tmp_dir=$(mktemp -d -t cu_binaries_XXXXXXXXXX)
    trap 'rm -rf "$tmp_dir"' EXIT
    cp ./*.hex "$tmp_dir"
    mv "$tmp_dir/application_merged.hex" "$tmp_dir/application.hex"
    rm "$tmp_dir/settings.hex"
    zip -j "${device_type}-binaries.zip" "$tmp_dir"/*.hex
  else
    echo "WARNING: 'mergehex' not found — skipping merged full-image hex and ${device_type}-binaries.zip." >&2
    echo "         Those are only needed for direct SWD/J-Link programming; the DFU package" >&2
    echo "         (${device_type}-dfu-full.zip) is complete and is what USB/BLE DFU flashing uses." >&2
    echo "         Install Nordic nRF Command Line Tools to enable the merged-hex artifacts." >&2
  fi
)
