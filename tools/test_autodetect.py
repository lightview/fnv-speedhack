import os
import re
import glob

home = "/home/deck"
roots = [
    os.path.join(home, ".local", "share", "Steam"),
    os.path.join(home, ".steam", "steam"),
    os.path.join(home, ".steam", "debian-installation"),
    os.path.join(home, ".steam", "root"),
]
roots += glob.glob("/run/media/*/*")
seen, out = set(), []
for r in roots:
    if r and r not in seen and os.path.isdir(r):
        seen.add(r)
        out.append(r)
print("ROOTS:", out)

for root in out:
    vdf = os.path.join(root, "steamapps", "libraryfolders.vdf")
    paths = [root]
    try:
        with open(vdf, encoding="utf-8", errors="replace") as f:
            for m in re.finditer(r'"path"\s+"([^"]+)"', f.read()):
                p = m.group(1).replace("\\\\", "\\")
                if p not in paths:
                    paths.append(p)
    except Exception as e:
        print("  vdf fail:", root, e)
    for lib in paths:
        for sub in ["Fallout New Vegas", "Fallout New Vegas enplczru"]:
            gd = os.path.join(lib, "steamapps", "common", sub)
            exe = os.path.join(gd, "FalloutNV.exe")
            if os.path.exists(os.path.dirname(gd)):
                print("CHECK:", lib, "|", sub, "->", os.path.isfile(exe))
