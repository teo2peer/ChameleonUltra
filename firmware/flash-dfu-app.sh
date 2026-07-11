#!/bin/bash

if [[ $BASH_SOURCE = */* ]]; then
  cd -- "${BASH_SOURCE%/*}/" || exit
fi

if [ "${ALLOW_APP_ONLY_DFU:-0}" != "1" ]; then
    echo "App-only flashing requires ALLOW_APP_ONLY_DFU=1 after bootloader migration."
    exit 1
fi

if [ ! -f objects/ultra-dfu-app.zip ] && [ ! -f objects/lite-dfu-app.zip ]; then
    echo "No app-only DFU package exists. Deploy the full FDS-aware bootloader first."
    exit 1
fi

if ! ../resource/tools/enter_dfu.py; then
    echo "Wait for device to be off"
    echo "Press B and plug"
    echo "LEDS 4 & 5 should blink"
fi
while :; do
  lsusb|grep -q 1915:521f && break
  sleep 1
done

device_type=ultra
lsusb | grep 1915:521f | grep -q ChameleonLite && device_type=lite

echo "Flashing $device_type"

dfu_package=objects/${device_type}-dfu-app.zip

if [ ! -f $dfu_package ]; then
    echo "DFU package for $device_type not found, aborting."
    echo "App-only packages require an already-migrated FDS-aware bootloader and ALLOW_APP_ONLY_DFU=1."
    exit 1
fi

nrfutil device program --firmware $dfu_package --traits nordicDfu
