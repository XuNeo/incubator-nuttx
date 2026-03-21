/****************************************************************************
 * arch/arm/src/t113/hardware/t113_uart.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Register definitions from vendor/allwinnertech/chips/r528/hardware/r528_uart.h
 * T113-S3 and R528 are the same silicon.
 *
 ****************************************************************************/

#ifndef __ARCH_ARM_SRC_T113_HARDWARE_T113_UART_H
#define __ARCH_ARM_SRC_T113_HARDWARE_T113_UART_H

#include <nuttx/config.h>
#include "hardware/t113_memorymap.h"

/* Register offsets *********************************************************/

#define T113_UART_RBR_OFFSET    0x0000  /* Receive Buffer (DLAB=0) */
#define T113_UART_THR_OFFSET    0x0000  /* Transmit Holding (DLAB=0) */
#define T113_UART_DLL_OFFSET    0x0000  /* Divisor Latch Low (DLAB=1) */
#define T113_UART_DLH_OFFSET    0x0004  /* Divisor Latch High (DLAB=1) */
#define T113_UART_IER_OFFSET    0x0004  /* Interrupt Enable (DLAB=0) */
#define T113_UART_IIR_OFFSET    0x0008  /* Interrupt Identity */
#define T113_UART_FCR_OFFSET    0x0008  /* FIFO Control */
#define T113_UART_LCR_OFFSET    0x000c  /* Line Control */
#define T113_UART_MCR_OFFSET    0x0010  /* Modem Control */
#define T113_UART_LSR_OFFSET    0x0014  /* Line Status */
#define T113_UART_MSR_OFFSET    0x0018  /* Modem Status */
#define T113_UART_SCH_OFFSET    0x001c  /* Scratch */
#define T113_UART_USR_OFFSET    0x007c  /* UART Status */
#define T113_UART_TFL_OFFSET    0x0080  /* TX FIFO Level */
#define T113_UART_RFL_OFFSET    0x0084  /* RX FIFO Level */
#define T113_UART_HALT_OFFSET   0x00a4  /* Halt TX */

/* Register virtual addresses ***********************************************/

#define T113_UART_RBR(n)        (T113_UART_VADDR(n) + T113_UART_RBR_OFFSET)
#define T113_UART_THR(n)        (T113_UART_VADDR(n) + T113_UART_THR_OFFSET)
#define T113_UART_DLL(n)        (T113_UART_VADDR(n) + T113_UART_DLL_OFFSET)
#define T113_UART_DLH(n)        (T113_UART_VADDR(n) + T113_UART_DLH_OFFSET)
#define T113_UART_IER(n)        (T113_UART_VADDR(n) + T113_UART_IER_OFFSET)
#define T113_UART_IIR(n)        (T113_UART_VADDR(n) + T113_UART_IIR_OFFSET)
#define T113_UART_FCR(n)        (T113_UART_VADDR(n) + T113_UART_FCR_OFFSET)
#define T113_UART_LCR(n)        (T113_UART_VADDR(n) + T113_UART_LCR_OFFSET)
#define T113_UART_MCR(n)        (T113_UART_VADDR(n) + T113_UART_MCR_OFFSET)
#define T113_UART_LSR(n)        (T113_UART_VADDR(n) + T113_UART_LSR_OFFSET)
#define T113_UART_MSR(n)        (T113_UART_VADDR(n) + T113_UART_MSR_OFFSET)
#define T113_UART_SCH(n)        (T113_UART_VADDR(n) + T113_UART_SCH_OFFSET)
#define T113_UART_USR(n)        (T113_UART_VADDR(n) + T113_UART_USR_OFFSET)
#define T113_UART_TFL(n)        (T113_UART_VADDR(n) + T113_UART_TFL_OFFSET)
#define T113_UART_RFL(n)        (T113_UART_VADDR(n) + T113_UART_RFL_OFFSET)
#define T113_UART_HALT(n)       (T113_UART_VADDR(n) + T113_UART_HALT_OFFSET)

/* Bit definitions (from r528_uart.h — identical hardware) ******************/

/* IER */
#define UART_IER_ERBFI          (1 << 0)
#define UART_IER_ETBEI          (1 << 1)
#define UART_IER_ELSI           (1 << 2)
#define UART_IER_EDSSI          (1 << 3)
#define UART_IER_PTIME          (1 << 7)
#define UART_IER_ALLIE          0x0000008f

