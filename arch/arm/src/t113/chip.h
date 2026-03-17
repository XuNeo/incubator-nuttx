/****************************************************************************
 * arch/arm/src/t113/chip.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#ifndef __ARCH_ARM_SRC_T113_CHIP_H
#define __ARCH_ARM_SRC_T113_CHIP_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#ifndef __ASSEMBLY__
#  include <nuttx/arch.h>
#endif

#include "hardware/t113_memorymap.h"

/* IRQ numbers (from r528_irq.h — T113/R528 same silicon) */

#define T113_IRQ_TWI0     41
#define T113_IRQ_TWI1     42
#define T113_IRQ_TWI2     43
#define T113_IRQ_TWI3     44
#define T113_IRQ_SPI0     47
#define T113_IRQ_SPI1     48
#define T113_IRQ_UART0    34
#define T113_IRQ_UART1    35
#define T113_IRQ_UART2    36
#define T113_IRQ_UART3    37
#define T113_IRQ_DMA0     82  /* channels 0-7 */
#define T113_IRQ_DMA1     83  /* channels 8-15 */

/* CONFIG_GICD_BASE and CONFIG_GICC_BASE expected by armv7-a/arm_gicv2.c */

#define CONFIG_GICD_BASE  T113_GIC_DIST_PADDR
#define CONFIG_GICC_BASE  T113_GIC_CPU_PADDR

#endif /* __ARCH_ARM_SRC_T113_CHIP_H */
