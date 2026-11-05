import os
import re
import shutil

path = os.path.expanduser(
    "~/.local/share/Steam/userdata/109517971/config/localconfig.vdf")
LO = "WINEDLLOVERRIDES=nvse_steam_loader.dll=n,b %command%"

s = open(path, encoding="utf-8", errors="replace").read()

# find the app-config block: `"22380"` + `{` whose body has LastPlayed/LaunchOptions
best = None
for m in re.finditer(r'"22380"\s*\{', s):
    j = m.end() - 1          # at the opening brace
    depth = 0
    k = j
    while k < len(s):
        if s[k] == '{':
            depth += 1
        elif s[k] == '}':
            depth -= 1
            if depth == 0:
                break
        k += 1
    block = s[j:k + 1]
    if 'LastPlayed' in block or 'LaunchOptions' in block:
        best = (j, k)
        break

if best is None:
    print("APP_BLOCK_NOT_FOUND")
    raise SystemExit(1)

j, k = best
block = s[j:k + 1]
if '"LaunchOptions"' in block:
    newblock = re.sub(r'"LaunchOptions"\s*"[^"]*"',
                      '"LaunchOptions"\t\t"' + LO + '"', block, count=1)
else:
    newblock = '{\n\t\t\t\t\t\t"LaunchOptions"\t\t"' + LO + '"' + block[1:]

s2 = s[:j] + newblock + s[k + 1:]

shutil.copy(path, path + ".bak-fnvspeed-20261105")
open(path, "w", encoding="utf-8").write(s2)
print("OK: LaunchOptions set for app 22380")
