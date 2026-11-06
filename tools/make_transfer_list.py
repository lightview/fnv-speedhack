import sys


def load(path):
    out = {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            try:
                size, _mtime, rel = line.rstrip("\n").split("\t", 2)
                out[rel] = int(size)
            except ValueError:
                continue
    return out


pc = load("pc_manifest.tsv")
deck = load("deck_manifest.tsv")

EXCLUDE_PREFIXES = (
    "_dev/",          # our toolchain/repo, not part of the game
    "Saves/",
    ".zcode/",        # zcode session plans inside the game dir
)
EXCLUDE_NAMES = ("fnvspeed",)


def excluded(rel):
    if rel.startswith(EXCLUDE_PREFIXES):
        return True
    base = rel.rsplit("/", 1)[-1]
    return any(part in base for part in EXCLUDE_NAMES)


transfer = []
for rel, _size in pc.items():
    if excluded(rel):
        continue
    if rel not in deck or deck[rel] != pc[rel]:
        transfer.append(rel)

stale = [rel for rel in deck
         if not excluded(rel) and rel not in pc]

with open("transfer_list.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(transfer))
with open("stale_list.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(stale))

total = sum(pc[r] for r in transfer)
print(f"transfer: {len(transfer)} files, {total/1024**3:.2f} GB")
print(f"stale to delete on deck: {len(stale)} files")
