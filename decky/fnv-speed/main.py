import os
import re
import json
import glob
import shutil
import asyncio
import select
import threading
import time

import decky

DLL_NAME = "FNVSpeedhack.dll"
PROXY_NAME = "dinput8.dll"   # the speedhack ships as a dinput8 proxy on Deck
CTL_FILE = "fnvspeed.txt"
STATUS_FILE = "fnvspeed_status.txt"
GAME_EXE = "FalloutNV.exe"
# Steam installs localized FNV depots into suffixed folders; which one runs
# depends on the account language (e.g. "russian" -> enplczru)
GAME_SUBDIRS = ["Fallout New Vegas", "Fallout New Vegas enplczru"]

# Steam Deck controller hidraw: L4/L5/R4/R5 in the 0x09 input report
# (same layout as the kernel's hid-steam.c). Read straight from the device,
# so paddle presses work with no Steam Input bindings at all.
VALVE_DECK_HID_ID = "000028DE:00001205"
DECK_INPUT_REPORT = 0x09
PAD_BUTTONS = {"L4": (13, 1), "R4": (13, 2), "L5": (9, 7), "R5": (10, 0)}
PAD_SPEEDS = {"L4": 1.0, "L5": 3.0, "R4": 5.0, "R5": 10.0}
TOGGLE_COOLDOWN = 0.6

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


def _game_dirs_in_library(library: str) -> list[str]:
    out = []
    for sub in GAME_SUBDIRS:
        game_dir = os.path.join(library, "steamapps", "common", sub)
        if os.path.isfile(os.path.join(game_dir, GAME_EXE)):
            out.append(game_dir)
    return out


def _detect_candidates() -> list[str]:
    found: list[str] = []
    for root in _steam_roots():
        for library in _library_paths(root):
            for gd in _game_dirs_in_library(library):
                real = os.path.realpath(gd)
                if real not in found:
                    found.append(real)
    return found


def _pick_candidate(candidates: list[str]) -> str | None:
    """Prefer the localized copy (e.g. enplczru) when the account plays one."""
    for c in candidates:
        if c.rstrip("/").endswith("enplczru"):
            return c
    return candidates[0] if candidates else None


def _patch_prefix_overrides(game_dir: str) -> list[str]:
    """Add dinput8=native,builtin to the Wine overrides of every Proton
    prefix whose appmanifest installdir matches this game copy. Wine merges
    duplicate [DllOverrides] sections, so appending is safe. Returns appids."""
    installdir = os.path.basename(game_dir.rstrip("/"))
    steamapps = os.path.dirname(os.path.dirname(game_dir))  # ../.. of common/
    compatdata = os.path.join(os.path.dirname(steamapps), "compatdata")

    appids = []
    try:
        for entry in os.listdir(steamapps):
            m = re.match(r"appmanifest_(\d+)\.acf$", entry)
            if not m:
                continue
            try:
                with open(os.path.join(steamapps, entry), encoding="utf-8",
                          errors="replace") as f:
                    body = f.read()
            except OSError:
                continue
            if re.search(r'"installdir"\s+"' + re.escape(installdir) + '"', body):
                appids.append(m.group(1))
    except OSError:
        return []

    patched = []
    for appid in appids:
        reg = os.path.join(compatdata, appid, "pfx", "user.reg")
        if not os.path.isfile(reg):
            continue
        try:
            with open(reg, encoding="utf-8", errors="replace") as f:
                body = f.read()
            if '"dinput8"="native' in body:
                patched.append(appid)
                continue
            shutil.copy(reg, reg + ".bak-fnvspeed")
            block = ('\n[Software\\\\Wine\\\\DllOverrides] %d\n'
                     '"dinput8"="native,builtin"\n') % int(time.time())
            if not body.endswith("\n"):
                body += "\n"
            with open(reg, "a", encoding="utf-8") as f:
                f.write(block)
            patched.append(appid)
        except OSError as exc:
            decky.logger.warning("prefix %s patch failed: %s", appid, exc)
    return patched


def _deck_hidraw_nodes() -> list[str]:
    nodes = []
    for path in sorted(glob.glob("/sys/class/hidraw/hidraw*")):
        try:
            with open(os.path.join(path, "device", "uevent"), "r") as f:
                if VALVE_DECK_HID_ID in f.read().upper():
                    nodes.append("/dev/" + os.path.basename(path))
        except Exception:  # unreadable/odd uevent - just skip the node
            continue
    return nodes


