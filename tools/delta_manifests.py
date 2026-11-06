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

new = {k: v for k, v in pc.items() if k not in deck}
changed = {k: v for k, v in pc.items() if k in deck and deck[k] != v}
same = [k for k in pc if k in deck and deck[k] == pc[k]]
stale = set(deck) - set(pc)

gb = 1024 ** 3
mb = 1024 ** 2


def top(items, n=8):
    return sorted(items.items(), key=lambda kv: -kv[1])[:n]


print(f"PC files: {len(pc)}, Deck files: {len(deck)}")
print(f"identical (by size): {len(same)}")
print(f"NEW on PC (absent on Deck): {len(new)} files, {sum(new.values())/gb:.2f} GB")
print(f"CHANGED (size differs): {len(changed)} files, {sum(changed.values())/gb:.2f} GB")
print(f"STALE on Deck (absent on PC): {len(stale)} files")
print()
print("transfer total (new+changed):",
      f"{(sum(new.values())+sum(changed.values()))/gb:.2f} GB")
print()
print("top NEW:")
for k, v in top(new):
    print(f"  {v/mb:8.1f} MB  {k[:90]}")
print("top CHANGED:")
for k, v in top(changed):
    print(f"  {v/mb:8.1f} MB  {k[:90]}")
