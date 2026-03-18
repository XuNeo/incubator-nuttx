/****************************************************************************
 * arch/arm/src/t113/t113_irq.c
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
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>

#include "arm_internal.h"
#include "sctlr.h"
#include "gic.h"

extern uint8_t _vector_start[];
extern uint8_t _vector_end[];

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define T113_C0_RST_CTRL    0x09010000
#define T113_USB0_BASE      0x04101000
#define T113_USB0_INTUSBE   0x06
#define T113_USB0_INTRTXE   0x08
#define T113_USB0_INTRRXE   0x0a
#define T113_IRQ_USB0       61

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: t113_usb0_quiesce
 *
 * Description:
 *   Disable USB0 interrupt sources left active by xfel FEL mode.
 *   The boot0 path resets USB, so this is only needed for the
 *   xfel load-to-DDR workflow.
 *
 ****************************************************************************/

static void t113_usb0_quiesce(void)
{
  putreg8(0, T113_USB0_BASE + T113_USB0_INTUSBE);
  putreg8(0, T113_USB0_BASE + T113_USB0_INTRTXE);
  putreg16(0, T113_USB0_BASE + T113_USB0_INTRRXE);

  up_disable_irq(T113_IRQ_USB0);
  putreg32(1 << (T113_IRQ_USB0 % 32), GIC_ICDICPR(T113_IRQ_USB0));
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_irqinitialize
 *
 * Description:
 *   This function is called by up_initialize() during the bring-up of the
 *   system.  It is the responsibility of this function to put the interrupt
 *   subsystem into the working and ready state.
 *
 ****************************************************************************/

void up_irqinitialize(void)
{
  putreg32(getreg32(T113_C0_RST_CTRL) & ~(1 << 1), T113_C0_RST_CTRL);

  arm_gic0_initialize();
  arm_gic_initialize();

  t113_usb0_quiesce();

#ifdef CONFIG_ARCH_LOWVECTORS
  DEBUGASSERT((((uintptr_t)_vector_start) & ~VBAR_MASK) == 0);
  cp15_wrvbar((uint32_t)_vector_start);
#endif

#ifndef CONFIG_SUPPRESS_INTERRUPTS
  up_irq_enable();
#endif
}
