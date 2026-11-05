import os
import re
import json
import glob
import shutil
import asyncio

import decky

DLL_NAME = "FNVSpeedhack.dll"
CTL_FILE = "fnvspeed.txt"
STATUS_FILE = "fnvspeed_status.txt"
GAME_EXE = "FalloutNV.exe"
GAME_SUBDIR = os.path.join("Fallout New Vegas")

DEFAULT_INI = """[speedhack]
; VK codes (decimal). 0 disables a hotkey.
; F8=119 F9=120 F10=121
HotkeyCycle=119
HotkeyUp=120
HotkeyDown=121
; direct binds "vk:speed" - Numpad1..4 for Steam Deck back paddles
; (Steam Input: L4->Num1 L5->Num2 R4->Num3 R5->Num4 = 1x/3x/5x/10x)
SpeedKeys=97:1.0,98:3.0,99:5.0,100:10.0
; comma separated speed presets
Presets=1.0,2.0,3.0,5.0,10.0
StartupSpeed=1.0
; file IPC (used by this Decky plugin)
ControlFileEnabled=1
ControlPollMs=250
HotkeyPollMs=30
BeepFeedback=1
LogEnabled=1
"""


def _settings_path() -> str:
    return os.path.join(decky.DECKY_PLUGIN_SETTINGS_DIR, "fnvspeed.json")


def _load_settings() -> dict:
    try:
        with open(_settings_path(), "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return {}


def _save_settings(data: dict) -> None:
    os.makedirs(decky.DECKY_PLUGIN_SETTINGS_DIR, exist_ok=True)
    with open(_settings_path(), "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)


def _steam_roots() -> list[str]:
    home = decky.DECKY_USER_HOME
    roots = [
        os.path.join(home, ".local", "share", "Steam"),
        os.path.join(home, ".steam", "steam"),
        os.path.join(home, ".steam", "debian-installation"),
        os.path.join(home, ".steam", "root"),
    ]
    # secondary library locations: SD cards and external drives
    roots += glob.glob("/run/media/*/*")
    seen, out = set(), []
    for r in roots:
        if r and r not in seen and os.path.isdir(r):
            seen.add(r)
            out.append(r)
    return out


def _library_paths(root: str) -> list[str]:
    paths = [root]
    vdf = os.path.join(root, "steamapps", "libraryfolders.vdf")
    try:
        with open(vdf, "r", encoding="utf-8", errors="replace") as f:
            for m in re.finditer(r'"path"\s+"([^"]+)"', f.read()):
                p = m.group(1).replace("\\\\", "\\")
                if p not in paths:
                    paths.append(p)
    except Exception:
        pass
    return paths


def _game_dir_if_valid(library: str) -> str | None:
    game_dir = os.path.join(library, "steamapps", "common", GAME_SUBDIR)
    if os.path.isfile(os.path.join(game_dir, GAME_EXE)):
        return game_dir
    return None


def _detect_candidates() -> list[str]:
    found: list[str] = []
    for root in _steam_roots():
        for library in _library_paths(root):
            gd = _game_dir_if_valid(library)
            if gd and gd not in found:
                found.append(gd)
    return found


class Plugin:
    async def get_status(self) -> dict:
        settings = _load_settings()
        game_dir = settings.get("game_dir") or None
        dll_installed = False
        speed = None

        if game_dir:
            dll_installed = os.path.isfile(
                os.path.join(game_dir, "Data", "NVSE", "Plugins", DLL_NAME))
            try:
                with open(os.path.join(game_dir, STATUS_FILE), "r") as f:
                    speed = float(f.read().strip())
            except Exception:
                speed = None

        return {
            "game_dir": game_dir,
            "dll_installed": dll_installed,
            "speed": speed,
            "candidates": await self.autodetect(),
        }

    async def autodetect(self) -> list[str]:
        return _detect_candidates()

    async def set_game_dir(self, path: str) -> bool:
        path = (path or "").strip().strip('"')
        if not os.path.isfile(os.path.join(path, GAME_EXE)):
            decky.logger.warning("set_game_dir: no %s in %r", GAME_EXE, path)
            return False
        settings = _load_settings()
        settings["game_dir"] = path
        _save_settings(settings)
        decky.logger.info("game dir set: %s", path)
        return True

    async def set_speed(self, v: float) -> bool:
        settings = _load_settings()
        game_dir = settings.get("game_dir")
        if not game_dir:
            return False
        try:
            with open(os.path.join(game_dir, CTL_FILE), "w") as f:
                f.write("%.2f\n" % float(v))
            return True
        except Exception as e:
            decky.logger.error("set_speed failed: %s", e)
            return False

    async def install_dll(self) -> dict:
        settings = _load_settings()
        game_dir = settings.get("game_dir")
        if not game_dir:
            return {"ok": False, "error": "game_dir not set"}

        src = os.path.join(decky.DECKY_PLUGIN_DIR, DLL_NAME)
        dst_dir = os.path.join(game_dir, "Data", "NVSE", "Plugins")
        dst = os.path.join(dst_dir, DLL_NAME)
        try:
            os.makedirs(dst_dir, exist_ok=True)
            shutil.copyfile(src, dst)
            ini = os.path.join(dst_dir, "FNVSpeedhack.ini")
            if not os.path.isfile(ini):
                with open(ini, "w") as f:
                    f.write(DEFAULT_INI)
            decky.logger.info("DLL installed: %s", dst)
            return {"ok": True, "path": dst}
        except Exception as e:
            decky.logger.error("install_dll failed: %s", e)
            return {"ok": False, "error": str(e)}

    async def _main(self):
        decky.logger.info("FNV Speed plugin loaded")

    async def _unload(self):
        decky.logger.info("FNV Speed plugin unloaded")

    async def _uninstall(self):
        pass
