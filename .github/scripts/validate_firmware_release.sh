#!/usr/bin/env bash

set -euo pipefail

repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
device=
package=0

while (($#)); do
  case $1 in
    --device) device=${2:-}; shift 2 ;;
    --package) package=1; shift ;;
    *) echo "Usage: $0 [--device ultra|lite] [--package]" >&2; exit 2 ;;
  esac
done

read_define() {
  local file=$1 name=$2
  awk -v name="$name" '$1 == "#define" && $2 == name { print $3; exit }' "$file"
}

read_flash_value() {
  local file=$1 field=$2
  awk -v field="$field" '/FLASH \(rx\)/ { for (i = 1; i <= NF; i++) if ($i == field) { value = $(i + 2); sub(/,/, "", value); print value; exit } }' "$file"
}

read_region_value() {
  local file=$1 region=$2 field=$3
  awk -v region="$region" -v field="$field" '$1 == region { for (i = 1; i <= NF; i++) if ($i == field) { value = $(i + 2); sub(/,/, "", value); print value; exit } }' "$file"
}

require_uint32() {
  local name=$1 value=$2
  if [[ ! $value =~ ^[0-9]+$ ]] || ((10#$value < 1 || 10#$value > 4294967295)); then
    echo "$name must be a decimal integer from 1 through 4294967295; got '$value'." >&2
    exit 1
  fi
}

app_ld="$repo_root/firmware/application/application.ld"
boot_ld="$repo_root/firmware/bootloader/bootloader.ld"
app_config="$repo_root/firmware/application/src/sdk_config.h"
boot_config="$repo_root/firmware/bootloader/src/sdk_config.h"

app_origin=$(( $(read_flash_value "$app_ld" ORIGIN) ))
app_length=$(( $(read_flash_value "$app_ld" LENGTH) ))
boot_origin=$(( $(read_flash_value "$boot_ld" ORIGIN) ))
boot_length=$(( $(read_flash_value "$boot_ld" LENGTH) ))
fds_pages=$(( $(read_define "$app_config" FDS_VIRTUAL_PAGES) ))
fds_page_words=$(( $(read_define "$app_config" FDS_VIRTUAL_PAGE_SIZE) ))
fds_reserved_pages=$(( $(read_define "$app_config" FDS_VIRTUAL_PAGES_RESERVED) ))
dfu_data_size=$(( $(read_define "$boot_config" NRF_DFU_APP_DATA_AREA_SIZE) ))
accept_same=$(( $(read_define "$boot_config" NRF_DFU_APP_ACCEPT_SAME_VERSION) ))
settings_origin=$(( $(read_region_value "$boot_ld" bootloader_settings_page ORIGIN) ))
settings_length=$(( $(read_region_value "$boot_ld" bootloader_settings_page LENGTH) ))
mbr_origin=$(( $(read_region_value "$boot_ld" mbr_params_page ORIGIN) ))
mbr_length=$(( $(read_region_value "$boot_ld" mbr_params_page LENGTH) ))
fds_size=$((fds_pages * fds_page_words * 4))
app_end=$((app_origin + app_length))
fds_end=$((app_end + fds_size))
boot_end=$((boot_origin + boot_length))

if ((app_origin != 0x27000)); then
  printf 'Application origin 0x%X does not start after the S140/MBR reservation at 0x27000.\n' "$app_origin" >&2
  exit 1
fi
if ((fds_reserved_pages != 0)); then
  echo "FDS_VIRTUAL_PAGES_RESERVED must remain 0 because the linker reserves the complete FDS area." >&2
  exit 1
fi
if ((app_end != boot_origin - fds_size || fds_end != boot_origin)); then
  printf 'Application/FDS/bootloader overlap: app end=0x%X, FDS size=0x%X, bootloader start=0x%X.\n' "$app_end" "$fds_size" "$boot_origin" >&2
  exit 1
fi
if ((dfu_data_size != fds_size)); then
  printf 'NRF_DFU_APP_DATA_AREA_SIZE=0x%X, expected FDS reservation 0x%X.\n' "$dfu_data_size" "$fds_size" >&2
  exit 1
fi
if ((accept_same != 0)); then
  echo "NRF_DFU_APP_ACCEPT_SAME_VERSION must be 0." >&2
  exit 1
fi
if ((boot_end != mbr_origin || mbr_origin + mbr_length != settings_origin ||
      settings_origin + settings_length > 0x100000)); then
  printf 'Bootloader/MBR/settings boundary invalid: boot end=0x%X, MBR=[0x%X,0x%X), settings=[0x%X,0x%X).\n' \
    "$boot_end" "$mbr_origin" "$((mbr_origin + mbr_length))" \
    "$settings_origin" "$((settings_origin + settings_length))" >&2
  exit 1
fi

crc_dependency='$(SDK_ROOT)/components/libraries/crc16/crc16.c'
if ! grep -Fq "$crc_dependency" "$repo_root/firmware/application/Makefile"; then
  echo "Application Makefile must compile the Nordic crc16 dependency used by FDS." >&2
  exit 1
fi

if [[ -n $device ]]; then
  case $device in
    ultra) expected_hw=0 ;;
    lite) expected_hw=1 ;;
    *) echo "Device must be 'ultra' or 'lite'; got '$device'." >&2; exit 1 ;;
  esac
  if [[ -n ${HW_VERSION:-} && $HW_VERSION != "$expected_hw" ]]; then
    echo "HW_VERSION for $device must be $expected_hw; got '$HW_VERSION'." >&2
    exit 1
  fi
