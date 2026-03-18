/****************************************************************************
 * arch/arm/src/t113/t113_timerisr.c
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
#include <time.h>
#include <assert.h>
#include <debug.h>

#include <nuttx/arch.h>

#include <arch/irq.h>
#include <arch/armv7-a/cp15.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define T113_CNTFRQ           24000000
#define GIC_IRQ_SEC_PHY_TIMER 29

/****************************************************************************
 * Private Data
 ****************************************************************************/

static uint32_t g_timer_reload;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: t113_timerisr
 *
 * Description:
 *   The timer ISR will perform a variety of services for various portions
 *   of the systems.
 *
 ****************************************************************************/

static int t113_timerisr(int irq, void *context, void *arg)
{
  CP15_SET(CNTP_CTL, 0);
  CP15_SET(CNTP_TVAL, g_timer_reload);
  CP15_SET(CNTP_CTL, 1);
  nxsched_process_timer();
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Function:  up_timer_initialize
 *
 * Description:
 *   This function is called during start-up to initialize
 *   the timer interrupt.  Uses the ARMv7-A generic timer (CNTP).
 *
 ****************************************************************************/

void up_timer_initialize(void)
{
  uint32_t cntfrq;

  CP15_SET(CNTFRQ, T113_CNTFRQ);
  cntfrq = CP15_GET(CNTFRQ);

  g_timer_reload = cntfrq / CONFIG_USEC_PER_TICK;

  up_disable_irq(GIC_IRQ_SEC_PHY_TIMER);
  irq_attach(GIC_IRQ_SEC_PHY_TIMER, t113_timerisr, NULL);
  CP15_SET(CNTP_TVAL, g_timer_reload);
  CP15_SET(CNTP_CTL, 1);
  up_enable_irq(GIC_IRQ_SEC_PHY_TIMER);
}
