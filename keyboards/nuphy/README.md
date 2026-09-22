# Nuphy Keyboards

This directory contains the NuPhy keyboards maintained in this tree, along with shared documentation and the common implementation used across the newer refactor path.

## Keyboards

- [Air60 V2 ANSI](./air60v2/ansi/readme.md) ([日本語](./air60v2/ansi/readme.ja.md)): adds the US-JIS mode, IME keys beside Space, the Caps Lock / Left Ctrl swap for typing a US keyboard on a Japanese-layout host, and status lights while Fn is held
- [Air75 V2 ANSI](./air75v2/ansi/readme.md)
- [Gem80](./gem80/readme.md)
- [Halo75 V2 ANSI](./halo75v2/ansi/readme.md)
- [Halo96 V2 ANSI](./halo96v2/ansi/readme.md)

`Halo96v2` is introduced as a supported keyboard starting with this release line.

## Shared Docs

- [Nuphy keyboard instructions](./instructions.md)
- [Shared common implementation notes](./common/README.md)

## Features added to the common layer with the Air60 V2

Available to every board on the common layer; see [common/README.md](./common/README.md#optional-board-hooks).

- US-JIS substitution (`common/core/usjis.c`): keycodes `USJIS_TOG`, `USJIS_ON`, `USJIS_OFF` and the VIA value `id_usjis_toggle`. Only the Air60 V2 keymap places it (Fn+Tab).
- Wireless NKRO fix: key codes above 0x77 (for example the JIS keys International1 and International3) are now sent over 2.4 GHz and Bluetooth.
- `set_notice_on_side()`: boards with two side strips can put the OS-switch, sleep and US-JIS notices on a strip of their choice.
- `CAPS_LOCK_ROW` / `CAPS_LOCK_COL` for the under-key Caps Lock indicator, `NUPHY_TENS_DIGIT_ROW` for boards without an F-row.

## Notes

- VIA JSON files for the supported boards live under each keyboard's `keymaps/default/` directory.
- The shared implementation used by the refactored boards lives in `keyboards/nuphy/common`.
- Board-specific readmes still contain per-keyboard flashing/build details.
