reports = [(cmd, p) for (cmd, p) in nrf["frames"] if 0xE0 <= cmd <= 0xE4]
print("key reports over UART: %d (matrix scans with a row selected: %d)" % (len(reports), sys.modules["air60"].scans))
seen = []
for cmd, p in reports:
    s = "%s %s" % (CMD_NAMES.get(cmd), hexs(p))
    if not seen or seen[-1][0] != s:
        seen.append([s, 1])
    else:
        seen[-1][1] += 1
for s, n in seen:
    print("  %s  x%d" % (s, n))
