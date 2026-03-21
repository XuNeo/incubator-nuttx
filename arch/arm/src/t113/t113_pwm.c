/****************************************************************************
 * arch/arm/src/t113/t113_pwm.c
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
#include <nuttx/timers/pwm.h>

#include "arm_internal.h"
#include "hardware/t113_pwm.h"
#include "hardware/t113_ccu.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define T113_PWM_CLK_HZ   24000000  /* HOSC 24 MHz */

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct t113_pwm_s
{
  const struct pwm_ops_s *ops;
  uint8_t channel;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int t113_pwm_setup(FAR struct pwm_lowerhalf_s *dev);
static int t113_pwm_shutdown(FAR struct pwm_lowerhalf_s *dev);
static int t113_pwm_start(FAR struct pwm_lowerhalf_s *dev,
                          FAR const struct pwm_info_s *info);
static int t113_pwm_stop(FAR struct pwm_lowerhalf_s *dev);
static int t113_pwm_ioctl(FAR struct pwm_lowerhalf_s *dev,
                          int cmd, unsigned long arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct pwm_ops_s g_pwm_ops =
{
  .setup    = t113_pwm_setup,
  .shutdown = t113_pwm_shutdown,
  .start    = t113_pwm_start,
  .stop     = t113_pwm_stop,
  .ioctl    = t113_pwm_ioctl,
};

static struct t113_pwm_s g_pwm0 =
{
  .ops     = &g_pwm_ops,
  .channel = 0,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int t113_pwm_setup(FAR struct pwm_lowerhalf_s *dev)
{
  uint32_t val;

  /* Deassert PWM reset and enable clock gate via CCU */

  val = getreg32(T113_CCU_PWM_BGR);
  val |= (1 << 16) | (1 << 0);
  putreg32(val, T113_CCU_PWM_BGR);

  return OK;
}

static int t113_pwm_shutdown(FAR struct pwm_lowerhalf_s *dev)
{
  struct t113_pwm_s *priv = (struct t113_pwm_s *)dev;
  uint8_t ch = priv->channel;

  /* Disable channel */

  putreg32(getreg32(T113_PWM_PER) & ~(1 << ch), T113_PWM_PER);
  putreg32(getreg32(T113_PWM_PCGR) & ~(1 << ch), T113_PWM_PCGR);

  return OK;
}

static int t113_pwm_start(FAR struct pwm_lowerhalf_s *dev,
                          FAR const struct pwm_info_s *info)
{
  struct t113_pwm_s *priv = (struct t113_pwm_s *)dev;
  uint8_t ch = priv->channel;
  uint32_t period;
  uint32_t active;
  uint32_t prescaler;
  uint32_t ppr;

  if (info->frequency == 0)
    {
      return -EINVAL;
    }

  /* Calculate prescaler and period.
   * PWM freq = CLK_HZ / (prescaler + 1) / (period + 1)
   * Start with prescaler=0, increase if period > 65535.
   */

  prescaler = 0;
  period = (T113_PWM_CLK_HZ / info->frequency) - 1;

  while (period > 65535 && prescaler < 255)
    {
      prescaler++;
      period = (T113_PWM_CLK_HZ / ((prescaler + 1) * info->frequency)) - 1;
    }

  if (period > 65535)
    {
      return -EINVAL;
    }

  /* duty is ub16 (0x0000-0xFFFF maps to 0-100%).
   * active = period * duty / 65536
   */

  active = (uint32_t)((uint64_t)(period + 1) * info->duty / 65536);
  if (active > period + 1)
    {
      active = period + 1;
    }

  /* Set prescaler in PCR */

  putreg32((prescaler & 0xff) | T113_PWM_PCR_ACT_STA, T113_PWM_PCR(ch));

  /* Set period and active in PPR */

  ppr = ((period & 0xffff) << T113_PWM_PPR_PERIOD_SHIFT) |
        ((active & 0xffff) << T113_PWM_PPR_ACTIVE_SHIFT);
  putreg32(ppr, T113_PWM_PPR(ch));

  /* Enable clock gate and channel */

  putreg32(getreg32(T113_PWM_PCGR) | (1 << ch), T113_PWM_PCGR);
  putreg32(getreg32(T113_PWM_PER) | (1 << ch), T113_PWM_PER);

  return OK;
}

static int t113_pwm_stop(FAR struct pwm_lowerhalf_s *dev)
{
  struct t113_pwm_s *priv = (struct t113_pwm_s *)dev;
  uint8_t ch = priv->channel;

  putreg32(getreg32(T113_PWM_PER) & ~(1 << ch), T113_PWM_PER);
  return OK;
}

static int t113_pwm_ioctl(FAR struct pwm_lowerhalf_s *dev,
                          int cmd, unsigned long arg)
{
  return -ENOTTY;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void t113_pwm_initialize(int channel)
{
  char devpath[16];

  g_pwm0.channel = channel;
  snprintf(devpath, sizeof(devpath), "/dev/pwm%d", channel);
  pwm_register(devpath, (FAR struct pwm_lowerhalf_s *)&g_pwm0);
}
