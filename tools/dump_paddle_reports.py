import glob
import os
import select
import time

nodes = []
for path in sorted(glob.glob("/sys/class/hidraw/hidraw*")):
    try:
        with open(os.path.join(path, "device", "uevent")) as f:
            if "000028DE:00001205" in f.read().upper():
                nodes.append("/dev/" + os.path.basename(path))
    except OSError:
        continue

print("valve nodes:", nodes, flush=True)

fds = {}
for node in nodes:
    try:
        fds[os.open(node, os.O_RDONLY | os.O_NONBLOCK)] = node
    except OSError as e:
        print("open fail", node, e, flush=True)

print("reading 60s - PRESS R4 NOW", flush=True)
deadline = time.monotonic() + 60
last = {}
while time.monotonic() < deadline:
    ready, _, _ = select.select(list(fds.keys()), [], [], 0.2)
    for fd in ready:
        try:
            rep = os.read(fd, 64)
        except OSError as e:
            print(fds[fd], "read err", e, flush=True)
            continue
        if len(rep) < 16:
            continue
        rid = rep[2]
        if rid == 0x09:
            cur = (rep[9], rep[10], rep[13], rep[8], rep[11])
            if cur != last.get(fd):
                print(f"{fds[fd]} id09 b8={rep[8]:02x} b9={rep[9]:02x} "
                      f"b10={rep[10]:02x} b11={rep[11]:02x} b13={rep[13]:02x}",
                      flush=True)
                last[fd] = cur
print("done", flush=True)
