#!/bin/bash
# Builds the example app for reMarkable 1 / reMarkable 2 (armv7, hard-float).
# Result: ./output-rm -> copy this directory to
#   /home/root/xovi/exthome/appload/<any-name>/ on the tablet.
# Requires: rcc (Qt), make, arm-linux-gnueabihf-g++ (e.g. Debian package gcc-arm-linux-gnueabihf)
set -e
cd "$(dirname "$0")"

rm -rf output-rm
mkdir -p output-rm/backend

if [ -f icon.png ]; then
    cp icon.png output-rm/icon.png
else
    # icon.png is stored base64-encoded (icon.png.b64) in this repo
    base64 -d icon.png.b64 > output-rm/icon.png
fi
cp manifest.json output-rm/
rcc --binary -o output-rm/resources.rcc application.qrc

make -C backend clean rm
cp backend/entry output-rm/backend/entry
chmod +x output-rm/backend/entry   # AppLoad exec()s backend/entry -- it must be executable

echo "Built ./output-rm"