class PadListener(threading.Thread):
    """Fires on_speed(name) once per press of a back paddle, from hidraw."""

    def __init__(self, on_speed, log):
        # NOTE: target=self._run is mandatory - without it Thread.run()
        # returns immediately and the listener never executes.
        super().__init__(name="FnvSpeedPaddles", daemon=True, target=self._run)
        self._on_speed = on_speed
        self._log = log
        self._stop = threading.Event()

    def stop(self):
        self._stop.set()
        self.join(timeout=2)

    def _run(self):
        try:
            self._run_inner()
        except BaseException as exc:
            # a dead listener must never be silent
            self._log(f"[-] paddle listener crashed: {exc!r}")

    def _run_inner(self):
        fds = {}   # fd -> node path
        announced = False
        was_down = None
        last_toggle = 0.0
        while not self._stop.is_set():
            if not fds:
                nodes = _deck_hidraw_nodes()
                if not nodes:
                    if not announced:
                        self._log("[!] Steam Deck controller hidraw not found; paddle speed keys off.")
                        announced = True
                    self._stop.wait(3.0)
                    continue
                for node in nodes:  # listen to ALL of them: only one streams
                    try:
                        fds[os.open(node, os.O_RDONLY | os.O_NONBLOCK)] = node
                    except OSError:
                        pass
                if not fds:
                    self._stop.wait(3.0)
                    continue
                announced = False
                was_down = None
                self._log(f"[pad] listening on {len(fds)} hidraw nodes")
            try:
                ready, _, _ = select.select(list(fds.keys()), [], [], 0.5)
                if not ready:
                    continue
                report = None
                for fd in ready:
                    try:
                        report = os.read(fd, 64)
                    except OSError:
                        report = None
                    if report:
                        break
            except OSError:
                for fd in fds:
                    os.close(fd)
                fds = {}  # suspend/resume re-enumerates the controller
                self._stop.wait(1.0)
                continue
            if not report or len(report) < 16 or report[2] != DECK_INPUT_REPORT:
                continue
            now = time.monotonic()
            down = {name for name, (byte, bit) in PAD_BUTTONS.items()
                    if report[byte] & (1 << bit)}
            if down != was_down and down and now - last_toggle >= TOGGLE_COOLDOWN:
                last_toggle = now
                for name in sorted(down):
                    try:
                        self._on_speed(name)
                    except Exception as exc:
                        self._log(f"[-] paddle action failed: {exc}")
            was_down = down
        for fd in fds:
            os.close(fd)


class Plugin:
    async def get_status(self) -> dict:
        settings = _load_settings()
        game_dir = settings.get("game_dir") or None

        # self-heal: on first run pick the game automatically, no manual path
        if not game_dir:
            candidates = await self.autodetect()
            best = _pick_candidate(candidates)
            if best:
                settings["game_dir"] = best
                _save_settings(settings)
                game_dir = best
                decky.logger.info("auto-selected game dir: %s", best)

        dll_installed = False
        speed = None
        if game_dir:
            dll_installed = (
                os.path.isfile(os.path.join(game_dir, PROXY_NAME))       # dinput8 proxy
                or os.path.isfile(os.path.join(game_dir, "Data", "NVSE",
                                               "Plugins", DLL_NAME)))    # NVSE plugin
            try:
                with open(os.path.join(game_dir, STATUS_FILE), "r") as f:
                    speed = float(f.read().strip())
            except Exception:
                speed = None

        return {
            "game_dir": game_dir,
            "dll_installed": dll_installed,
            "speed": speed,
            "notify": bool(settings.get("notify", True)),
            "candidates": await self.autodetect(),
        }

    async def set_notify(self, v: bool) -> bool:
        settings = _load_settings()
        settings["notify"] = bool(v)
        _save_settings(settings)
        return bool(v)

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
        """One-click setup: dinput8 proxy + ini + Wine DLL override in the
        Proton prefix. Works on any FNV exe version (no NVSE needed)."""
        settings = _load_settings()
        game_dir = settings.get("game_dir")
        if not game_dir:
            return {"ok": False, "error": "game_dir not set"}

        src = os.path.join(decky.DECKY_PLUGIN_DIR, PROXY_NAME)
        dst = os.path.join(game_dir, PROXY_NAME)
        try:
            if not os.path.isfile(src):
                return {"ok": False, "error": f"{PROXY_NAME} missing from plugin"}
            shutil.copyfile(src, dst)
            ini = os.path.join(game_dir, "FNVSpeedhack.ini")
            if not os.path.isfile(ini):
                with open(ini, "w") as f:
                    f.write(DEFAULT_INI)
            prefixes = _patch_prefix_overrides(game_dir)
            decky.logger.info("speedhack installed: %s (prefixes: %s)", dst, prefixes)
            return {"ok": True, "path": dst, "prefixes": prefixes}
        except Exception as e:
            decky.logger.error("install_dll failed: %s", e)
            return {"ok": False, "error": str(e)}

    async def _main(self):
        self.loop = asyncio.get_running_loop()
        self.paddles = PadListener(self._on_paddle, decky.logger.info)
        self.paddles.start()
        decky.logger.info("FNV Speed plugin loaded")

    def _on_paddle(self, name: str) -> None:
        speed = PAD_SPEEDS.get(name)
        if not speed:
            return
        ok = asyncio.run_coroutine_threadsafe(
            self.set_speed(speed), self.loop).result(timeout=5)
        decky.logger.info("paddle %s -> %.2fx (%s)",
                          name, speed, "written" if ok else "no game dir")

    async def _unload(self):
        self.paddles.stop()
        decky.logger.info("FNV Speed plugin unloaded")

    async def _uninstall(self):
        self.paddles.stop()
        pass
