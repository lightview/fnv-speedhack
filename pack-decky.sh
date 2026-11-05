#!/bin/sh
# Assemble the Decky plugin installable folder/zip.
# Output: release/fnv-speed.zip (and the staging folder release/fnv-speed/)
set -e
cd "$(dirname "$0")"

[ -f decky/fnv-speed/dist/index.js ] || {
    echo "[!] dist/index.js missing - run: cd decky/fnv-speed && npm install && npm run build"
    exit 1
}
[ -f FNVSpeedhack.dll ] || {
    echo "[!] FNVSpeedhack.dll missing - run ./build.sh first"
    exit 1
}

STAGE=release/fnv-speed
rm -rf release
mkdir -p "$STAGE"

cp decky/fnv-speed/plugin.json "$STAGE/"
cp decky/fnv-speed/main.py "$STAGE/"
cp decky/fnv-speed/dist/index.js "$STAGE/"
cp decky/fnv-speed/LICENSE "$STAGE/" 2>/dev/null || true
cp LICENSE "$STAGE/LICENSE"
cp README.md "$STAGE/README.md"
cp FNVSpeedhack.dll "$STAGE/"

cd release
powershell.exe -NoProfile -Command 'Compress-Archive -Path fnv-speed -DestinationPath fnv-speed.zip -Force' >/dev/null
cd ..
echo "[OK] packaged: release/fnv-speed.zip"
ls -la release
