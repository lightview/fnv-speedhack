#!/bin/sh
# Build the 32-bit speedhack DLLs with a mingw-w64 i686 toolchain.
#   FNVSpeedhack.dll  - NVSE plugin (Data\NVSE\Plugins)
#   dist/dinput8.dll  - dinput8 proxy (game folder, works without NVSE)
# Usage: ./build.sh            (finds gcc / i686-w64-mingw32-gcc in PATH)
#        CC=/path/to/gcc ./build.sh
set -e
cd "$(dirname "$0")"

CC="${CC:-gcc}"
command -v "$CC" >/dev/null 2>&1 || CC=i686-w64-mingw32-gcc
command -v "$CC" >/dev/null 2>&1 || {
    echo "[!] mingw gcc not found. Install winlibs MinGW-w64 i686 or set CC=path/to/gcc"
    exit 1
}

echo "[*] compiler: $("$CC" -dumpversion) ($(command -v "$CC"))"

mkdir -p build dist

CFLAGS="-O2 -Wall -Wextra -Ithird_party/minhook/include -Isrc"
COMMON="src/hooks.c src/config.c src/control.c src/log.c src/xinput_pad.c \
third_party/minhook/src/hook.c \
third_party/minhook/src/buffer.c \
third_party/minhook/src/trampoline.c \
third_party/minhook/src/hde/hde32.c"

OBJS=""
for f in $COMMON; do
    o="build/$(basename "$f" .c).o"
    # shellcheck disable=SC2086
    "$CC" $CFLAGS -c "$f" -o "$o"
    OBJS="$OBJS $o"
done

"$CC" -O2 -Wall -Wextra -shared \
    -o FNVSpeedhack.dll \
    src/main.c $OBJS \
    -static-libgcc \
    src/fnvspeed.def \
    -luser32
STRIP="$(dirname "$(command -v "$CC")")/strip.exe"
command -v "$STRIP" >/dev/null 2>&1 || STRIP=strip
"$STRIP" FNVSpeedhack.dll
echo "[OK] FNVSpeedhack.dll ($(ls -la FNVSpeedhack.dll | awk '{print $5}') bytes)"

"$CC" -O2 -Wall -Wextra -shared \
    -o dist/dinput8.dll \
    src/proxy_dinput8.c $OBJS \
    -static-libgcc \
    src/fnvspeed_dinput8.def \
    -luser32
"$STRIP" dist/dinput8.dll
echo "[OK] dist/dinput8.dll ($(ls -la dist/dinput8.dll | awk '{print $5}') bytes)"
