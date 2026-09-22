# Minimal STM32F0 RCC: every "xxxON" enable bit immediately reports "xxxRDY",
# and the clock switch SW is mirrored into SWS. All other bits are plain storage.
# offsets: CR 0x00, CFGR 0x04, BDCR 0x20, CSR 0x24, CR2 0x34
MIRROR = {
    0x00: [(0, 1), (16, 17), (24, 25)],   # HSI, HSE, PLL
    0x20: [(0, 1)],                        # LSE
    0x24: [(0, 1)],                        # LSI
    0x34: [(0, 1), (16, 17)],              # HSI14, HSI48
}
if request.IsInit:
    regs = {0x00: 0x83}
elif request.IsRead:
    v = regs.get(request.Offset, 0)
    if request.Offset == 0x04:
        v = (v & ~0xC) | ((v & 0x3) << 2)      # SWS := SW
    for on, rdy in MIRROR.get(request.Offset, []):
        if v & (1 << on):
            v |= (1 << rdy)
        else:
            v &= ~(1 << rdy)
    request.Value = v
elif request.IsWrite:
    regs[request.Offset] = request.Value
