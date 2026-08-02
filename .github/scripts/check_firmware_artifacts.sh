#!/usr/bin/env bash

set -euo pipefail

repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
artifact_dir=${1:-"$repo_root/firmware/objects"}
if [[ $artifact_dir != /* ]]; then
  artifact_dir="$repo_root/$artifact_dir"
fi

if [[ ! -d $artifact_dir ]]; then
  echo "No firmware artifact directory; map/size budget checks skipped."
  exit 0
fi

shopt -s nullglob
maps=("$artifact_dir"/*.map)
executables=("$artifact_dir"/*.out "$artifact_dir"/*.elf)
size_reports=("$artifact_dir"/*size*.txt)
if [[ ! -f $artifact_dir/application.map || ! -f $artifact_dir/bootloader.map ]]; then
  echo "Firmware artifacts must include application.map and bootloader.map." >&2
  exit 1
fi
if ((${#size_reports[@]} == 0)); then
  echo "Firmware artifacts must include a size report." >&2
  exit 1
fi

check_map_region() {
  local map_file=$1 region=$2 expected_origin=$3 expected_length=$4
  local values origin length
  values=$(awk -v region="$region" '$1 == region && $2 ~ /^0x/ { print $2, $3; exit }' "$map_file")
  if [[ -z $values ]]; then
    echo "$(basename "$map_file") has no $region memory-region report." >&2
    exit 1
  fi
  read -r origin length <<<"$values"
  if [[ ! $origin =~ ^0[xX][0-9a-fA-F]+$ || ! $length =~ ^0[xX][0-9a-fA-F]+$ ]]; then
    echo "$(basename "$map_file") has malformed $region region values." >&2
    exit 1
  fi
  if ((origin != expected_origin || length != expected_length)); then
    printf '%s %s region is [0x%X,+0x%X), expected [0x%X,+0x%X).\n' \
      "$(basename "$map_file")" "$region" "$origin" "$length" \
      "$expected_origin" "$expected_length" >&2
    exit 1
  fi
}

for map_file in "${maps[@]}"; do
  case $(basename "$map_file") in
    application.map)
      check_map_region "$map_file" FLASH 0x27000 0xA0000
      check_map_region "$map_file" RAM 0x20003AE8 0x34518
      ;;
    bootloader.map)
      check_map_region "$map_file" FLASH 0xF3000 0xB000
      check_map_region "$map_file" RAM 0x20005978 0x32688
      ;;
    *)
      echo "Unexpected firmware map file: $(basename "$map_file")" >&2
      exit 1
      ;;
  esac
done

if ((${#executables[@]} != 0 && ${#size_reports[@]} == 0)); then
  echo "Firmware executables exist but no size report was generated." >&2
  exit 1
fi

checked_application=0
checked_bootloader=0
for report in "${size_reports[@]}"; do
  while read -r text data bss _ _ filename; do
    [[ $text =~ ^[0-9]+$ && $data =~ ^[0-9]+$ && $bss =~ ^[0-9]+$ ]] || continue
    flash_used=$((text + data))
    ram_used=$((data + bss))
    case $(basename "$filename") in
      application.out|application.elf)
        ((flash_used <= 0xA0000)) || { echo "Application flash budget exceeded: $flash_used > $((0xA0000))." >&2; exit 1; }
        ((ram_used <= 0x34518)) || { echo "Application RAM budget exceeded: $ram_used > $((0x34518))." >&2; exit 1; }
        checked_application=1
        ;;
      bootloader.out|bootloader.elf)
        ((flash_used <= 0xB000)) || { echo "Bootloader flash budget exceeded: $flash_used > $((0xB000))." >&2; exit 1; }
        ((ram_used <= 0x32688)) || { echo "Bootloader RAM budget exceeded: $ram_used > $((0x32688))." >&2; exit 1; }
        checked_bootloader=1
        ;;
    esac
  done < "$report"
done

if ((${#executables[@]} != 0 && (!checked_application || !checked_bootloader))); then
  echo "Size reports must contain application and bootloader rows." >&2
  exit 1
fi

echo "Firmware map regions and available size budgets are valid."
