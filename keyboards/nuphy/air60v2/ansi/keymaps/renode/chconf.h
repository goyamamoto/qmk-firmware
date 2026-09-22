// Renode emulation only: switch contexts through PendSV instead of NMI.
// Renode's Cortex-M0 core does not deliver the NMI that ChibiOS's ARMv6-M
// port pends from __port_exit_from_isr while PRIMASK is set.
#pragma once
#define CORTEX_ALTERNATE_SWITCH TRUE
#include_next <chconf.h>
