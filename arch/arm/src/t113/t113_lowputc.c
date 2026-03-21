/****************************************************************************
 * arch/arm/src/t113/t113_lowputc.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <debug.h>

#include <arch/board/board.h>

#include "arm_internal.h"
#include "hardware/t113_uart.h"
#include "t113_lowputc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#if defined(CONFIG_UART0_SERIAL_CONSOLE)
#  define CONSOLE_BASE   T113_UART0_VADDR
#  define CONSOLE_BAUD   CONFIG_UART0_BAUD
#  define CONSOLE_BITS   CONFIG_UART0_BITS
#  define CONSOLE_PARITY CONFIG_UART0_PARITY
#  define CONSOLE_2STOP  CONFIG_UART0_2STOP
#elif defined(CONFIG_UART1_SERIAL_CONSOLE)
#  define CONSOLE_BASE   T113_UART1_VADDR
#  define CONSOLE_BAUD   CONFIG_UART1_BAUD
#  define CONSOLE_BITS   CONFIG_UART1_BITS
#  define CONSOLE_PARITY CONFIG_UART1_PARITY
#  define CONSOLE_2STOP  CONFIG_UART1_2STOP
#elif defined(CONFIG_UART2_SERIAL_CONSOLE)
#  define CONSOLE_BASE   T113_UART2_VADDR
#  define CONSOLE_BAUD   CONFIG_UART2_BAUD
#  define CONSOLE_BITS   CONFIG_UART2_BITS
#  define CONSOLE_PARITY CONFIG_UART2_PARITY
#  define CONSOLE_2STOP  CONFIG_UART2_2STOP
#elif defined(CONFIG_UART3_SERIAL_CONSOLE)
#  define CONSOLE_BASE   T113_UART3_VADDR
#  define CONSOLE_BAUD   CONFIG_UART3_BAUD
#  define CONSOLE_BITS   CONFIG_UART3_BITS
#  define CONSOLE_PARITY CONFIG_UART3_PARITY
#  define CONSOLE_2STOP  CONFIG_UART3_2STOP
#elif defined(HAVE_SERIAL_CONSOLE)
#  error "No CONFIG_UARTn_SERIAL_CONSOLE setting"
#endif

#if CONSOLE_BITS == 5
#  define CONSOLE_LCR_DLS UART_LCR_DLS_5BITS
#elif CONSOLE_BITS == 6
#  define CONSOLE_LCR_DLS UART_LCR_DLS_6BITS
#elif CONSOLE_BITS == 7
#  define CONSOLE_LCR_DLS UART_LCR_DLS_7BITS
#elif CONSOLE_BITS == 8
#  define CONSOLE_LCR_DLS UART_LCR_DLS_8BITS
#elif defined(HAVE_SERIAL_CONSOLE)
#  error "Invalid CONFIG_UARTn_BITS setting for console"
#endif

#if CONSOLE_PARITY == 0
#  define CONSOLE_LCR_PAR 0
#elif CONSOLE_PARITY == 1
#  define CONSOLE_LCR_PAR UART_LCR_PEN
#elif CONSOLE_PARITY == 2
#  define CONSOLE_LCR_PAR (UART_LCR_PEN | UART_LCR_EPS)
#elif defined(HAVE_SERIAL_CONSOLE)
#  error "Invalid CONFIG_UARTn_PARITY setting for CONSOLE"
#endif

#if CONSOLE_2STOP != 0
#  define CONSOLE_LCR_STOP UART_LCR_STOP
#else
#  define CONSOLE_LCR_STOP 0
#endif

#define CONSOLE_LCR_VALUE \
  (CONSOLE_LCR_DLS | CONSOLE_LCR_PAR | CONSOLE_LCR_STOP)

#define T113_CCU_UART_BGR_REG  (0x0200190c)
#define T113_GPIO_PORTF_CFG0   (0x020000f0)
#define T113_UART_USR_TFNF     (1 << 1)

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void arm_lowputc(char ch)
{
#if defined(HAVE_SERIAL_CONSOLE)
  while (!(getreg32(CONSOLE_BASE + T113_UART_USR_OFFSET) &
           T113_UART_USR_TFNF));
  putreg32((uint32_t)ch, CONSOLE_BASE + T113_UART_THR_OFFSET);
#endif
}

void t113_lowsetup(void)
{
#if defined(HAVE_SERIAL_CONSOLE) && !defined(CONFIG_SUPPRESS_UART_CONFIG)
  uint32_t reg;
  int i;

  reg = getreg32(T113_CCU_UART_BGR_REG);
  reg &= ~(1 << 16);
  putreg32(reg, T113_CCU_UART_BGR_REG);
  for (i = 0; i < 100; i++);
  reg |= (1 << 16);
  putreg32(reg, T113_CCU_UART_BGR_REG);

  reg = getreg32(T113_CCU_UART_BGR_REG);
  reg &= ~(1 << 0);
  putreg32(reg, T113_CCU_UART_BGR_REG);
  for (i = 0; i < 100; i++);
  reg |= (1 << 0);
  putreg32(reg, T113_CCU_UART_BGR_REG);

  /* PF2 = UART0_TX (func 3), PF4 = UART0_RX (func 3) */

  reg = getreg32(T113_GPIO_PORTF_CFG0);
  reg &= ~(0xf << (BOARD_UART0_TX_PIN * 4));
  reg |=  (BOARD_UART0_PIN_FUNC << (BOARD_UART0_TX_PIN * 4));
  reg &= ~(0xf << (BOARD_UART0_RX_PIN * 4));
  reg |=  (BOARD_UART0_PIN_FUNC << (BOARD_UART0_RX_PIN * 4));
  putreg32(reg, T113_GPIO_PORTF_CFG0);

  putreg32(UART_FCR_RFIFOR | UART_FCR_XFIFOR,
           CONSOLE_BASE + T113_UART_FCR_OFFSET);
  putreg32(UART_FCR_FIFOE | UART_FCR_RT_ONE,
           CONSOLE_BASE + T113_UART_FCR_OFFSET);
  putreg32(CONSOLE_LCR_VALUE | UART_LCR_DLAB,
           CONSOLE_BASE + T113_UART_LCR_OFFSET);
  putreg32(T113_UART_DL(CONSOLE_BAUD) >> 8,
           CONSOLE_BASE + T113_UART_DLH_OFFSET);
  putreg32(T113_UART_DL(CONSOLE_BAUD) & 0xff,
           CONSOLE_BASE + T113_UART_DLL_OFFSET);
  putreg32(CONSOLE_LCR_VALUE,
           CONSOLE_BASE + T113_UART_LCR_OFFSET);
  putreg32(UART_FCR_RT_ONE | UART_FCR_XFIFOR |
           UART_FCR_RFIFOR | UART_FCR_FIFOE,
           CONSOLE_BASE + T113_UART_FCR_OFFSET);

  putreg32(0, CONSOLE_BASE + T113_UART_IER_OFFSET);
  putreg32(1 << (34 - 32), 0x03021184);
#endif
}
