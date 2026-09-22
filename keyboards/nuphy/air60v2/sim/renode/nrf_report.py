from collections import OrderedDict
counts = OrderedDict()
for cmd, payload in nrf["frames"]:
    name = CMD_NAMES.get(cmd, "0x%02X" % cmd)
    counts[name] = counts.get(name, 0) + 1
print("nrf summary: %d frames from keyboard, %d bytes sent back" % (len(nrf["frames"]), nrf["sent"]))
for name, n in counts.items():
    print("  %-12s x%d" % (name, n))
