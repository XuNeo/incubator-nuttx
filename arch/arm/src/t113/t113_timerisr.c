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
#include <nuttx/arch.h>
#include <arch/irq.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define GIC_IRQ_SEC_PHY_TIMER 29

/****************************************************************************
 * Private Data
 ****************************************************************************/

static uint32_t g_timer_reload;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static inline uint32_t read_cntfrq(void)
{
  uint32_t val;
  __asm__ volatile("mrc p15, 0, %0, c14, c0, 0" : "=r"(val));
  return val;
}

static inline void write_cntp_tval(uint32_t val)
{
  __asm__ volatile("mcr p15, 0, %0, c14, c2, 0" :: "r"(val));
  __asm__ volatile("isb");
}

static inline void write_cntp_ctl(uint32_t val)
{
  __asm__ volatile("mcr p15, 0, %0, c14, c2, 1" :: "r"(val));
  __asm__ volatile("isb");
}

static int t113_timerisr(int irq, void *context, void *arg)
{
  write_cntp_ctl(0);
  write_cntp_tval(g_timer_reload);
  write_cntp_ctl(1);
  nxsched_process_timer();
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void up_timer_initialize(void)
{
  uint32_t cntfrq = read_cntfrq();
  if (cntfrq == 0)
    {
      cntfrq = 24000000;
      __asm__ volatile("mcr p15, 0, %0, c14, c0, 0" :: "r"(cntfrq));
    }

  g_timer_reload = cntfrq / CONFIG_USEC_PER_TICK;

  irq_attach(GIC_IRQ_SEC_PHY_TIMER, t113_timerisr, NULL);
  write_cntp_tval(g_timer_reload);
  write_cntp_ctl(1);
  up_enable_irq(GIC_IRQ_SEC_PHY_TIMER);
}
