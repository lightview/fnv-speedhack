# FNVSpeedhack

Cheat Engine-style **game speed control** for Fallout: New Vegas — 1x to 10x
(slow-mo too) at the touch of a button. Ships as a tiny DLL plus an optional
**Decky Loader plugin** for Steam Deck, so switching speed on the Deck is just
pressing the back paddles: **L4 = 1x, L5 = 3x, R4 = 5x, R5 = 10x** — no Steam
Input bindings, no mods, no Cheat Engine.

[README на русском](README.ru.md)

## How it works

The same technique Cheat Engine's speedhack uses, self-implemented: the DLL
hooks the Windows timing APIs inside the game process (`QueryPerformanceCounter`,
`GetTickCount(64)`, `timeGetTime` via [MinHook](https://github.com/TsudaKageyu/minhook))
and scales elapsed time by the chosen factor. No game memory addresses, no exe
patches — works with **any** FNV build (Steam, GOG, any language) and even other
Gamebryo titles (Fallout 3).

Everything speeds up: movement, AI, animations, menus. Audio pitch stays normal
(same as Cheat Engine). Single-player use, saves unaffected.

Two interchangeable builds from one codebase:

| Build | File | Where it goes | Needs |
|---|---|---|---|
| NVSE plugin | `FNVSpeedhack.dll` | `Data\NVSE\Plugins\` | xNVSE |
| **dinput8 proxy** | `dinput8.dll` | game root | nothing (any exe) |

The proxy is the recommended way everywhere: `FalloutNV.exe` imports
`dinput8.dll`, so the game loads it by itself. On Windows just drop it in the
game folder. Under Proton add `WINEDLLOVERRIDES="dinput8=n,b" %command%` (the
Decky plugin's install button does this for you via the prefix registry).

## Steam Deck install

1. Install the Decky plugin from `fnv-speed.zip` (Releases) — unpack to
   `~/homebrew/plugins/fnv-speed/` and restart Decky (or install the zip from
   the Decky store's manual install).
2. QAM (…) → Decky → **FNV Speed** → **Install speedhack**.
   That single button copies the proxy DLL into the game folder, writes the
   ini and sets the `dinput8=native,builtin` override in the right Proton
   prefix (found via the appmanifest, localized depots included).
3. Play. Back paddles switch the speed: **L4 = 1x, L5 = 3x, R4 = 5x,
   R5 = 10x** — read straight from the controller via hidraw, so no Steam
   Input setup is needed. The QAM panel also has a 3×2 preset grid, a
   fine-tune slider and toasts on every speed change.

## Windows install

1. Grab `dinput8.dll` from the release and drop it into the game root
   (next to `FalloutNV.exe`). Done.
   (Or use the NVSE plugin build into `Data\NVSE\Plugins\` if you run xNVSE.)
2. Hotkeys: **F8** cycle, **F9/F10** step, **Numpad 1–4** direct
   1x/3x/5x/10x. A short beep confirms each change.
3. Configure everything in `FNVSpeedhack.ini` next to the DLL — see the
   reference below. A `fnvspeed.log` is written next to the DLL.

## Configuration

| Key | Default | Meaning |
|---|---|---|
| `HotkeyCycle` | `119` (F8) | cycle presets, `0` = off (VK code) |
| `HotkeyUp` / `HotkeyDown` | `120` / `121` | step up / down, `0` = off |
| `SpeedKeys` | `97:1.0,98:3.0,99:5.0,100:10.0` | direct `vk:speed` binds (Numpad1–4) |
| `PadModifier` | *(empty)* | XInput combos: e.g. `LB+RB`, then `PadA/B/X/Y` speeds; off by default on Deck |
| `Presets` | `1.0,2.0,3.0,5.0,10.0` | preset list for cycle/step (max 16) |
| `StartupSpeed` | `1.0` | speed applied on load |
| `ControlFileEnabled` | `1` | `fnvspeed.txt` / `fnvspeed_status.txt` IPC |
| `ControlPollMs` / `HotkeyPollMs` | `250` / `30` | poll intervals |
| `BeepFeedback` | `1` | beep on speed change |
| `LogEnabled` | `1` | write `fnvspeed.log` |

File IPC: write a number (`3.0`), or `toggle` / `up` / `down` / `reset` into
`fnvspeed.txt` in the game root; the DLL applies it within ~250 ms and reports
the current speed back in `fnvspeed_status.txt`. That's what the Decky panel
uses — and you can script it from anywhere (SSH included).

## Building from source

```sh
# DLLs (mingw-w64 i686, e.g. winlibs; Ubuntu: sudo apt install gcc-mingw-w64-i686)
./build.sh            # -> FNVSpeedhack.dll + dist/dinput8.dll

# Decky plugin frontend (Node.js 20+)
cd decky/fnv-speed && npm install && npm run build

# package the decky zip
cd ../.. && ./pack-decky.sh   # -> release/fnv-speed.zip
```

A standalone smoke test verifies hooks, hotkeys and file IPC without the game:

```sh
gcc -O2 -o test_harness.exe test/test_harness.c -lkernel32 -luser32
./test_harness.exe test/run          # NVSE build
./test_harness.exe test/run_proxy proxy  # dinput8 proxy build
```

## Credits & license

MIT — see [LICENSE](LICENSE). MinHook is vendored under its own MIT license.
Paddle reading follows the kernel's `hid-steam.c` report layout.
