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

/****************************************************************************
 * Assembly Macros
 ****************************************************************************/

#ifdef __ASSEMBLY__

#if defined(CONFIG_SMP) && CONFIG_ARCH_INTERRUPTSTACK > 7
  .macro  setirqstack, tmp1, tmp2
  mrc     p15, 0, \tmp1, c0, c0, 5
  and     \tmp1, \tmp1, #3
  ldr     \tmp2, =g_irqstack_top
  lsls    \tmp1, \tmp1, #2
  add     \tmp2, \tmp2, \tmp1
  ldr     sp, [\tmp2, #0]
  .endm

  .macro  setfiqstack, tmp1, tmp2
  mrc     p15, 0, \tmp1, c0, c0, 5
  and     \tmp1, \tmp1, #3
  ldr     \tmp2, =g_fiqstack_top
  lsls    \tmp1, \tmp1, #2
  add     \tmp2, \tmp2, \tmp1
  ldr     sp, [\tmp2, #0]
  .endm
#endif

#endif /* __ASSEMBLY__ */

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* IRQ numbers */

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
