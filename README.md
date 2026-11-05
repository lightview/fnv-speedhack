# FNVSpeedhack

Cheat Engine-style **game speed control** for Fallout: New Vegas, packaged so it
just works everywhere: a plain **NVSE plugin** for Windows plus a **Decky Loader
plugin** for Steam Deck. Toggle between 1x, 2x, 3x, 5x (or slow-mo 0.5x) on the
fly while playing.

[README на русском](README.ru.md)

## How it works

Exactly the same technique Cheat Engine's speedhack uses: the DLL hooks the
Windows timing APIs (`QueryPerformanceCounter`, `GetTickCount`,
`GetTickCount64`, `timeGetTime`) inside the game process and scales elapsed
time by the chosen factor. No game memory addresses are used, so it works with
**any version of FNV** (Steam, GOG, 1.4.0.725 English/German) and even other
Gamebryo games (Fallout 3).

Notes:

- Game speed means *everything*: movement, animations, AI, loading menus.
  Audio volume/pitch stays normal (same as Cheat Engine).
- Single-player only, of course. Savegames are not affected.

## Windows install

1. Install [NVSE](https://github.com/ellieseavey/xNVSE/releases) (xNVSE) —
   copy its files into the game folder. If you have it already, skip.
2. Copy `FNVSpeedhack.dll` into `Data\NVSE\Plugins\`.
3. Play. Hotkeys:
   - **F8** — cycle through presets: 1x → 2x → 3x → 5x → 1x
   - **F9 / F10** — step up / down
   - a short beep confirms each preset (1x low … 5x high)

All hotkeys, presets and polling rates are configurable via
`FNVSpeedhack.ini` (see [`FNVSpeedhack.ini.example`](FNVSpeedhack.ini.example))
next to the DLL. A `fnvspeed.log` is written next to the DLL for debugging.

MO2 users: the DLL lives in the real game folder, so it loads regardless of
MO2 — nothing to configure in MO2.

## Steam Deck install

Requirements: Decky Loader, NVSE working under Proton.

1. **NVSE under Proton** — copy the xNVSE files into the game folder
   (`.../steamapps/common/Fallout New Vegas/`) and set Steam launch options
   (Properties → General → Launch Options):

   ```
   WINEDLLOVERRIDES="nvse_steam_loader.dll=n,b" %command%
   ```

   (This is the standard way to load xNVSE under Proton. The speedhack DLL
   itself needs **no** override — NVSE loads it explicitly.)

2. **Decky plugin** — grab `fnv-speed.zip` from Releases, unzip it so the
   folder lands at `~/homebrew/plugins/fnv-speed/`, then reload plugins
   (or restart Steam).

3. In game mode: `Quick Access Menu (…) → Decky → FNV Speed`:
   - press **Auto-detect** to find your game folder,
   - press **Install NVSE plugin DLL** (bundled DLL gets copied into
     `Data\NVSE\Plugins\`),
   - tap the speed buttons: `0.5x 1x 1.5x 2x 3x 5x`.

The plugin talks to the DLL through a small file in the game folder
(`fnvspeed.txt`), and the DLL publishes the current speed back
(`fnvspeed_status.txt`), so the panel always shows the real in-game speed.

**Prefer physical buttons?** No plugin needed: in Steam Input, put
`F8` (cycle) on a back paddle or make a shift layer — the DLL watches
keyboard hotkeys on Deck the same way it does on Windows.

## Building from source

Windows / Linux both work:

```sh
# DLL (needs mingw-w64 i686, e.g. winlibs; on Ubuntu: sudo apt install gcc-mingw-w64-i686)
./build.sh            # -> FNVSpeedhack.dll

# Decky plugin frontend (needs Node.js)
cd decky/fnv-speed
npm install
npm run build         # -> dist/index.js

# Package the Decky zip (needs the DLL + dist built first)
cd ../..
./pack-decky.sh       # -> release/fnv-speed.zip
```

There is also a standalone smoke test that verifies the whole hook + file-IPC
pipeline without the game:

```sh
gcc -O2 -o test_harness.exe test/test_harness.c -lkernel32 -luser32
./test_harness.exe test/run    # expects test/run/Data/NVSE/Plugins/FNVSpeedhack.dll
```

## Configuration reference

| Key                  | Default        | Meaning                                   |
|----------------------|----------------|-------------------------------------------|
| `HotkeyCycle`        | `119` (F8)     | cycle presets, `0` = off (VK code)        |
| `HotkeyUp`           | `120` (F9)     | step up, `0` = off                        |
| `HotkeyDown`         | `121` (F10)    | step down, `0` = off                      |
| `SpeedKeys`          | `97:1.0,98:3.0,99:5.0,100:10.0` | direct `vk:speed` binds (Numpad1..4), up to 8 |
| `Presets`            | `1.0,2.0,3.0,5.0,10.0` | up to 16 values, e.g. add `0.5` for slow-mo |
| `StartupSpeed`       | `1.0`          | speed on plugin load                      |
| `ControlFileEnabled` | `1`            | fnvspeed.txt / fnvspeed_status.txt IPC    |
| `HotkeyPollMs`       | `30`           | hotkey poll interval                      |
| `ControlPollMs`      | `250`          | file poll interval                        |
| `BeepFeedback`       | `1`            | beep on speed change                      |
| `LogEnabled`         | `1`            | write fnvspeed.log                        |

### Recommended Steam Deck paddle layout

In Steam Input (FNV → controller layout → Edit layout → Back Grip Buttons):

| Paddle | Keyboard key | Speed |
|--------|--------------|-------|
| L4 | Keypad 1 | 1x |
| L5 | Keypad 2 | 3x |
| R4 | Keypad 3 | 5x |
| R5 | Keypad 4 | 10x |

## License

MIT — see [LICENSE](LICENSE). MinHook is vendored under its own MIT license
(`third_party/minhook/LICENSE.txt`).
