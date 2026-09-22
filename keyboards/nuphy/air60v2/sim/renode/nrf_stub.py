# Fake nRF module for the NuPhy Air60 V2 firmware under Renode.
# Runs inside the Renode monitor (include @nrf_stub.py) and answers the
# keyboard's UART protocol on usart1: 3-byte ACKs, the 32-byte config table
# for CMD_READ_DATA and a "connected on 2.4G, 85 % battery" status for
# CMD_RF_STS_SYSC. Every frame the keyboard sends is recorded in `frames`.
machine = monitor.Machine
uart = machine["sysbus.usart1"]

CMD_NAMES = {
    0xF0: "POWER_UP", 0xF1: "SLEEP", 0xF2: "HAND", 0xF3: "SNIF", 0xF4: "24G_SUSPEND", 0xFE: "IDLE_EXIT",
    0xE0: "RPT_MS", 0xE1: "RPT_BYTE_KB", 0xE2: "RPT_BIT_KB", 0xE3: "RPT_CONSUME", 0xE4: "RPT_SYS",
    0xC0: "SET_LINK", 0xC1: "SET_CONFIG", 0xC2: "GET_CONFIG", 0xC3: "SET_NAME", 0xC4: "GET_NAME",
    0xC5: "CLR_DEVICE", 0xC7: "NEW_ADV", 0xC9: "RF_STS_SYSC", 0xCA: "SET_24G_NAME", 0xCF: "GO_TEST",
    0xB1: "RF_DFU", 0x80: "WRITE_DATA", 0x81: "READ_DATA",
}
LINK_RF_24, RF_CONNECT = 0, 3

nrf = {
    "rx": [],            # bytes of the frame being assembled
    "frames": [],        # (cmd, payload bytes) in arrival order
    "link_mode": LINK_RF_24,
    "rf_state": RF_CONNECT,
    "battery": 85,
    "charge": 0,
    "sent": 0,
}

def vtime():
    try:
        return str(machine.ElapsedVirtualTime.TimeElapsed)[:12]
    except Exception:
        return "?"

def hexs(bs):
    return " ".join("%02X" % b for b in bs)

def send(bs):
    for b in bs:
        uart.WriteChar(b)
    nrf["sent"] += len(bs)

def reply(cmd, payload):
    frame = [0x5A, cmd, 0x00, len(payload)] + list(payload) + [sum(payload) & 0xFF]
    send(frame)

def ack(cmd):
    send([0x5A, cmd, 0xA0])

def handle(cmd, payload):
    name = CMD_NAMES.get(cmd, "0x%02X" % cmd)
    nrf["frames"].append((cmd, payload))
    if cmd == 0x81:                       # READ_DATA -> 32-byte func_tab
        tab = [0] * 32
        tab[4] = nrf["link_mode"]
        tab[5] = 0                        # rf_channel
        tab[6] = 1                        # ble_channel
        reply(cmd, tab)
    elif cmd == 0xC9:                     # RF_STS_SYSC -> link, state, led, charge, battery
        link = payload[0] if payload else nrf["link_mode"]
        reply(cmd, [link, nrf["rf_state"], 0x00, nrf["charge"], nrf["battery"]])
    elif cmd == 0xC0:                     # SET_LINK: follow the requested mode
        if payload:
            nrf["link_mode"] = payload[0]
        ack(cmd)
    elif 0xE0 <= cmd <= 0xE4:             # HID reports: no answer
        pass
    else:
        ack(cmd)
    print("nrf %s <- %-12s %s" % (vtime(), name, hexs(payload)))

def on_char(b):
    b = int(b)
    rx = nrf["rx"]
    if not rx and b != 0x5A:
        return
    rx.append(b)
    if len(rx) >= 4 and len(rx) == rx[3] + 5:
        cmd, payload = rx[1], rx[4:4 + rx[3]]
        nrf["rx"] = []
        handle(cmd, payload)

uart.CharReceived += on_char
print("nrf stub attached to usart1")
