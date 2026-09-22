# NuPhy Air60 V2 (ANSI)

* Keyboard Maintainer: [goyamamoto](https://github.com/goyamamoto)
* Hardware Supported: NuPhy Air60 V2 (ANSI), USB `19F5:3255`
* Hardware Availability: [NuPhy](https://nuphy.com/)

Japanese: [readme.ja.md](./readme.ja.md)

## Why this firmware

The Air60 V2 ships with a QMK build from NuPhy's own fork
([nuphy-src/qmk_firmware](https://github.com/nuphy-src/qmk_firmware)),
which stopped at a 2024 QMK and has not moved since. The
[ryodeushii](https://github.com/ryodeushii/qmk-firmware) tree keeps NuPhy's
wireless boards on a current QMK through a shared `keyboards/nuphy/common`
layer, but it did not include the Air60 V2. This port adds the Air60 V2 to
that tree, so it gets the same maintained base as the Air75 V2, Halo75 V2,
Halo96 V2 and Gem80.

The second motivation is typing on a US keyboard against hosts that are set
to the Japanese keyboard layout. On such a host the symbols do not match
the keycaps. The features below solve that on the keyboard, following the
[zmk-kb1-usjis](https://github.com/goyamamoto/zmk-kb1-usjis) firmware for the
Keychron B1 Pro, so both keyboards behave the same way.

## What is different from the stock firmware

| Problem | What this firmware does |
|---|---|
| Stock QMK is a 2024 snapshot with no upstream path. | Built on the current QMK of the ryodeushii tree. Wireless, sleep, debounce, VIA menus and the RGB code come from the shared common layer used by the other NuPhy boards. The Air60 V2 keeps only its own matrix, LED table and side lights. |
| A JIS-layout host reads `` ` ~ @ ^ & * ( ) _ = + [ { ] } \ \| : ' " `` differently from the US keycaps. | **US-JIS mode** (Fn+Tab): those twenty chords are replaced by the JIS chord for the printed character. Off by default, kept across power cycles, active only with the OS switch on Win. |
| Switching the IME needs a dedicated key on a JIS keyboard. | **IME keys beside Space**: tap the left one for IME off, the right one for IME on (Mac: Eisu / Kana, Win: Muhenkan / Henkan). Held, they stay Cmd (Mac) or Alt (Win). |
| Ctrl is far away on a 60 % board. | **Caps Lock and Left Ctrl are swapped** on both base layers. |
| The JIS keys `\` `_` `\|` never arrived over 2.4 GHz or Bluetooth in Win mode. | Fixed in the common layer: the wireless NKRO sender only carried key codes up to 0x77. |
| The OS-switch and sleep-toggle blink appeared on the left strip; the stock firmware uses the right one. | Blinks on the right strip like stock. |
| A stuck LED driver could stall the main loop for seconds. | The I2C timeout is 5 ms (stock: 1 ms, common-layer default: 100 ms). |
| VIA rejected the stock definition on current VIA. | The VIA definition uses `qmk_rgb_matrix_keycodes` and lists every custom keycode. |
| No way to see the link or the US-JIS mode at a glance. | **Status lights while Fn is held**: the key of the current link (Q/W/E, R, Y) and Tab for US-JIS. Dark whenever RGB Matrix is off. |
| The common layer's charging animation only blinked the top segment at a high charge, and side indicators could flicker against the effect. | While charging the level bar breathes in its own colour; full is a steady green bar. The strips are drawn inside the RGB frame, under the indicators, so nothing flickers. |

Everything else (side light effects, battery bar, sleep, pairing, VIA
lighting menus) works as on the other boards of the common layer.

## Keys

| Key | Action |
|---|---|
| Fn+Tab | US-JIS mode on / off. The right strip blinks green (on) or red (off). Also `USJIS_ON` / `USJIS_OFF` keycodes and a VIA toggle (Custom Configs, Layout). |
| Key left of Space | Hold: Cmd (Mac) / Alt (Win). Tap: Eisu (Mac) / Muhenkan (Win) = IME off. |
| Key right of Space | Hold: Cmd (Mac) / Alt (Win). Tap: Kana (Mac) / Henkan (Win) = IME on. |
| Key printed Caps Lock | Left Ctrl |
| Key printed Ctrl | Caps Lock |
| Fn+Q / W / E, Fn+R | Bluetooth 1-3, 2.4 GHz |
| Fn+M plus arrows / ; | Side light mode, brightness, colour |
| Fn held | Status lights: the key of the current link lights up (Q/W/E blue for BLE 1-3, R green for 2.4 GHz, Y white for USB; blinking until a wireless link is connected) and Tab shows US-JIS (green on, red off). Off whenever RGB Matrix is off. |
| Esc held while plugging in | Bootloader (DFU) |

The OS switch selects the base layer: Mac (layers 0, 1, 4) or Win (layers
2, 3, 5). Layer 6 is the side light layer (Fn+M).

### US-JIS mode details

The substitution is decided when a key goes down, from the key and the Shift
state at that moment, and undone when the key goes up. While one of the
substituted keys is held, Shift in the report follows the most recently
pressed key, so `@` followed by `A` gives `@A`. Ctrl, Alt and GUI pass
through. A mode change while a key is held waits until all keys are
released. The twenty substitutions and the rules are in
`keyboards/nuphy/common/core/usjis.c`.

Verified on a Windows host set to the Japanese layout, wired and wireless.
Pressing another key while a substituted key is held (`=` then `A` gives
`=a`, `@` then `A` gives `@A`) is checked on hardware and by the host tests,
which compare every report of those sequences.
On macOS keep the OS switch on Mac: macOS types a US keyboard as printed
anyway, and it discards the JIS key codes (0x87, 0x89) the mode would send
for `\`, `_` and `|`.

### IME keys on Windows

Set Microsoft IME to use Muhenkan for "IME off" and Henkan for "IME on"
(Settings, Time & Language, Language, Japanese, Microsoft IME, Key and touch
customization). macOS needs no setup. A key pressed while one of the IME
keys is held takes the modifier at once (`HOLD_ON_OTHER_KEY_PRESS`).

## Building and flashing

    qmk compile -kb nuphy/air60v2/ansi -km via

Enter the bootloader by holding Esc while plugging the keyboard in with the
mode switch on wired, then:

    dfu-util -a 0 -s 0x08000000:leave -D nuphy_air60v2_ansi_via.bin

QMK Toolbox works too; see [instructions.md](../../instructions.md). The
nRF module firmware is not touched, and Esc while plugging in always brings
the bootloader back, so a bad flash can be repeated.

After flashing a build whose default keymap changed, VIA's stored keymap
still applies. Use "Reset keymap" in VIA or edit the keys. VIA needs the
definition `keymaps/default/NuPhy Air60 V2 via3.json` loaded once through
the Design tab.

## Hardware

* STM32F072, 6x17 matrix (COL2ROW), same pins as the Halo75 V2
* Two IS31FL3733 LED drivers on I2C1: 64 key LEDs and 10 side LEDs (index 64-73, two strips of five)
* nRF module on USART1 for 2.4 GHz and Bluetooth

## Side lights

Effects run on both strips. The left strip shows caps lock and the link
state. The right strip shows the battery level and the OS-switch, sleep and
US-JIS notices. While charging the level bar breathes in its own colour, so
the level stays readable; when the charger reports full it shows a steady
full green bar.

The Air60 V2 has no F-row, so two-digit values (debounce, sleep timeout,
numeric battery) show the tens digit on the Q row (Tab = 0, Q = 1 ...) and
the ones digit on the number row.

## Tests and emulator

`keyboards/nuphy/air60v2/sim/` holds a static comparison against the stock
firmware, unit tests for the side lights, the config code and the US-JIS
substitution, and a Renode emulator that boots the firmware on a virtual
STM32F072 and types keys through a fake nRF module. See
[sim/README.md](../sim/README.md).

## Shared code

This keyboard uses `keyboards/nuphy/common`. Board-local files hold the
matrix scan, the LED table, the side lights and the keyboard metadata. The
common layer gained a few things for this board that other boards can use:
the US-JIS substitution and its keycodes, `set_notice_on_side()` for boards
with two strips, `CAPS_LOCK_ROW`/`CAPS_LOCK_COL` and `NUPHY_TENS_DIGIT_ROW`.
