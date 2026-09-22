// Renode emulation only: drive I2C1 by interrupts instead of DMA, because
// Renode's STM32F7_I2C model does not implement the TX DMA request.
#pragma once
#include_next <mcuconf.h>
#undef STM32_I2C_USE_DMA
#define STM32_I2C_USE_DMA FALSE
