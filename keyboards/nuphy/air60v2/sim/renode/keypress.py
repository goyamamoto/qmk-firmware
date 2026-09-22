# Key matrix model for the Air60 V2 under Renode (include @keypress.py).
# The firmware drives one row low at a time (16-bit writes to GPIOx_BSRR:
# offset 0x18 sets, 0x1A clears) and reads the column inputs, which idle
# high through pull-ups. For every pressed (row, col) this module pulls the
# column pin low while its row is selected. Exposed as module `air60`:
#     air60.press(row, col) / air60.release(row, col) / air60.release_all()
import sys, types

ROWS = [("gpioPortC", 14), ("gpioPortC", 15), ("gpioPortA", 0), ("gpioPortA", 1), ("gpioPortA", 2), ("gpioPortA", 3)]
COLS = [("gpioPortA", 4), ("gpioPortA", 5), ("gpioPortA", 6), ("gpioPortA", 7), ("gpioPortB", 0), ("gpioPortB", 1),
        ("gpioPortB", 10), ("gpioPortB", 11), ("gpioPortB", 12), ("gpioPortB", 13), ("gpioPortB", 14), ("gpioPortB", 15),
        ("gpioPortA", 8), ("gpioPortA", 9), ("gpioPortA", 10), ("gpioPortA", 15), ("gpioPortB", 3)]
BASE = {"gpioPortA": 0x48000000, "gpioPortB": 0x48000400, "gpioPortC": 0x48000800}

m = types.ModuleType("air60")
sys.modules["air60"] = m
m.machine = monitor.Machine
m.ports = dict((name, m.machine["sysbus." + name]) for name in BASE)
m.row_low = [False] * len(ROWS)
m.pressed = set()
m.scans = 0

def apply_cols():
    for c, (port, pin) in enumerate(COLS):
        low = any(m.row_low[r] for (r, cc) in m.pressed if cc == c)
        m.ports[port].OnGPIO(pin, not low)
m.apply_cols = apply_cols

def on_row_write(port, offset, value):
    bits = int(value) & 0xFFFF
    changed = False
    for r, (rport, pin) in enumerate(ROWS):
        if rport != port or not (bits & (1 << pin)):
            continue
        low = (offset == 0x1A)   # 0x1A clears the pin (row selected), 0x18 sets it
        if m.row_low[r] != low:
            m.row_low[r] = low
            changed = True
        if low:
            m.scans += 1
    if changed and m.pressed:
        apply_cols()
m.on_row_write = on_row_write

def press(row, col):
    m.pressed.add((row, col))
    apply_cols()
m.press = press

def release(row, col):
    m.pressed.discard((row, col))
    apply_cols()
m.release = release

def release_all():
    m.pressed.clear()
    apply_cols()
m.release_all = release_all

def make_hook(port, off):
    def hook(cpu, address, width, value):
        on_row_write(port, off, value)
    return hook

for name in ("gpioPortA", "gpioPortC"):
    for off in (0x18, 0x1A):
        m.machine.SystemBus.AddWatchpointHook(BASE[name] + off, Antmicro.Renode.Peripherals.Bus.SysbusAccessWidth.Word,
            Antmicro.Renode.Peripherals.Bus.Access.Write, make_hook(name, off))
print("air60 key matrix model installed")
