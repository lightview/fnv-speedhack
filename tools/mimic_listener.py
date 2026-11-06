import glob
import os
import select
import time

VALVE = "000028DE:00001205"
REPORT = 0x09

def deck_hidraw_nodes():
    nodes = []
    for path in sorted(glob.glob("/sys/class/hidraw/hidraw*")):
        try:
            with open(os.path.join(path, "device", "uevent"), "r") as f:
                if VALVE in f.read().upper():
                    nodes.append("/dev/" + os.path.basename(path))
        except OSError:
            continue
    return nodes

def open_input_node():
    for node in deck_hidraw_nodes():
        print("trying", node, flush=True)
        fd = os.open(node, os.O_RDONLY | os.O_NONBLOCK)
        deadline = time.monotonic() + 0.5
        got = False
        while time.monotonic() < deadline:
            ready, _, _ = select.select([fd], [], [], 0.1)
            if not ready:
                continue
            try:
                report = os.read(fd, 64)
            except OSError:
                break
            if len(report) >= 16 and report[2] == REPORT:
                print("  -> streams 0x09, CHOSEN", flush=True)
                return fd
            if len(report):
                print("   report id", hex(report[2]), flush=True)
        print("  -> no 0x09, skip", flush=True)
        os.close(fd)
    return None

fd = open_input_node()
if fd is None:
    print("NO NODE CHOSEN", flush=True)
    raise SystemExit
print("listening 25s on chosen node - PRESS R4", flush=True)
last = None
deadline = time.monotonic() + 25
while time.monotonic() < deadline:
    ready, _, _ = select.select([fd], [], [], 0.5)
    if not ready:
        continue
    try:
        rep = os.read(fd, 64)
    except OSError as e:
        print("read err", e, flush=True)
        break
    if len(rep) < 16 or rep[2] != REPORT:
        continue
    cur = (rep[9], rep[10], rep[13])
    if cur != last:
        print(f"b9={cur[0]:02x} b10={cur[1]:02x} b13={cur[2]:02x}", flush=True)
        last = cur
os.close(fd)
