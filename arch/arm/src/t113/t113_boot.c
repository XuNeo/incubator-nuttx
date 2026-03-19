/****************************************************************************
 * arch/arm/src/t113/t113_boot.c
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
#include <debug.h>

#ifdef CONFIG_LEGACY_PAGING
#  include <nuttx/page.h>
#endif

#include "chip.h"

#ifdef CONFIG_SMP
void t113_cpu_enable(void);
void arm_enable_smp(int cpu);
#endif
#include "arm.h"
#include "mmu.h"
#include "arm_internal.h"
#include "t113_lowputc.h"
#include "t113_boot.h"

#ifdef CONFIG_T113_BOOT0
extern uint32_t _boot0_start;
volatile uint32_t *g_boot0_anchor = &_boot0_start;
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_ARCH_ROMPGTABLE
#  define NMAPPINGS \
     (sizeof(section_mapping) / sizeof(struct section_mapping_s))
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

extern uint8_t _vector_start[];
extern uint8_t _vector_end[];

#ifndef CONFIG_ARCH_ROMPGTABLE
static const struct section_mapping_s section_mapping[] =
{
  { T113_SRAM_PSECTION, T113_SRAM_VSECTION,
    T113_SRAM_MMUFLAGS, T113_SRAM_NSECTIONS
  },
  { T113_SP0_PSECTION,    T113_SP0_VSECTION,
    T113_SP0_MMUFLAGS,    T113_SP0_NSECTIONS
  },
  { T113_SP1_PSECTION,    T113_SP1_VSECTION,
    T113_SP1_MMUFLAGS,    T113_SP1_NSECTIONS
  },
  { T113_SH0_PSECTION,    T113_SH0_VSECTION,
    T113_SH0_MMUFLAGS,    T113_SH0_NSECTIONS
  },
  { T113_SH2_PSECTION,    T113_SH2_VSECTION,
    T113_SH2_MMUFLAGS,    T113_SH2_NSECTIONS
  },
  { T113_PERIPH_PSECTION, T113_PERIPH_VSECTION,
    T113_PERIPH_MMUFLAGS, T113_PERIPH_NSECTIONS
  },
  { T113_APBS0_PSECTION,  T113_APBS0_VSECTION,
    T113_APBS0_MMUFLAGS,  T113_APBS0_NSECTIONS
  },
  { T113_CPUX_PSECTION,   T113_CPUX_VSECTION,
    T113_CPUX_MMUFLAGS,   T113_CPUX_NSECTIONS
  },
  { T113_DDR_MAPPADDR,    T113_DDR_MAPVADDR,
    T113_DDR_MMUFLAGS,    T113_DDR_NSECTIONS
  },
};
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

#ifndef CONFIG_ARCH_ROMPGTABLE
static inline void t113_setupmappings(void)
{
  mmu_l1_map_regions(section_mapping, NMAPPINGS);
}
#endif

#if !defined(CONFIG_ARCH_ROMPGTABLE) && !defined(CONFIG_ARCH_LOWVECTORS)
static void t113_vectormapping(void)
{
  uint32_t vector_paddr = T113_VECTOR_PADDR & PTE_SMALL_PADDR_MASK;
  uint32_t vector_vaddr = T113_VECTOR_VADDR & PTE_SMALL_PADDR_MASK;
  uint32_t end_paddr = T113_VECTOR_PADDR +
                       (_vector_end - _vector_start);

  while (vector_paddr < end_paddr)
    {
      mmu_l2_setentry(VECTOR_L2_VBASE, vector_paddr, vector_vaddr,
                      MMU_L2_VECTORFLAGS);
      vector_paddr += 4096;
      vector_vaddr += 4096;
    }

  mmu_l1_setentry(VECTOR_L2_PBASE & PMD_PTE_PADDR_MASK,
                  T113_VECTOR_VADDR & PMD_PTE_PADDR_MASK,
                  MMU_L1_VECTORFLAGS);
}
#else
#  define t113_vectormapping()
#endif

static void t113_copyvectorblock(void)
{
  uint32_t *src  = (uint32_t *)_vector_start;
  uint32_t *end  = (uint32_t *)_vector_end;
  uint32_t *dest = (uint32_t *)(T113_VECTOR_VSRAM);

  while (src < end)
    {
      *dest++ = *src++;
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void arm_boot(void)
{
#ifndef CONFIG_ARCH_ROMPGTABLE
  t113_setupmappings();
  t113_vectormapping();
#endif

  t113_copyvectorblock();

#ifdef CONFIG_ARCH_LOWVECTORS
  __asm__ __volatile__(
    "mcr p15, 0, %0, c12, c0, 0\n"
    :
    : "r"(CONFIG_RAM_START)
    : "memory"
  );
#endif

  arm_fpuconfig();

#ifdef CONFIG_SMP
  arm_enable_smp(0);
#endif

#ifdef CONFIG_BOOT_SDRAM_DATA
  arm_data_initialize();
#endif

#ifdef USE_EARLYSERIALINIT
  arm_earlyserialinit();
#endif

#ifdef CONFIG_SMP
  t113_cpu_enable();
#endif

  t113_boardinitialize();
}
