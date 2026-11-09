#!/bin/bash
# Builds the example app for the reMarkable Paper Pro (aarch64).
# Result: ./output-rmpp -> copy this directory to
#   /home/root/xovi/exthome/appload/<any-name>/ on the tablet.
# Requires: rcc (Qt), make, aarch64-linux-gnu-g++ (e.g. Debian package gcc-aarch64-linux-gnu)
set -e
cd "$(dirname "$0")"

rm -rf output-rmpp
mkdir -p output-rmpp/backend

if [ -f icon.png ]; then
    cp icon.png output-rmpp/icon.png
else
    # icon.png is stored base64-encoded (icon.png.b64) in this repo
    base64 -d icon.png.b64 > output-rmpp/icon.png
fi
cp manifest.json output-rmpp/
rcc --binary -o output-rmpp/resources.rcc application.qrc

make -C backend clean rmpp
cp backend/entry output-rmpp/backend/entry
chmod +x output-rmpp/backend/entry   # AppLoad exec()s backend/entry -- it must be executable

echo "Built ./output-rmpp"