fi

if ((package)); then
  if [[ -z $device ]]; then
    echo "--package requires --device ultra|lite." >&2
    exit 1
  fi
  require_uint32 APPLICATION_VERSION "${APPLICATION_VERSION:-}"
  require_uint32 BOOTLOADER_VERSION "${BOOTLOADER_VERSION:-}"
  if [[ -z ${DFU_SIGNING_KEY:-} || ! -r $DFU_SIGNING_KEY ]]; then
    echo "Package generation requires DFU_SIGNING_KEY to name a readable external private-key file." >&2
    exit 1
  fi
  if ! grep -q -E '^-----BEGIN ([A-Z0-9]+ )?PRIVATE KEY-----$' "$DFU_SIGNING_KEY"; then
    echo "DFU_SIGNING_KEY does not contain a PEM private key." >&2
    exit 1
  fi
  key_dir=$(cd -- "$(dirname -- "$DFU_SIGNING_KEY")" && pwd -P)
  case "$key_dir/$(basename -- "$DFU_SIGNING_KEY")" in
    "$repo_root"/*)
      echo "DFU_SIGNING_KEY must be outside the repository checkout." >&2
      exit 1
      ;;
  esac
  if ! command -v nrfutil >/dev/null 2>&1; then
    echo "nrfutil is required to verify the DFU signing key." >&2
    exit 1
  fi
  generated_key=$(nrfutil nrf5sdk-tools keys display --key pk --format code "$DFU_SIGNING_KEY" |
    grep -Eo '0x[0-9a-fA-F]{2}' | tr '[:upper:]' '[:lower:]' | tr -d '\n')
  compiled_key=$(grep -Eo '0x[0-9a-fA-F]{2}' \
    "$repo_root/firmware/bootloader/src/dfu_public_key.c" |
    tr '[:upper:]' '[:lower:]' | tr -d '\n')
  if [[ -z $generated_key || $generated_key != "$compiled_key" ]]; then
    echo "DFU_SIGNING_KEY does not match the public key compiled into the bootloader." >&2
    exit 1
  fi
fi

printf 'Firmware layout valid: app [0x%X,0x%X), FDS [0x%X,0x%X), bootloader [0x%X,0x%X).\n' \
  "$app_origin" "$app_end" "$app_end" "$fds_end" "$boot_origin" "$boot_end"
