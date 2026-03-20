/****************************************************************************
 * arch/arm/src/t113/t113_gpio.c
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
#include <stdbool.h>
#include <errno.h>

#include "arm_internal.h"
#include "hardware/t113_gpio.h"
#include "t113_gpio.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: t113_gpio_config
 *
 * Description:
 *   Configure a GPIO pin using the encoded pin descriptor from
 *   t113_gpio.h.  Sets function, pull, and drive level.
 *
 ****************************************************************************/

int t113_gpio_config(uint16_t pinset)
{
  unsigned int port = T113_GPIO_PORT(pinset);
  unsigned int pin  = T113_GPIO_PINNO(pinset);
  unsigned int func = T113_GPIO_FUNC(pinset);
  unsigned int pull = T113_GPIO_PULL_VAL(pinset);
  unsigned int drv  = T113_GPIO_DRV_VAL(pinset);
  uint32_t reg;
  uint32_t val;

  /* Set pin function in CFG register (4 bits per pin, 8 pins per reg) */

  reg = T113_PIO_CFG(port, pin >> 3);
  val = getreg32(reg);
  val &= ~(0xf << ((pin & 7) * 4));
  val |= (func & 0xf) << ((pin & 7) * 4);
  putreg32(val, reg);

  /* Set pull-up/down in PULL register (2 bits per pin, 16 pins per reg) */

  if (pin < 16)
    {
      reg = T113_PIO_PULL0(port);
    }
  else
    {
      reg = T113_PIO_PULL1(port);
    }

  val = getreg32(reg);
  val &= ~(0x3 << ((pin & 15) * 2));
  val |= (pull & 0x3) << ((pin & 15) * 2);
  putreg32(val, reg);

  /* Set drive level in DRV register (2 bits per pin, 16 pins per reg) */

  if (pin < 16)
    {
      reg = T113_PIO_DRV0(port);
    }
  else
    {
      reg = T113_PIO_DRV1(port);
    }

  val = getreg32(reg);
  val &= ~(0x3 << ((pin & 15) * 2));
  val |= (drv & 0x3) << ((pin & 15) * 2);
  putreg32(val, reg);

  return OK;
}

/****************************************************************************
 * Name: t113_gpio_write
 *
 * Description:
 *   Write a value to a GPIO output pin.
 *
 ****************************************************************************/

void t113_gpio_write(uint16_t pinset, bool value)
{
  unsigned int port = T113_GPIO_PORT(pinset);
  unsigned int pin  = T113_GPIO_PINNO(pinset);
  uint32_t reg;
  uint32_t val;

  reg = T113_PIO_DAT(port);
  val = getreg32(reg);

  if (value)
    {
      val |= (1 << pin);
    }
  else
    {
      val &= ~(1 << pin);
    }

  putreg32(val, reg);
}

/****************************************************************************
 * Name: t113_gpio_read
 *
 * Description:
 *   Read the current state of a GPIO pin.
 *
 ****************************************************************************/

bool t113_gpio_read(uint16_t pinset)
{
  unsigned int port = T113_GPIO_PORT(pinset);
  unsigned int pin  = T113_GPIO_PINNO(pinset);

  return (getreg32(T113_PIO_DAT(port)) >> pin) & 1;
}
