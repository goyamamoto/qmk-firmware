# QMK for NuPhy keyboards: Air60 V2 port

This fork of [ryodeushii/qmk-firmware](https://github.com/ryodeushii/qmk-firmware)
adds the **NuPhy Air60 V2** to the shared NuPhy common layer (Air75 V2,
Halo75 V2, Halo96 V2, Gem80), so the Air60 V2 runs on a current QMK instead
of NuPhy's 2024 snapshot. On top of the port, the Air60 V2 keymap gains
features for typing a US keyboard on a host set to the Japanese layout:

- **US-JIS mode** (Fn+Tab): the twenty symbol chords that a JIS-layout host
  reads differently from the US keycaps are typed as printed.
- **IME keys beside Space**: tap for IME off / on (Eisu / Kana on Mac,
  Muhenkan / Henkan on Windows), hold for Cmd / Alt as before.
- **Caps Lock and Left Ctrl swapped** on both base layers.
- **Status lights while Fn is held**: the current link on Q/W/E/R/Y and the US-JIS mode on Tab, never while RGB Matrix is off.

It also fixes the wireless NKRO sender of the common layer, which never sent
key codes above 0x77 (the JIS backslash and yen keys), and updates the VIA
definition for current VIA.

| | |
|---|---|
| Board readme | [keyboards/nuphy/air60v2/ansi/readme.md](keyboards/nuphy/air60v2/ansi/readme.md) ([日本語](keyboards/nuphy/air60v2/ansi/readme.ja.md)) |
| Build | `qmk compile -kb nuphy/air60v2/ansi -km via` |
| Flash | hold Esc while plugging in, then `dfu-util -a 0 -s 0x08000000:leave -D nuphy_air60v2_ansi_via.bin` |
| Tests and emulator | [keyboards/nuphy/air60v2/sim/README.md](keyboards/nuphy/air60v2/sim/README.md): stock comparison, unit tests, Renode emulator |
| All NuPhy boards in this tree | [keyboards/nuphy/README.md](keyboards/nuphy/README.md) |

The rest of this file is the upstream QMK readme.

---

# Quantum Mechanical Keyboard Firmware

[![Current Version](https://img.shields.io/github/tag/qmk/qmk_firmware.svg)](https://github.com/qmk/qmk_firmware/tags)
[![Discord](https://img.shields.io/discord/440868230475677696.svg)](https://discord.gg/qmk)
[![Docs Status](https://img.shields.io/badge/docs-ready-orange.svg)](https://docs.qmk.fm)
[![GitHub contributors](https://img.shields.io/github/contributors/qmk/qmk_firmware.svg)](https://github.com/qmk/qmk_firmware/pulse/monthly)
[![GitHub forks](https://img.shields.io/github/forks/qmk/qmk_firmware.svg?style=social&label=Fork)](https://github.com/qmk/qmk_firmware/)

This is a keyboard firmware based on the [tmk\_keyboard firmware](https://github.com/tmk/tmk_keyboard) with some useful features for Atmel AVR and ARM controllers, and more specifically, the [OLKB product line](https://olkb.com), the [ErgoDox EZ](https://ergodox-ez.com) keyboard, and the Clueboard product line.

## Documentation

* [See the official documentation on docs.qmk.fm](https://docs.qmk.fm)

The docs are powered by [VitePress](https://vitepress.dev/). They are also viewable offline; see [Previewing the Documentation](https://docs.qmk.fm/#/contributing?id=previewing-the-documentation) for more details.

You can request changes by making a fork and opening a [pull request](https://github.com/qmk/qmk_firmware/pulls).

## Supported Keyboards

* [Planck](/keyboards/planck/)
* [Preonic](/keyboards/preonic/)
* [ErgoDox EZ](/keyboards/ergodox_ez/)
* [Clueboard](/keyboards/clueboard/)
* [Cluepad](/keyboards/clueboard/17/)
* [Atreus](/keyboards/atreus/)

The project also includes community support for [lots of other keyboards](/keyboards/).

## Maintainers

QMK is developed and maintained by Jack Humbert of OLKB with contributions from the community, and of course, [Hasu](https://github.com/tmk). The OLKB product firmwares are maintained by [Jack Humbert](https://github.com/jackhumbert), the Ergodox EZ by [ZSA Technology Labs](https://github.com/zsa), the Clueboard by [Zach White](https://github.com/skullydazed), and the Atreus by [Phil Hagelberg](https://github.com/technomancy).

## Official Website

[qmk.fm](https://qmk.fm) is the official website of QMK, where you can find links to this page, the documentation, and the keyboards supported by QMK.
