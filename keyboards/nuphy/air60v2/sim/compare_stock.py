#!/usr/bin/env python3
"""Static comparison of the Air60 V2 port against its sources.

Compares keyboards/nuphy/air60v2/ansi against

  * the NuPhy stock firmware (nuphy-src/qmk_firmware, keyboards/nuphy/air60_v2/ansi)
    for every hardware fact: pins, I2C addresses, LED driver table, LED
    coordinates, matrix layout, keymap, VIA definition, side light tables.
  * the Halo75 V2 in this tree, whose matrix code and pins the port reuses.

Usage:
  keyboards/nuphy/air60v2/sim/compare_stock.py --stock /path/to/nuphy-src/qmk_firmware

The stock tree is https://github.com/nuphy-src/qmk_firmware (branch
nuphy-keyboards); it is not part of this repository. --stock defaults to
$NUPHY_STOCK_QMK, then ../refs/nuphy/qmk_firmware next to this tree.

Exit status is 1 when any FAIL is reported.  WARN and INFO lines are
differences that are intentional or cosmetic and only need a human look.
"""

import argparse
import filecmp
import json
import os
import re
import sys
from pathlib import Path

# --- report ---------------------------------------------------------------

results = {"PASS": 0, "FAIL": 0, "WARN": 0, "INFO": 0}


def report(level, check, detail=""):
    results[level] += 1
    line = f"[{level}] {check}"
    if detail:
        line += f": {detail}"
    print(line)


def check(cond, name, detail_fail="", detail_pass=""):
    if cond:
        report("PASS", name, detail_pass)
    else:
        report("FAIL", name, detail_fail)
    return cond


# --- parsing helpers ------------------------------------------------------


def strip_c_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def parse_defines(path):
    """Single-line #define NAME VALUE -> {NAME: VALUE} (comments stripped)."""
    out = {}
    for line in strip_c_comments(path.read_text()).splitlines():
        m = re.match(r"\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\s*(.*?)\s*$", line)
        if m and "(" not in m.group(1):
            out[m.group(1)] = re.sub(r"\s+", " ", m.group(2))
    return out


def parse_enum(path, name="custom_keycodes"):
    text = strip_c_comments(path.read_text())
    m = re.search(r"enum\s+" + name + r"\s*\{(.*?)\}", text, re.S)
    if not m:
        raise SystemExit(f"enum {name} not found in {path}")
    ids = []
    for item in m.group(1).split(","):
        item = item.strip()
        if not item:
            continue
        ids.append(item.split("=")[0].strip())
    return ids


