/****************************************************************************
 * arch/arm/src/t113/hardware/t113_gpio.h
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

#ifndef __ARCH_ARM_SRC_T113_HARDWARE_T113_GPIO_H
#define __ARCH_ARM_SRC_T113_HARDWARE_T113_GPIO_H

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* PIO controller base address.
 * T113 has banks B-G (no bank A).  Bank index: B=1 C=2 D=3 E=4 F=5 G=6.
 * Each bank occupies 0x30 bytes starting at PIO_BASE + bank * 0x30.
 *
 * Register layout per bank (offset from bank base):
 *   0x00  Pn_CFG0  - pin 0-7 config   (4 bits/pin)
 *   0x04  Pn_CFG1  - pin 8-15 config
 *   0x08  Pn_CFG2  - pin 16-23 config
 *   0x0C  Pn_CFG3  - pin 24-31 config
 *   0x10  Pn_DAT   - data register
 *   0x14  Pn_DRV0  - drive level pin 0-15 (2 bits/pin)
 *   0x18  Pn_DRV1  - drive level pin 16-31
 *   0x1C  Pn_DRV2  - reserved on some banks
 *   0x20  Pn_DRV3  - reserved on some banks
 *   0x24  Pn_PULL0 - pull up/down pin 0-15 (2 bits/pin)
 *   0x28  Pn_PULL1 - pull up/down pin 16-31
 */

#define T113_PIO_BASE           0x02000000

/* Bank indices */

#define T113_GPIO_PORTB         1
#define T113_GPIO_PORTC         2
#define T113_GPIO_PORTD         3
#define T113_GPIO_PORTE         4
#define T113_GPIO_PORTF         5
#define T113_GPIO_PORTG         6

/* Per-bank register offsets */

#define T113_PIO_BANK_OFFSET(n)  ((n) * 0x30)

#define T113_PIO_CFG(n, r)      (T113_PIO_BASE + T113_PIO_BANK_OFFSET(n) + (r) * 4)
#define T113_PIO_CFG0(n)        (T113_PIO_BASE + T113_PIO_BANK_OFFSET(n) + 0x00)
#define T113_PIO_CFG1(n)        (T113_PIO_BASE + T113_PIO_BANK_OFFSET(n) + 0x04)
#define T113_PIO_CFG2(n)        (T113_PIO_BASE + T113_PIO_BANK_OFFSET(n) + 0x08)
#define T113_PIO_CFG3(n)        (T113_PIO_BASE + T113_PIO_BANK_OFFSET(n) + 0x0c)
#define T113_PIO_DAT(n)         (T113_PIO_BASE + T113_PIO_BANK_OFFSET(n) + 0x10)
#define T113_PIO_DRV0(n)        (T113_PIO_BASE + T113_PIO_BANK_OFFSET(n) + 0x14)
#define T113_PIO_DRV1(n)        (T113_PIO_BASE + T113_PIO_BANK_OFFSET(n) + 0x18)
#define T113_PIO_PULL0(n)       (T113_PIO_BASE + T113_PIO_BANK_OFFSET(n) + 0x24)
#define T113_PIO_PULL1(n)       (T113_PIO_BASE + T113_PIO_BANK_OFFSET(n) + 0x28)

/* Pin config function values (4 bits per pin in CFG registers) */

#define T113_GPIO_INPUT         0x00
#define T113_GPIO_OUTPUT        0x01
#define T113_GPIO_FUNC2         0x02
#define T113_GPIO_FUNC3         0x03
#define T113_GPIO_FUNC4         0x04
#define T113_GPIO_FUNC5         0x05
#define T113_GPIO_FUNC6         0x06
#define T113_GPIO_FUNC7         0x07
#define T113_GPIO_FUNC8         0x08
#define T113_GPIO_IRQ           0x0e
#define T113_GPIO_DISABLED      0x0f

/* Pull-up/down values (2 bits per pin in PULL registers) */

#define T113_GPIO_PULL_NONE     0x00
#define T113_GPIO_PULL_UP       0x01
#define T113_GPIO_PULL_DOWN     0x02

/* Drive level values (2 bits per pin in DRV registers) */

#define T113_GPIO_DRV_LEVEL0    0x00
#define T113_GPIO_DRV_LEVEL1    0x01
#define T113_GPIO_DRV_LEVEL2    0x02
#define T113_GPIO_DRV_LEVEL3    0x03

/* GPIO pin encoding:
 *   bits[15:12] = bank (1=B, 2=C, ... 6=G)
 *   bits[11:8]  = function (0=input, 1=output, 2-8=alt, 0xe=irq, 0xf=dis)
 *   bits[7:6]   = pull (0=none, 1=up, 2=down)
 *   bits[5:4]   = drive level (0-3)
 *   bits[3:0]   = pin number within bank (0-31)
 */

#define T113_GPIO_PIN_SHIFT     0
#define T113_GPIO_PIN_MASK      (0xf << T113_GPIO_PIN_SHIFT)
#define T113_GPIO_DRV_SHIFT     4
#define T113_GPIO_DRV_MASK      (0x3 << T113_GPIO_DRV_SHIFT)
#define T113_GPIO_PULL_SHIFT    6
#define T113_GPIO_PULL_MASK     (0x3 << T113_GPIO_PULL_SHIFT)
#define T113_GPIO_FUNC_SHIFT    8
#define T113_GPIO_FUNC_MASK     (0xf << T113_GPIO_FUNC_SHIFT)
#define T113_GPIO_PORT_SHIFT    12
#define T113_GPIO_PORT_MASK     (0xf << T113_GPIO_PORT_SHIFT)

#define T113_GPIO_ENCODE(port, pin, func, pull, drv) \
  ((((port) & 0xf) << T113_GPIO_PORT_SHIFT) | \
   (((func) & 0xf) << T113_GPIO_FUNC_SHIFT) | \
   (((pull) & 0x3) << T113_GPIO_PULL_SHIFT) | \
   (((drv)  & 0x3) << T113_GPIO_DRV_SHIFT)  | \
   (((pin)  & 0xf) << T113_GPIO_PIN_SHIFT))

#define T113_GPIO_PORT(x)       (((x) & T113_GPIO_PORT_MASK) >> T113_GPIO_PORT_SHIFT)
#define T113_GPIO_PINNO(x)      (((x) & T113_GPIO_PIN_MASK) >> T113_GPIO_PIN_SHIFT)
#define T113_GPIO_FUNC(x)       (((x) & T113_GPIO_FUNC_MASK) >> T113_GPIO_FUNC_SHIFT)
#define T113_GPIO_PULL_VAL(x)   (((x) & T113_GPIO_PULL_MASK) >> T113_GPIO_PULL_SHIFT)
#define T113_GPIO_DRV_VAL(x)    (((x) & T113_GPIO_DRV_MASK) >> T113_GPIO_DRV_SHIFT)

/* Max pins per bank */

#define T113_GPIO_NPINS_B       9
#define T113_GPIO_NPINS_C       8
#define T113_GPIO_NPINS_D       23
#define T113_GPIO_NPINS_E       18
#define T113_GPIO_NPINS_F       7
#define T113_GPIO_NPINS_G       16

#endif /* __ARCH_ARM_SRC_T113_HARDWARE_T113_GPIO_H */
