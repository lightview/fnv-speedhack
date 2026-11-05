import os
import shutil

APPS = ["22380", "22490"]

# appended as a new global section; wine merges duplicate keys safely
BLOCK = (
    "\n[Software\\\\Wine\\\\DllOverrides] 1793970000\n"
    '"dinput8"="native,builtin"\n'
    '"nvse_steam_loader.dll"="native,builtin"\n'
)

for app in APPS:
    p = os.path.expanduser(
        "~/.local/share/Steam/steamapps/compatdata/%s/pfx/user.reg" % app)
    if not os.path.isfile(p):
        print("SKIP %s: no user.reg" % app)
        continue
    s = open(p, encoding="utf-8", errors="replace").read()
    if 'dinput8"="native' in s:
        print("SKIP %s: dinput8 override already present" % app)
        continue
    shutil.copy(p, p + ".bak-fnvspeed")
    if not s.endswith("\n"):
        s += "\n"
    open(p, "a", encoding="utf-8").write(BLOCK)
    print("OK %s: dinput8 override appended (backup saved)" % app)