/* IIR */
#define UART_IIR_IID_SHIFT      0
#define UART_IIR_IID_MASK       (15 << UART_IIR_IID_SHIFT)
#  define UART_IIR_IID_MODEM      (0  << UART_IIR_IID_SHIFT)
#  define UART_IIR_IID_NONE       (1  << UART_IIR_IID_SHIFT)
#  define UART_IIR_IID_TXEMPTY    (2  << UART_IIR_IID_SHIFT)
#  define UART_IIR_IID_RECV       (4  << UART_IIR_IID_SHIFT)
#  define UART_IIR_IID_LINESTATUS (6  << UART_IIR_IID_SHIFT)
#  define UART_IIR_IID_BUSY       (7  << UART_IIR_IID_SHIFT)
#  define UART_IIR_IID_TIMEOUT    (12 << UART_IIR_IID_SHIFT)
#define UART_IIR_FEFLAG_SHIFT   6
#define UART_IIR_FEFLAG_MASK    (3 << UART_IIR_FEFLAG_SHIFT)
#  define UART_IIR_FEFLAG_DISABLE (0 << UART_IIR_FEFLAG_SHIFT)
#  define UART_IIR_FEFLAG_ENABLE  (3 << UART_IIR_FEFLAG_SHIFT)

/* FCR */
#define UART_FCR_FIFOE          (1 << 0)
#define UART_FCR_RFIFOR         (1 << 1)
#define UART_FCR_XFIFOR         (1 << 2)
#define UART_FCR_DMAM           (1 << 3)
#define UART_FCR_TFT_SHIFT      4
#define UART_FCR_TFT_MASK       (3 << UART_FCR_TFT_SHIFT)
#  define UART_FCR_TFT_EMPTY    (0 << UART_FCR_TFT_SHIFT)
#  define UART_FCR_TFT_TWO      (1 << UART_FCR_TFT_SHIFT)
#  define UART_FCR_TFT_QUARTER  (2 << UART_FCR_TFT_SHIFT)
#  define UART_FCR_TFT_HALF     (3 << UART_FCR_TFT_SHIFT)
#define UART_FCR_RT_SHIFT       6
#define UART_FCR_RT_MASK        (3 << UART_FCR_RT_SHIFT)
#  define UART_FCR_RT_ONE       (0 << UART_FCR_RT_SHIFT)
#  define UART_FCR_RT_QUARTER   (1 << UART_FCR_RT_SHIFT)
#  define UART_FCR_RT_HALF      (2 << UART_FCR_RT_SHIFT)
#  define UART_FCR_RT_MINUS2    (3 << UART_FCR_RT_SHIFT)

/* LCR */
#define UART_LCR_DLS_SHIFT      0
#define UART_LCR_DLS_MASK       (3 << UART_LCR_DLS_SHIFT)
#  define UART_LCR_DLS_5BITS    (0 << UART_LCR_DLS_SHIFT)
#  define UART_LCR_DLS_6BITS    (1 << UART_LCR_DLS_SHIFT)
#  define UART_LCR_DLS_7BITS    (2 << UART_LCR_DLS_SHIFT)
#  define UART_LCR_DLS_8BITS    (3 << UART_LCR_DLS_SHIFT)
#define UART_LCR_STOP           (1 << 2)
#define UART_LCR_PEN            (1 << 3)
#define UART_LCR_EPS            (1 << 4)
#define UART_LCR_BC             (1 << 6)
#define UART_LCR_DLAB           (1 << 7)

/* MCR */
#define UART_MCR_DTR            (1 << 0)
#define UART_MCR_RTS            (1 << 1)
#define UART_MCR_LOOP           (1 << 4)
#define UART_MCR_AFCE           (1 << 5)
#define UART_MCR_SIRE           (1 << 6)

/* LSR */
#define UART_LSR_DR             (1 << 0)
#define UART_LSR_OE             (1 << 1)
#define UART_LSR_PE             (1 << 2)
#define UART_LSR_FE             (1 << 3)
#define UART_LSR_BI             (1 << 4)
#define UART_LSR_THRE           (1 << 5)
#define UART_LSR_TEMT           (1 << 6)
#define UART_LSR_FIFOERR        (1 << 7)

/* MSR */
#define UART_MSR_DCTS           (1 << 0)
#define UART_MSR_DDSR           (1 << 1)
#define UART_MSR_TERI           (1 << 2)
#define UART_MSR_DDCD           (1 << 3)
#define UART_MSR_CTS            (1 << 4)
#define UART_MSR_DSR            (1 << 5)
#define UART_MSR_RI             (1 << 6)
#define UART_MSR_DCD            (1 << 7)

/* USR */
#define UART_USR_BUSY           (1 << 0)
#define UART_USR_TFNF           (1 << 1)
#define UART_USR_TFE            (1 << 2)
#define UART_USR_RFNE           (1 << 3)
#define UART_USR_RFF            (1 << 4)

/* HALT */
#define UART_HALT_HALT_TX       (1 << 0)
#define UART_HALT_FORCECFG      (1 << 1)
#define UART_HALT_LCRUP         (1 << 2)
#define UART_HALT_SIR_TX_INVERT (1 << 4)
#define UART_HALT_SIR_RX_INVERT (1 << 5)

/* UART clock: OSC24M = 24 MHz */

#define T113_UART_CLK           24000000
#define T113_UART_DL(baud)      (T113_UART_CLK / ((baud) << 4))

#endif /* __ARCH_ARM_SRC_T113_HARDWARE_T113_UART_H */
