#!/bin/bash
# Builds the example app for the AppLoad PC emulator (native build).
# Result: ./output -> place this directory into the emulator's applications_root folder.
# Requires: rcc (Qt), make, g++
set -e
cd "$(dirname "$0")"

rm -rf output
mkdir -p output/backend

if [ -f icon.png ]; then
    cp icon.png output/icon.png
else
    # icon.png is stored base64-encoded (icon.png.b64) in this repo
    base64 -d icon.png.b64 > output/icon.png
fi
cp manifest.json output/
rcc --binary -o output/resources.rcc application.qrc

make -C backend clean all
cp backend/entry output/backend/entry

echo "Built ./output"
