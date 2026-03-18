/****************************************************************************
 * arch/arm/include/t113/irq.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * IRQ numbers from vendor/allwinnertech/chips/r528/include/r528_irq.h
 * T113-S3 and R528 are the same silicon.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/* GIC interrupt count */

#define NR_IRQS   160

/* UART interrupt numbers */

#define T113_IRQ_UART0  34
#define T113_IRQ_UART1  35
#define T113_IRQ_UART2  36
#define T113_IRQ_UART3  37

/* USB interrupt numbers */

#define T113_IRQ_USB0   61

/* DMA interrupt numbers */

#define T113_IRQ_DMA0   82
#define T113_IRQ_DMA1   83

#endif /* __ARCH_ARM_INCLUDE_T113_IRQ_H */
