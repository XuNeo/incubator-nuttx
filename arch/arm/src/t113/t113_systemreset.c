/****************************************************************************
 * arch/arm/src/t113/t113_systemreset.c
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

#include <nuttx/arch.h>
#include <nuttx/board.h>

#ifdef CONFIG_BOARDCTL_RESET_CAUSE
#  include <sys/boardctl.h>
#endif

#include "arm_internal.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* T113 has no dedicated reset-status register.  We use RTC General
 * Purpose Data Register 0 (GP_DATA_REG[0]) at 0x07090100 as a
 * software reset-cause marker.  This register is in the VDD_RTC
 * power domain and retains its value across warm resets.
 *
 * Protocol:
 *   - board_reset() writes a magic value before triggering WDT soft
 *     reset so we can distinguish software-initiated resets from
 *     power-on or external resets.
 *   - t113_get_reset_cause() reads GP_DATA_REG[0] at boot, then
 *     clears it so the next cold boot sees zero.
 */

#define T113_RTC_BASE           0x07090000
#define T113_RTC_GP_DATA0       (T113_RTC_BASE + 0x100)

/* Magic values written before reset */

#define T113_RESET_MAGIC_SW     0x5752  /* "WR" - software warm reset */

/* Watchdog registers (same as t113_wdt.c) */

#define T113_TIMER_BASE         0x02050000
#define WDOG_SOFT_RST_REG       (T113_TIMER_BASE + 0x00a8)
#define WDOG_SOFT_RST_KEY       (0x16aa << 16)

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: board_reset_cause
 *
 * Description:
 *   Get the cause of the last reset by reading the software marker
 *   in RTC GP_DATA_REG[0].
 *
 ****************************************************************************/

#ifdef CONFIG_BOARDCTL_RESET_CAUSE
int board_reset_cause(FAR struct boardioc_reset_cause_s *cause)
{
  uint32_t marker;

  marker = getreg32(T113_RTC_GP_DATA0);

  /* Clear marker for next boot cycle */

  putreg32(0, T113_RTC_GP_DATA0);

  if (marker == T113_RESET_MAGIC_SW)
    {
      cause->cause = BOARDIOC_RESETCAUSE_CPU_SOFT;
    }
  else
    {
      cause->cause = BOARDIOC_RESETCAUSE_SYS_CHIPPOR;
    }

  cause->flag = 0;

  sinfo("reset cause: %d (marker=0x%08" PRIx32 ")\n",
        cause->cause, marker);
  return 0;
}
#endif

/****************************************************************************
 * Name: board_reset
 *
 * Description:
 *   Reset board.  Write a software-reset marker to RTC GP_DATA_REG[0]
 *   then trigger a watchdog soft reset.
 *
 ****************************************************************************/

int board_reset(int status)
{
  /* Mark this as a software-initiated reset */

  putreg32(T113_RESET_MAGIC_SW, T113_RTC_GP_DATA0);

  /* Trigger watchdog module soft reset — instant full-chip reset */

  putreg32(WDOG_SOFT_RST_KEY | 1, WDOG_SOFT_RST_REG);

  /* Wait for reset */

  for (; ; );
  return 0;
}
