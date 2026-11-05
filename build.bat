@echo off
rem Build the 32-bit FNVSpeedhack.dll with a mingw-w64 i686 toolchain.
rem Put gcc.exe (winlibs MinGW-w64 i686) in PATH or set CC before running.
setlocal
cd /d "%~dp0"

set CC=%CC%
if "%CC%"=="" (
    where gcc >nul 2>nul && set CC=gcc
)
if "%CC%"=="" (
    where i686-w64-mingw32-gcc >nul 2>nul && set CC=i686-w64-mingw32-gcc
)
if "%CC%"=="" (
    echo [!] mingw gcc not found. Install winlibs MinGW-w64 i686 or set CC=^<path^>
    exit /b 1
)

"%CC%" -O2 -Wall -Wextra -shared ^
    -o FNVSpeedhack.dll ^
    src\main.c ^
    src\hooks.c ^
    src\config.c ^
    src\control.c ^
    src\log.c ^
    third_party\minhook\src\hook.c ^
    third_party\minhook\src\buffer.c ^
    third_party\minhook\src\trampoline.c ^
    third_party\minhook\src\hde\hde32.c ^
    -Ithird_party\minhook\include ^
    -Isrc ^
    -static-libgcc ^
    src\fnvspeed.def ^
    -luser32
if errorlevel 1 exit /b 1

for %%P in ("%CC%") do set "STRIP=%%~dpPstrip.exe"
if exist "%STRIP%" ("%STRIP%" FNVSpeedhack.dll) else (strip FNVSpeedhack.dll 2>nul)
echo [OK] built FNVSpeedhack.dll