def split_top_level(text):
    """Split on commas that are not inside parentheses."""
    parts, depth, cur = [], 0, []
    for ch in text:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append("".join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    tail = "".join(cur).strip()
    if tail:
        parts.append(tail)
    return parts


def parse_keymap_layers(path):
    """Return {layer: (macro_name, [keycodes])} from a keymap.c."""
    text = strip_c_comments(path.read_text())
    m = re.search(r"keymaps\s*\[\]\s*\[MATRIX_ROWS\]\s*\[MATRIX_COLS\]\s*=\s*\{", text)
    if not m:
        raise SystemExit(f"keymaps array not found in {path}")
    body = text[m.end():]
    layers = {}
    pos = 0
    while True:
        lm = re.search(r"\[(\d+)\]\s*=\s*(LAYOUT\w*)\s*\(", body[pos:])
        if not lm:
            break
        layer = int(lm.group(1))
        macro = lm.group(2)
        start = pos + lm.end()
        depth, i = 1, start
        while depth:
            if body[i] == "(":
                depth += 1
            elif body[i] == ")":
                depth -= 1
            i += 1
        args = split_top_level(body[start:i - 1])
        layers[layer] = (macro, [re.sub(r"\s+", "", a) for a in args])
        pos = i
    return layers


LED_MACRO_RE = re.compile(r"\{\s*(\d+)\s*,\s*([A-Za-z0-9_]+)\s*,\s*([A-Za-z0-9_]+)\s*,\s*([A-Za-z0-9_]+)\s*\}")


def norm_led_reg(name):
    """Old driver aliases A_1..L_16 -> SW1_CS1..SW12_CS16."""
    m = re.fullmatch(r"([A-L])_(\d+)", name)
    if m:
        return f"SW{ord(m.group(1)) - ord('A') + 1}_CS{m.group(2)}"
    return name


def parse_led_table(path, symbol):
    text = strip_c_comments(path.read_text())
    m = re.search(symbol + r"\s*\[[^\]]*\]\s*=\s*\{(.*?)\n\};", text, re.S)
    if not m:
        raise SystemExit(f"{symbol} not found in {path}")
    return [(int(d), norm_led_reg(r), norm_led_reg(g), norm_led_reg(b)) for d, r, g, b in LED_MACRO_RE.findall(m.group(1))]


def parse_side_pairs(path, symbol):
    text = strip_c_comments(path.read_text())
    m = re.search(symbol + r"\s*\[[^\]]*\]\s*\[2\]\s*=\s*\{(.*?)\};", text, re.S)
    if not m:
        raise SystemExit(f"{symbol} not found in {path}")
    side_index = int(re.search(r"#define\s+SIDE_INDEX\s+(\d+)", text).group(1))
    pairs = re.findall(r"\{\s*SIDE_INDEX\s*\+\s*(\d+)\s*,\s*SIDE_INDEX\s*\+\s*(\d+)\s*\}", m.group(1))
    return [(side_index + int(a), side_index + int(b)) for a, b in pairs]


def parse_side_list(path, symbol):
    text = strip_c_comments(path.read_text())
    m = re.search(symbol + r"\s*\[[^\]]*\]\s*=\s*\{(.*?)\};", text, re.S)
    side_index = int(re.search(r"#define\s+SIDE_INDEX\s+(\d+)", text).group(1))
    return [side_index + int(x) for x in re.findall(r"SIDE_INDEX\s*\+\s*(\d+)", m.group(1))]


def rgb_entry_key(e):
    return (tuple(e.get("matrix", [])) or None, e.get("x"), e.get("y"), e.get("flags"))


def layout_key(e):
    return (tuple(e["matrix"]), e.get("x"), e.get("y"), e.get("w", 1), e.get("h", 1))


# --- main -----------------------------------------------------------------


def main():
    ap = argparse.ArgumentParser()
    qmk_root = Path(__file__).resolve().parents[4]
    ap.add_argument("--stock", default=os.environ.get("NUPHY_STOCK_QMK") or str(qmk_root.parent / "refs/nuphy/qmk_firmware"),
                    help="root of the nuphy-src qmk_firmware clone")
    args = ap.parse_args()

    N = Path(args.stock) / "keyboards/nuphy/air60_v2/ansi"
    R = qmk_root / "keyboards/nuphy/air60v2/ansi"
    H = qmk_root / "keyboards/nuphy/halo75v2/ansi"
    C = qmk_root / "keyboards/nuphy/common"
    for p in (N, R, H, C):
        if not p.is_dir():
            raise SystemExit(f"missing directory: {p} (use --stock for the nuphy-src tree)")

    n_kb = json.loads((N / "keyboard.json").read_text())
    r_kb = json.loads((R / "keyboard.json").read_text())
    h_kb = json.loads((H / "keyboard.json").read_text())
    n_cfg = parse_defines(N / "config.h")
    r_cfg = parse_defines(R / "config.h")
    h_cfg = parse_defines(H / "config.h")

    print("== keyboard.json: identity and matrix")
    for key in ("vid", "pid"):
        check(n_kb["usb"][key].lower() == r_kb["usb"][key].lower(), f"usb.{key}", f"{n_kb['usb'][key]} vs {r_kb['usb'][key]}", r_kb["usb"][key])
    for key in ("processor", "bootloader", "diode_direction"):
        check(n_kb[key] == r_kb[key], key, f"{n_kb[key]} vs {r_kb[key]}", r_kb[key])
    check(n_kb["matrix_pins"] == r_kb["matrix_pins"], "matrix_pins equal stock", f"{n_kb['matrix_pins']} vs {r_kb['matrix_pins']}")
    check(h_kb["matrix_pins"] == r_kb["matrix_pins"], "matrix_pins equal Halo75 V2 (matrix.c reuse premise)")
    for key in ("device_version",):
        if n_kb["usb"].get(key) != r_kb["usb"].get(key):
            report("INFO", f"usb.{key} differs", f"stock {n_kb['usb'].get(key)} / port {r_kb['usb'].get(key)}")
    for key in ("debounce", "features"):
        if n_kb.get(key) != r_kb.get(key):
            report("INFO", f"{key} differs (intentional, common layer)", f"stock {n_kb.get(key)} / port {r_kb.get(key)}")

    print("== keyboard.json: physical layout (LAYOUT vs LAYOUT_ansi_64)")
    n_layout = n_kb["layouts"]["LAYOUT"]["layout"]
    r_layout_name = next(iter(r_kb["layouts"]))
    r_layout = r_kb["layouts"][r_layout_name]["layout"]
    check(len(n_layout) == len(r_layout), "layout key count", f"{len(n_layout)} vs {len(r_layout)}", str(len(r_layout)))
    bad = [(i, layout_key(a), layout_key(b)) for i, (a, b) in enumerate(zip(n_layout, r_layout)) if layout_key(a) != layout_key(b)]
    check(not bad, "layout matrix/x/y/w per key", "; ".join(f"#{i} {a} vs {b}" for i, a, b in bad[:5]))
    label_diff = [(i, a.get("label"), b.get("label")) for i, (a, b) in enumerate(zip(n_layout, r_layout)) if a.get("label") != b.get("label")]
    if label_diff:
        report("INFO", "layout labels differ", "; ".join(f"#{i} {a!r} vs {b!r}" for i, a, b in label_diff[:8]))
    matrices = [tuple(e["matrix"]) for e in r_layout]
    check(len(set(matrices)) == len(matrices), "layout matrix positions unique")
    r_pos_of_matrix = {tuple(e["matrix"]): i for i, e in enumerate(r_layout)}
    r_label_of_matrix = {tuple(e["matrix"]): e.get("label") for e in r_layout}

    print("== rgb_matrix: LED coordinates and flags")
    n_rgb = n_kb["rgb_matrix"]["layout"]
    r_rgb = r_kb["rgb_matrix"]["layout"]
    n_keys = [e for e in n_rgb if "matrix" in e]
    n_side = [e for e in n_rgb if "matrix" not in e]
    led_count_expr = r_cfg.get("RGB_MATRIX_LED_COUNT", "")
    d1 = int(eval(r_cfg["DRIVER_1_LED_TOTAL"]))
    d2 = int(eval(r_cfg["DRIVER_2_LED_TOTAL"]))
    r_led_count = d1 + d2
    check(len(n_rgb) == r_led_count, "stock LED count == port RGB_MATRIX_LED_COUNT", f"{len(n_rgb)} vs {r_led_count} ({led_count_expr})", str(r_led_count))
    check(len(r_rgb) == len(n_keys), "port rgb layout lists exactly the key LEDs", f"{len(r_rgb)} vs {len(n_keys)}", f"{len(r_rgb)} keys, {len(n_side)} side LEDs implicit (flags 0), same convention as Halo75 V2")
    check(all(e.get("flags") == 0 for e in n_side), "stock side LEDs have flags 0 (so implicit zero entries are equivalent)")
    # Current QMK requires integer x/y here (schema: unsigned_int); the stock
    # tree used half units, so a 0.5 rounding is expected and harmless.
    bad, rounded = [], []
    for i, (a, b) in enumerate(zip(n_keys, r_rgb)):
        same_key = tuple(a["matrix"]) == tuple(b["matrix"]) and a.get("flags") == b.get("flags")
        dx, dy = abs(a["x"] - b["x"]), abs(a["y"] - b["y"])
        if not same_key or dx > 0.5 or dy > 0.5:
            bad.append((i, rgb_entry_key(a), rgb_entry_key(b)))
        elif dx or dy:
            rounded.append(f"#{i} {a['x']},{a['y']}->{b['x']},{b['y']}")
    check(not bad, "rgb layout matrix/flags per LED, x/y within 0.5", "; ".join(f"#{i} {a} vs {b}" for i, a, b in bad[:5]))
    if rounded:
        report("INFO", f"{len(rounded)} LED coordinates rounded to integers (QMK schema)", " ".join(rounded))
    check([tuple(e["matrix"]) for e in r_rgb] == [tuple(e["matrix"]) for e in r_layout], "rgb LED order == layout key order (index i lights key i)")
    for key in ("center_point", "driver"):
        check(n_kb["rgb_matrix"].get(key) == r_kb["rgb_matrix"].get(key), f"rgb_matrix.{key}")
    for key in ("max_brightness", "val_steps", "speed_steps", "led_process_limit", "led_flush_limit"):
        if n_kb["rgb_matrix"].get(key) != r_kb["rgb_matrix"].get(key):
            report("INFO", f"rgb_matrix.{key} differs", f"stock {n_kb['rgb_matrix'].get(key)} / port {r_kb['rgb_matrix'].get(key)}")
    check(n_kb["rgb_matrix"]["animations"] == r_kb["rgb_matrix"]["animations"], "rgb_matrix.animations equal")

    print("== IS31FL3733 driver table (g_is31_leds vs g_is31fl3733_leds)")
    n_leds = parse_led_table(N / "keymaps/default/keymap.c", "g_is31_leds")
    r_leds = parse_led_table(R / "ansi.c", "g_is31fl3733_leds")
    check(len(n_leds) == len(r_leds) == r_led_count, "driver table length", f"{len(n_leds)} vs {len(r_leds)} vs count {r_led_count}", str(len(r_leds)))
    bad = [(i, a, b) for i, (a, b) in enumerate(zip(n_leds, r_leds)) if a != b]
    check(not bad, "driver table entries (driver, R, G, B register)", "; ".join(f"#{i} {a} vs {b}" for i, a, b in bad[:5]))
    per_driver = {0: sum(1 for e in r_leds if e[0] == 0), 1: sum(1 for e in r_leds if e[0] == 1)}
    check(per_driver == {0: d1, 1: d2}, "LEDs per driver match DRIVER_n_LED_TOTAL", f"{per_driver} vs {{0: {d1}, 1: {d2}}}", str(per_driver))
    regs = [(e[0], c) for e in r_leds for c in e[1:]]
    check(len(set(regs)) == len(regs), "driver/register pairs unique (no LED shares a channel)")
    check(all(e[0] == 1 for e in r_leds[64:]), "side LEDs 64-73 are on driver 2")
    for i, (d, rr, gg, bb) in enumerate(r_leds):
        nums = [int(re.search(r"CS(\d+)", x).group(1)) for x in (rr, gg, bb)]
        sws = [int(re.search(r"SW(\d+)", x).group(1)) for x in (rr, gg, bb)]
        if not (nums[0] == nums[1] == nums[2] and sws[1] == sws[0] + 1 and sws[2] == sws[0] + 2):
            report("WARN", f"LED {i} has an unusual SW/CS pattern", str((rr, gg, bb)))

    print("== config.h: pins, addresses, timing")
    pin_map = [
        ("DEV_MODE_PIN", "DEVICE_MODE_PIN"), ("SYS_MODE_PIN", "OS_MODE_PIN"), ("DC_BOOST_PIN", "DC_BOOST_PIN"),
        ("NRF_RESET_PIN", "NRF_RESET_PIN"), ("NRF_BOOT_PIN", "NRF_TEST_PIN"), ("NRF_WAKEUP_PIN", "NRF_WAKEUP_PIN"),
        ("RGB_DRIVER_SDB1", "RGB_DRIVER_SDB1"), ("RGB_DRIVER_SDB2", "RGB_DRIVER_SDB2"), ("SERIAL_DRIVER", "SERIAL_DRIVER"),
        ("SD1_TX_PIN", "UART_TX_PIN"), ("SD1_TX_PAL_MODE", "UART_TX_PAL_MODE"), ("SD1_RX_PIN", "UART_RX_PIN"), ("SD1_RX_PAL_MODE", "UART_RX_PAL_MODE"),
        ("DRIVER_ADDR_1", "IS31FL3733_I2C_ADDRESS_1"), ("DRIVER_ADDR_2", "IS31FL3733_I2C_ADDRESS_2"),
        ("I2C_DRIVER", "I2C_DRIVER"), ("I2C1_SCL_PIN", "I2C1_SCL_PIN"), ("I2C1_SDA_PIN", "I2C1_SDA_PIN"), ("I2C1_CLOCK_SPEED", "I2C1_CLOCK_SPEED"),
        ("I2C1_SCL_PAL_MODE", "I2C1_SCL_PAL_MODE"), ("I2C1_SDA_PAL_MODE", "I2C1_SDA_PAL_MODE"),
        ("I2C1_TIMINGR_PRESC", "I2C1_TIMINGR_PRESC"), ("I2C1_TIMINGR_SCLDEL", "I2C1_TIMINGR_SCLDEL"), ("I2C1_TIMINGR_SDADEL", "I2C1_TIMINGR_SDADEL"),
        ("I2C1_TIMINGR_SCLH", "I2C1_TIMINGR_SCLH"), ("I2C1_TIMINGR_SCLL", "I2C1_TIMINGR_SCLL"), ("I2C1_DUTY_CYCLE", "I2C1_DUTY_CYCLE"),
        ("DRIVER_COUNT", "DRIVER_COUNT"), ("DRIVER_1_LED_TOTAL", "DRIVER_1_LED_TOTAL"), ("DRIVER_2_LED_TOTAL", "DRIVER_2_LED_TOTAL"),
        ("RGB_MATRIX_DEFAULT_MODE", "RGB_MATRIX_DEFAULT_MODE"), ("TAP_CODE_DELAY", "TAP_CODE_DELAY"), ("DYNAMIC_KEYMAP_MACRO_DELAY", "DYNAMIC_KEYMAP_MACRO_DELAY"),
    ]
    bad = []
    for n_name, r_name in pin_map:
        nv, rv = n_cfg.get(n_name), r_cfg.get(r_name)
        if nv is None or rv is None or nv != rv:
            bad.append(f"{n_name}={nv} vs {r_name}={rv}")
    check(not bad, f"{len(pin_map)} config values equal stock", "; ".join(bad))
    pins_r = {k: v for k, v in r_cfg.items() if k.endswith("_PIN") or k.startswith("RGB_DRIVER_SDB") or k.startswith("IS31FL3733_I2C_ADDRESS")}
    pins_h = {k: h_cfg.get(k) for k in pins_r}
    check(pins_r == pins_h, "pins and I2C addresses equal Halo75 V2", "; ".join(f"{k}: {pins_r[k]} vs {pins_h[k]}" for k in pins_r if pins_r[k] != pins_h[k]))
    only_h = sorted(set(h_cfg) - set(r_cfg))
    only_r = sorted(set(r_cfg) - set(h_cfg))
    if only_h:
        report("WARN", "defines present in Halo75 V2 config but missing in port", ", ".join(f"{k} ({h_cfg[k]})" for k in only_h))
    if only_r:
        report("INFO", "defines only in port config", ", ".join(only_r))
    stock_only = sorted(k for k in n_cfg if k not in dict(pin_map) and k not in r_cfg)
    if stock_only:
        report("INFO", "stock defines with no port counterpart (handled by common layer or driver defaults)", ", ".join(f"{k}={n_cfg[k]}" for k in stock_only))

    print("== semantics: OS switch polarity, indicator positions, digit rows")
    n_ansi = strip_c_comments((N / "ansi.c").read_text())
    m = re.search(r"if\s*\(\s*dial_scan\s*&\s*0[xX]02\s*\)\s*\{(.*?)\}\s*else", n_ansi, re.S)
    stock_high_is_win = bool(m and "SYS_SW_WIN" in m.group(1))
    check(stock_high_is_win and r_cfg.get("NUPHY_OS_SWITCH_HIGH_IS_WIN") == "1", "NUPHY_OS_SWITCH_HIGH_IS_WIN matches stock (pin high -> Win)", f"stock high->win={stock_high_is_win}, port={r_cfg.get('NUPHY_OS_SWITCH_HIGH_IS_WIN')}")
    stock_win_layer = int(re.search(r"dial_scan\s*&\s*0[xX]02.*?default_layer_set\(1\s*<<\s*(\d)\)", n_ansi, re.S).group(1))
    common_kb = strip_c_comments((C / "core/keyboard.c").read_text())
    common_layers = sorted(set(int(x) for x in re.findall(r"default_layer_set\(1\s*<<\s*(\d)\)", common_kb)))
    report("INFO", "base layers", f"stock Mac=0 Win={stock_win_layer}; common layer uses {common_layers} (Mac=0, Win=2) -> keymap layers are permuted")

    n_layers = parse_keymap_layers(N / "keymaps/default/keymap.c")
    r_layers = parse_keymap_layers(R / "keymaps/default/keymap.c")
    win_pos = r_pos_of_matrix.get((int(r_cfg["WIN_LOCK_ROW"]), int(r_cfg["WIN_LOCK_COL"])))
    win_layer_kc = r_layers[2][1][win_pos] if win_pos is not None else None
    check(win_layer_kc == "KC_LGUI", "WIN_LOCK_ROW/COL points at the Win key on the Win layer", f"layer 2 keycode at {(r_cfg['WIN_LOCK_ROW'], r_cfg['WIN_LOCK_COL'])} is {win_layer_kc}", f"({r_cfg['WIN_LOCK_ROW']},{r_cfg['WIN_LOCK_COL']}) = {win_layer_kc}")
    tens_row = int(r_cfg.get("NUPHY_TENS_DIGIT_ROW", 0))
    tens_keys = [r_layers[0][1][r_pos_of_matrix[(tens_row, c)]] if (tens_row, c) in r_pos_of_matrix else None for c in range(10)]
    check(tens_keys == ["KC_TAB", "KC_Q", "KC_W", "KC_E", "KC_R", "KC_T", "KC_Y", "KC_U", "KC_I", "KC_O"], "tens digit row (NUPHY_TENS_DIGIT_ROW) covers cols 0-9 with Tab,Q..O", str(tens_keys))
    ones_keys = [r_layers[0][1][r_pos_of_matrix[(1, c)]] if (1, c) in r_pos_of_matrix else None for c in range(1, 11)]
    check(ones_keys == ["KC_1", "KC_2", "KC_3", "KC_4", "KC_5", "KC_6", "KC_7", "KC_8", "KC_9", "KC_0"], "ones digit row (row 1, cols 1-10) is 1..9,0", str(ones_keys))
    check(r_cfg.get("DEFAULT_DETECT_NUMLOCK") == "0", "num lock detection off (Air60 has no num lock key)", r_cfg.get("DEFAULT_DETECT_NUMLOCK"))
    caps_pos = r_pos_of_matrix.get((int(r_cfg.get("CAPS_LOCK_ROW", 3)), int(r_cfg.get("CAPS_LOCK_COL", 0))))
    check(caps_pos is not None and r_layers[0][1][caps_pos] == "KC_CAPS" and r_layers[2][1][caps_pos] == "KC_CAPS", "CAPS_LOCK_ROW/COL points at Caps Lock on both base layers", f"key there: {r_layers[0][1][caps_pos] if caps_pos is not None else None}")

    print("== keymap: stock 7 layers vs port (layer permutation + RGB_ -> RM_ rename)")
    layer_perm = {0: 0, 1: 1, 2: 4, 3: 2, 4: 3, 5: 5, 6: 6}
    kc_rename = {"RGB_SPD": "RM_SPDD", "RGB_SPI": "RM_SPDU", "RGB_VAI": "RM_VALU", "RGB_VAD": "RM_VALD", "RGB_MOD": "RM_NEXT", "RGB_HUI": "RM_HUEU"}

    def translate(kc):
        m = re.fullmatch(r"MO\((\d+)\)", kc)
        if m:
            return f"MO({layer_perm[int(m.group(1))]})"
        return kc_rename.get(kc, kc)

    # keys the port adds on purpose: (port layer, position in LAYOUT order) -> keycode
    port_additions = {
        (1, r_pos_of_matrix[(2, 0)]): "USJIS_TOG", (3, r_pos_of_matrix[(2, 0)]): "USJIS_TOG",  # Fn+Tab: US-JIS toggle
        (0, r_pos_of_matrix[(5, 2)]): "LGUI_T(KC_LNG2)", (0, r_pos_of_matrix[(5, 9)]): "RGUI_T(KC_LNG1)",  # IME mod-taps beside Space
        (2, r_pos_of_matrix[(5, 2)]): "LALT_T(KC_INT5)", (2, r_pos_of_matrix[(5, 9)]): "RALT_T(KC_INT4)",
        (0, r_pos_of_matrix[(3, 0)]): "KC_LCTL", (0, r_pos_of_matrix[(5, 0)]): "KC_CAPS",  # Caps Lock and Left Ctrl swapped
        (2, r_pos_of_matrix[(3, 0)]): "KC_LCTL", (2, r_pos_of_matrix[(5, 0)]): "KC_CAPS",
    }
    check(sorted(n_layers) == list(range(7)) and sorted(r_layers) == list(range(7)), "both keymaps define layers 0-6", f"{sorted(n_layers)} vs {sorted(r_layers)}")
    for n_layer, r_layer in layer_perm.items():
        n_macro, n_keys_l = n_layers[n_layer]
        r_macro, r_keys_l = r_layers[r_layer]
        expected = [port_additions.get((r_layer, i), translate(k)) for i, k in enumerate(n_keys_l)]
        diffs = [(i, e, r) for i, (e, r) in enumerate(zip(expected, r_keys_l)) if e != r]
        check(len(expected) == len(r_keys_l) and not diffs, f"stock layer {n_layer} == port layer {r_layer}", f"len {len(expected)} vs {len(r_keys_l)}; " + "; ".join(f"#{i} ({r_label_of_matrix.get(tuple(r_layout[i]['matrix']))}) expected {e} got {r}" for i, e, r in diffs[:6]))
    r_enum = parse_enum(C / "core/keys.h")
    n_enum = parse_enum(N / "ansi.h")
    used = set()
    for _, keys in r_layers.values():
        for k in keys:
            if re.fullmatch(r"[A-Z][A-Z0-9_]*", k) and not k.startswith(("KC_", "RM_")) and k != "_______":
                used.add(k)
    unknown = sorted(k for k in used if k not in r_enum)
    check(not unknown, "custom keycodes used by the port keymap exist in common keys.h", ", ".join(unknown), ", ".join(sorted(used)))
    dropped = sorted(set(n_enum) - set(r_enum))
    if dropped:
        report("INFO", "stock custom keycodes without a port counterpart", ", ".join(dropped))

    print("== VIA definition")
    n_via = json.loads((N / "keymaps/via/NuPhy Air60 V2 via3.json").read_text())
    r_via = json.loads((R / "keymaps/default/NuPhy Air60 V2 via3.json").read_text())
    for key in ("vendorId", "productId", "matrix"):
        check(n_via[key] == r_via[key], f"via.{key}", f"{n_via[key]} vs {r_via[key]}", str(r_via[key]))
    check(n_via["layouts"]["keymap"] == r_via["layouts"]["keymap"], "via layouts.keymap equal stock")
    via_matrices = [tuple(int(x) for x in cell.split(",")) for row in r_via["layouts"]["keymap"] for cell in row if isinstance(cell, str)]
    check(via_matrices == [tuple(e["matrix"]) for e in r_layout], "via keymap order == keyboard.json layout order")
    check(int(r_via["matrix"]["rows"]) == len(r_kb["matrix_pins"]["rows"]) and int(r_via["matrix"]["cols"]) == len(r_kb["matrix_pins"]["cols"]), "via matrix size == matrix_pins")
    via_names = [k["name"].replace("\n", " ") for k in r_via["customKeycodes"]]
    check(len(via_names) == len(r_enum), "via customKeycodes count == common enum count", f"{len(via_names)} vs {len(r_enum)}", str(len(r_enum)))
    via_to_enum = {
        "RF DFU": "RF_DFU", "Link USB": "LNK_USB", "Link RF": "LNK_RF", "Link BLE_1": "LNK_BLE1", "Link BLE_2": "LNK_BLE2", "Link BLE_3": "LNK_BLE3",
        "Mac Task": "MAC_TASK", "Mac Search": "MAC_SEARCH", "Mac Voice": "MAC_VOICE", "Mac Console": "MAC_CONSOLE", "Mac DND": "MAC_DND",
        "Win lock": "WIN_LOCK", "Dev Reset": "DEV_RESET", "Sleep Mode": "SLEEP_MODE", "Bat Show": "BAT_SHOW", "RGB Test": "RGB_TEST", "Tilde": "SHIFT_GRV",
        "Side Light+": "SIDE_VAI", "Side Light-": "SIDE_VAD", "Side Mode": "SIDE_MOD", "Side Color": "SIDE_HUI", "Side Fast": "SIDE_SPI", "Side Slow": "SIDE_SPD",
        "Ambient Light+": "AMBIENT_VAI", "Ambient Light-": "AMBIENT_VAD", "Ambient Mode": "AMBIENT_MOD", "Ambient Color": "AMBIENT_HUI", "Ambient Fast": "AMBIENT_SPI", "Ambient Slow": "AMBIENT_SPD",
        "TOG USB Sleep": "TOG_USB_SLP", "TOG CAPS INDICATOR": "TOG_CAPS_IND",
        "DEBOUNCE PRESS INCREASE": "DEBOUNCE_PRESS_INC", "DEBOUNCE PRESS DECREASE": "DEBOUNCE_PRESS_DEC", "DEBOUNCE PRESS SHOW": "DEBOUNCE_PRESS_SHOW",
        "SLP TIME INCREASE": "SLEEP_TIMEOUT_INC", "SLP TIME DECREASE": "SLEEP_TIMEOUT_DEC", "SLP TIME SHOW VAL": "SLEEP_TIMEOUT_SHOW", "Mac Globe": "MAC_GLOBE",
        "DEBOUNCE RELEASE INCREASE": "DEBOUNCE_RELEASE_INC", "DEBOUNCE RELEASE DECREASE": "DEBOUNCE_RELEASE_DEC", "DEBOUNCE RELEASE SHOW": "DEBOUNCE_RELEASE_SHOW",
        "TOG DEEP SLEEP": "TOG_DEEP_SLEEP", "TOG BAT IND NUMERIC": "TOG_BAT_IND_NUM", "PRNT CFW VERSION": "FW_VERSION", "TOG POWERON ANIM": "TOG_POWER_ON_ANIMATION",
        "US-JIS Toggle": "USJIS_TOG", "US-JIS On": "USJIS_ON", "US-JIS Off": "USJIS_OFF",
    }
    mapped = [via_to_enum.get(n, f"?{n}") for n in via_names]
    diffs = [(i, a, b) for i, (a, b) in enumerate(zip(mapped, r_enum)) if a != b]
    check(not diffs and len(mapped) == len(r_enum), "via customKeycodes order == enum order (QK_KB_n mapping)", "; ".join(f"#{i} via {a} vs enum {b}" for i, a, b in diffs[:5]))
    ambient = [n for n in via_names if n.startswith("Ambient")]
    if ambient and r_cfg.get("NUPHY_AMBIENT_LIGHTING_ENABLED") == "0":
        report("INFO", "via exposes Ambient keycodes although NUPHY_AMBIENT_LIGHTING_ENABLED=0", "they are no-ops on the Air60 V2 (keeps QK_KB_n numbering shared with other boards)")

    print("== side lights (two 5-LED strips)")
    n_pairs = parse_side_pairs(N / "side.c", "side_led_index_tab")
    r_pairs = parse_side_pairs(R / "side.c", "side_row_index_tab")
    check(n_pairs == r_pairs, "row -> (left, right) LED pairs equal stock", f"{n_pairs} vs {r_pairs}", str(r_pairs))
    r_ind = parse_side_list(R / "side.c", "indicator_led_index_tab")
    r_bat = parse_side_list(R / "side.c", "battery_led_index_tab")
    n_side_c = strip_c_comments((N / "side.c").read_text())
    stock_left = [64 + i for i in range(5)]
    stock_right = [69 + i for i in range(5)]
    check(set(r_ind) == set(stock_left), "indicator strip == stock left strip (64-68)", str(r_ind))
    check(r_bat == [73, 72, 71, 70, 69] and "SIDE_INDEX + 9 - i" in n_side_c, "battery bar fills 73 -> 69, as stock bat_percent_led (SIDE_INDEX + 9 - i)", str(r_bat))
    stock_usage = {}
    for fn, body in re.findall(r"\n(?:static )?void (\w+)\([^)]*\)\s*\{(.*?)\n\}", n_side_c, re.S):
        strips = set()
        if "set_left_rgb(" in body:
            strips.add("left")
        if "set_right_rgb(" in body or "SIDE_INDEX + 9 - i" in body:
            strips.add("right")
        if strips and fn not in ("set_left_rgb", "set_right_rgb"):
            stock_usage[fn] = "+".join(sorted(strips))
    report("INFO", "stock strip usage per indicator", ", ".join(f"{k}={v}" for k, v in stock_usage.items()))
    right_non_bat = [k for k, v in stock_usage.items() if "right" in v and "bat" not in k]
    port_side_c = strip_c_comments((R / "side.c").read_text())
    m = re.search(r"void set_notice_on_side\([^)]*\)\s*\{(.*?)\}", port_side_c, re.S)
    notice_on_right = bool(m and "battery" in m.group(1))
    check(notice_on_right, "OS-switch and sleep-toggle notices go to the right strip like stock", f"stock right-strip users: {right_non_bat}; port set_notice_on_side found={bool(m)}", f"stock right-strip users: {right_non_bat}")

    print("== custom RGB effects (rgb_matrix_user.inc) index sanity")
    inc = strip_c_comments((R / "keymaps/default/rgb_matrix_user.inc").read_text())
    label_of_index = {i: r_label_of_matrix.get(tuple(e["matrix"])) for i, e in enumerate(r_rgb)}
    expect = {0: "Esc", 16: "W", 29: "A", 30: "S", 31: "D", 53: "↑", 61: "←", 62: "↓", 63: "→", 32: "F", 35: "J"}
    used_idx = sorted(set(int(x) for x in re.findall(r"rgb_matrix_set_color\((\d+),", inc)))
    bad = [(i, label_of_index.get(i), expect.get(i)) for i in used_idx if expect.get(i) != label_of_index.get(i)]
    check(not bad, "hard-coded LED indexes in custom effects point at the intended keys", "; ".join(f"#{i} is {l!r}, expected {e!r}" for i, l, e in bad), ", ".join(f"{i}={label_of_index[i]}" for i in used_idx))

    print("== files reused from Halo75 V2")
    for f in ("matrix.c", "mcu_pwr.h", "halconf.h", "mcuconf.h"):
        check(filecmp.cmp(H / f, R / f, shallow=False), f"{f} identical to Halo75 V2")
    r_rules = (R / "rules.mk").read_text()
    check("CUSTOM_MATRIX = lite" in r_rules and "matrix.c" in r_rules and "is31fl3733.c" in r_rules, "rules.mk wires matrix.c and the IS31FL3733 driver")

    print()
    print("summary: " + ", ".join(f"{k}={v}" for k, v in results.items()))
    return 1 if results["FAIL"] else 0


if __name__ == "__main__":
    sys.exit(main())
