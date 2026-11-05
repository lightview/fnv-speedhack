#!/bin/sh
# Build the 32-bit FNVSpeedhack.dll with a mingw-w64 i686 toolchain.
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

"$CC" -O2 -Wall -Wextra -shared \
    -o FNVSpeedhack.dll \
    src/main.c \
    src/hooks.c \
    src/config.c \
    src/control.c \
    src/log.c \
    third_party/minhook/src/hook.c \
    third_party/minhook/src/buffer.c \
    third_party/minhook/src/trampoline.c \
    third_party/minhook/src/hde/hde32.c \
    -Ithird_party/minhook/include \
    -Isrc \
    -static-libgcc \
    src/fnvspeed.def \
    -luser32

STRIP="$(dirname "$(command -v "$CC")")/strip.exe"
command -v "$STRIP" >/dev/null 2>&1 || STRIP=strip
"$STRIP" FNVSpeedhack.dll
echo "[OK] built $(ls -la FNVSpeedhack.dll | awk '{print $5}') bytes: FNVSpeedhack.dll"
