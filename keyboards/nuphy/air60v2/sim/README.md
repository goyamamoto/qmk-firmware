# Air60 V2 tests and emulator

Three checks, from cheapest to most complete. Run all of them after touching
the port; each takes well under a minute except the first Renode build.

| Check | Command | Catches |
|---|---|---|
| Static comparison | `sim/compare_stock.py --stock <nuphy-src tree>` | copied-wrong hardware facts |
| Host unit tests | `make -C sim/hosttest` | logic bugs in the side lights and config code |
| Renode emulator | `sim/renode/run.sh keytest.resc` | boot, I2C init, nRF protocol, key path |

Not covered: USB enumeration, how the LEDs actually look, real radio traffic,
I2C electrical behaviour. Those need the keyboard.

## 1. Static comparison against the stock firmware

    keyboards/nuphy/air60v2/sim/compare_stock.py --stock ../refs/nuphy/qmk_firmware

Compares `keyboards/nuphy/air60v2/ansi` with the NuPhy stock tree
(https://github.com/nuphy-src/qmk_firmware, branch `nuphy-keyboards`, cloned
outside this repository) and with the Halo75 V2 here: pins, I2C addresses,
IS31FL3733 register table, LED coordinates, layout, keymap (with the layer
permutation and `RGB_*` to `RM_*` renames), VIA definition, side-light
tables, hard-coded LED indexes in the custom effects. Exit 1 on any FAIL;
WARN and INFO lines are intentional differences that only need a look.

## 2. Host unit tests

    make -C keyboards/nuphy/air60v2/sim/hosttest

Compiles the real `ansi/side.c` and `common/config/config.c` (plus QMK's
`quantum/color.c`) for the host against stub QMK headers in `hosttest/stubs`,
then drives them with a fake clock and a recording `rgb_matrix_set_color`.
Covers the two-digit value display, config defaults and EEPROM init, strip
addressing, key controls, the four side effects, the power-on animation, the
battery bar (thresholds, brightness scaling, timeout, low-battery blink,
charging breathe, full charge, RF link gating), the housekeeping hook, the
Fn status lights and device reset. A second binary,
`test_usjis`, drives `common/core/usjis.c` through a model of QMK's report
handling: the twenty substitutions with either Shift, identity when the mode
is off or the OS switch is on Mac, and the press/release and mode-change
sequences from the zmk-kb1-usjis specification (S01-S06, S11-S14). For
S02, S03, S05 and S11-S14 it checks every report in order, because the host
picks a character from the report in which its key goes down.

## 3. Renode emulator

    qmk compile -kb nuphy/air60v2/ansi -km renode
    keyboards/nuphy/air60v2/sim/renode/run.sh keytest.resc

Boots the firmware on an emulated STM32F072 with a fake nRF module on
USART1, two dummy IS31FL3733 drivers on I2C1 and a key matrix model, then
presses A, Z, Esc and Fn+1, turns the US-JIS mode on with Fn+Tab and presses
Shift+2, = and Shift+-, taps and holds the IME keys beside Space, presses
the swapped Caps Lock and Left Ctrl keys, then turns the mode off and presses
Shift+2 again. Expected reports (mods, key): 00 04, 00 1D, 00 29, 00 3A, then
00 2F, 02 2D, 02 87, then 00 8B, 04 04, 01 00, 00 39, 00 8A, then 02 1F.
About 20 s.

Needs Renode 1.17 or newer (https://github.com/renode/renode/releases);
set `RENODE=/path/to/renode` if it is not on PATH.

### Firmware variant

`keymaps/renode` is the stock keymap plus two ChibiOS settings that only
matter under Renode. Never flash it.

* `CORTEX_ALTERNATE_SWITCH TRUE`: context switches through PendSV. Renode's
  Cortex-M0 core never runs the NMI that the ARMv6-M port pends from
  `__port_exit_from_isr` while PRIMASK is set, so the stock build hangs at
  the first ISR exit.
* `STM32_I2C_USE_DMA FALSE`: Renode's STM32F7_I2C model has no TX DMA.

### Platform (`renode/air60v2.repl`)

Renode's `stm32f072.repl` with these changes:

* `rcc.py`: RCC model whose ready bits follow the enable bits (the built-in
  model lacks LSI/HSI48 ready flags, so ChibiOS's clock init spins).
* `regfile.py` on FLASH, PWR, SYSCFG, CRS, USB, DBGMCU: write-then-read-back
  registers so init sequences terminate. USB never enumerates.
* DMA with 7 channels (the base file has 5 and crashes on channel 6).
* I2C1 event/error interrupts wired to NVIC 23 (unconnected in the base).
* SysTick and all timers at 48 MHz (the base file uses 72 and 10 MHz; ChibiOS
  keeps its system time on TIM2, so with 10 MHz one firmware millisecond took
  4.8 virtual ms and every held key looked like a tap).
* `run.sh` generates `air60v2.gen.repl` with absolute paths (Renode resolves
  Python peripheral paths relative to its installation) and `ff.bin`, loaded
  before the ELF so unprogrammed flash reads 0xFF like erased flash; the
  emulated EEPROM needs that.

`gpio_pullups.resc` drives the 17 column inputs, the OS switch (C1, Win) and
NRF_TEST high and the device-mode switch (C0) low = wireless. It must run
after the emulation has started, because peripheral state is reset on start.

### Python models (run inside the Renode monitor)

* `nrf_stub.py`: answers the keyboard's UART frames. ACK `5A cmd A0` for
  commands, the 32-byte config table for READ_DATA, and link 2.4G /
  connected / 85 % battery for RF_STS_SYSC. Every frame is recorded in
  `nrf["frames"]`; `nrf_report.py` prints a summary.
* `keypress.py`: watchpoints on the row-port BSRR writes pull a column low
  while its row is selected. `air60.press(row, col)`, `air60.release(row,
  col)`, `air60.release_all()` from `python "..."` in a script;
  `keyreport.py` lists the HID reports the fake nRF received.

### Speed

The main loop polls GPIO constantly, so at Renode's default 100 MIPS one
virtual second costs one to two minutes of wall time. The scripts set
`sysbus.cpu PerformanceInMips 10`, which makes a 7-second run finish in
about 10 s; firmware timing is driven by SysTick, so behaviour is the same.
