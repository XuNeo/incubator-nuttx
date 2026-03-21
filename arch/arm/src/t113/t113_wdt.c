/****************************************************************************
 * arch/arm/src/t113/t113_wdt.c
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
#include <errno.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/timers/watchdog.h>

#include "arm_internal.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define T113_TIMER_BASE       0x02050000

#define WDOG_IRQ_EN_REG       (T113_TIMER_BASE + 0x00a0)
#define WDOG_IRQ_STA_REG      (T113_TIMER_BASE + 0x00a4)
#define WDOG_SOFT_RST_REG     (T113_TIMER_BASE + 0x00a8)
#define WDOG_CTRL_REG         (T113_TIMER_BASE + 0x00b0)
#define WDOG_CFG_REG          (T113_TIMER_BASE + 0x00b4)
#define WDOG_MODE_REG         (T113_TIMER_BASE + 0x00b8)

#define WDOG_SOFT_RST_KEY     (0x16aa << 16)
#define WDOG_CTRL_KEY         (0xa57 << 1)
#define WDOG_CTRL_RESTART     (WDOG_CTRL_KEY | 1)
#define WDOG_CFG_RESET        0x01
#define WDOG_CFG_IRQ          0x02
#define WDOG_MODE_EN          (1 << 0)

/* WDOG_MODE_REG bit[7:4] = interval select:
 *   0=0.5s 1=1s 2=2s 3=3s 4=4s 5=5s 6=6s
 *   7=8s 8=10s 9=12s 10=14s 11=16s
 */

#define WDOG_MODE_INTV_SHIFT  4
#define WDOG_MODE_INTV_MASK   (0xf << WDOG_MODE_INTV_SHIFT)

static const uint16_t g_timeout_ms[] =
{
  500, 1000, 2000, 3000, 4000, 5000, 6000,
  8000, 10000, 12000, 14000, 16000
};

#define NTIMEOUTS (sizeof(g_timeout_ms) / sizeof(g_timeout_ms[0]))

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct t113_wdt_lowerhalf_s
{
  const struct watchdog_ops_s *ops;
  uint32_t timeout_ms;
  bool     started;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int t113_wdt_start(struct watchdog_lowerhalf_s *lower);
static int t113_wdt_stop(struct watchdog_lowerhalf_s *lower);
static int t113_wdt_keepalive(struct watchdog_lowerhalf_s *lower);
static int t113_wdt_getstatus(struct watchdog_lowerhalf_s *lower,
                              struct watchdog_status_s *status);
static int t113_wdt_settimeout(struct watchdog_lowerhalf_s *lower,
                               uint32_t timeout);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct watchdog_ops_s g_wdtops =
{
  .start      = t113_wdt_start,
  .stop       = t113_wdt_stop,
  .keepalive  = t113_wdt_keepalive,
  .getstatus  = t113_wdt_getstatus,
  .settimeout = t113_wdt_settimeout,
  .capture    = NULL,
  .ioctl      = NULL,
};

static struct t113_wdt_lowerhalf_s g_wdt =
{
  .ops        = &g_wdtops,
  .timeout_ms = 6000,
  .started    = false,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int t113_wdt_intv_index(uint32_t timeout_ms)
{
  int i;
  for (i = (int)NTIMEOUTS - 1; i >= 0; i--)
    {
      if (g_timeout_ms[i] <= timeout_ms)
        {
          return i;
        }
    }

  return 0;
}

static int t113_wdt_start(struct watchdog_lowerhalf_s *lower)
{
  struct t113_wdt_lowerhalf_s *priv =
    (struct t113_wdt_lowerhalf_s *)lower;
  int idx = t113_wdt_intv_index(priv->timeout_ms);

  putreg32(WDOG_CFG_RESET, WDOG_CFG_REG);
  putreg32((idx << WDOG_MODE_INTV_SHIFT) | WDOG_MODE_EN, WDOG_MODE_REG);
  putreg32(WDOG_CTRL_RESTART, WDOG_CTRL_REG);

  priv->started = true;
  return OK;
}

static int t113_wdt_stop(struct watchdog_lowerhalf_s *lower)
{
  struct t113_wdt_lowerhalf_s *priv =
    (struct t113_wdt_lowerhalf_s *)lower;

  putreg32(0, WDOG_MODE_REG);
  priv->started = false;
  return OK;
}

static int t113_wdt_keepalive(struct watchdog_lowerhalf_s *lower)
{
  putreg32(WDOG_CTRL_RESTART, WDOG_CTRL_REG);
  return OK;
}

static int t113_wdt_getstatus(struct watchdog_lowerhalf_s *lower,
                              struct watchdog_status_s *status)
{
  struct t113_wdt_lowerhalf_s *priv =
    (struct t113_wdt_lowerhalf_s *)lower;

  status->flags = priv->started ? WDFLAGS_ACTIVE : 0;
  status->timeout = priv->timeout_ms;
  status->timeleft = 0;
  return OK;
}

static int t113_wdt_settimeout(struct watchdog_lowerhalf_s *lower,
                               uint32_t timeout)
{
  struct t113_wdt_lowerhalf_s *priv =
    (struct t113_wdt_lowerhalf_s *)lower;
  int idx;

  if (timeout > 16000)
    {
      return -ERANGE;
    }

  idx = t113_wdt_intv_index(timeout);
  priv->timeout_ms = g_timeout_ms[idx];

  if (priv->started)
    {
      t113_wdt_start(lower);
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void t113_wdt_initialize(void)
{
  watchdog_register("/dev/watchdog0",
                    (struct watchdog_lowerhalf_s *)&g_wdt);
}

