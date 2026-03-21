/****************************************************************************
 * arch/arm/src/t113/t113_cpuboot.c
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

#include <nuttx/arch.h>
#include <arch/irq.h>
#include <arch/barriers.h>
#include <arch/armv7-a/cp15.h>

#include "arm_internal.h"
#include "smp.h"
#include "gic.h"
#include "hardware/t113_cpucfg.h"

#ifdef CONFIG_SMP

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define T113_CNTFRQ  24000000

/****************************************************************************
 * Public Data
 ****************************************************************************/

extern uint8_t _vector_start[];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void t113_set_cpu_boot_entry(int cpu, uint32_t entry)
{
  putreg32(entry, T113_CPU_SOFT_ENT(cpu));
  up_udelay(100);
  arm_dsb(15);
  arm_isb();
}

static void t113_enable_cpu(int cpu)
{
  uint32_t val;

  /* Assert core reset */

  val = getreg32(T113_C0_RST_CTRL);
  val &= ~(1 << cpu);
  putreg32(val, T113_C0_RST_CTRL);
  up_udelay(100);

  /* L1RSTDISABLE hold low */

  val = getreg32(T113_C0_CTRL_REG0);
  val &= ~(1 << cpu);
  putreg32(val, T113_C0_CTRL_REG0);
  up_udelay(200);

  /* Deassert core reset */

  val = getreg32(T113_C0_RST_CTRL);
  val |= (1 << cpu);
  putreg32(val, T113_C0_RST_CTRL);
  up_udelay(100);

  arm_dsb(15);
  arm_isb();
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: t113_cpu_disable
 ****************************************************************************/

void t113_cpu_disable(void)
{
  uint32_t val;

  val = getreg32(T113_C0_RST_CTRL);
  val &= ~C0_RST_CTRL_CORE1;
  putreg32(val, T113_C0_RST_CTRL);
}

/****************************************************************************
 * Name: t113_cpu_enable
 ****************************************************************************/

void t113_cpu_enable(void)
{
  int cpu;

  for (cpu = 1; cpu < CONFIG_SMP_NCPUS; cpu++)
    {
      t113_set_cpu_boot_entry(cpu, (uint32_t)__cpu1_start);
      t113_enable_cpu(cpu);
    }
}

/****************************************************************************
 * Name: arm_cpu_boot
 *
 * Description:
 *   Continues the C-level initialization started by the assembly language
 *   __cpu[n]_start function.  Called on each secondary CPU after low-level
 *   setup (MMU, stacks) is complete.
 *
 ****************************************************************************/

void arm_cpu_boot(int cpu)
{
  arm_enable_smp(cpu);

#ifdef CONFIG_ARCH_FPU
  arm_fpuconfig();
#endif

  CP15_SET(CNTFRQ, T113_CNTFRQ);

  up_irqinitialize();

  for (; ; )
    {
      asm("WFI");
    }
}

#endif /* CONFIG_SMP */
