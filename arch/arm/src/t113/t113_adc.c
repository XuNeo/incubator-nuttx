/****************************************************************************
 * arch/arm/src/t113/t113_adc.c
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
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/analog/adc.h>
#include <nuttx/analog/ioctl.h>

#include "arm_internal.h"
#include "hardware/t113_adc.h"
#include "hardware/t113_ccu.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct t113_adc_s
{
  const struct adc_ops_s *ops;
  uint8_t chanmask;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  t113_adc_bind(FAR struct adc_dev_s *dev,
                          FAR const struct adc_callback_s *callback);
static void t113_adc_reset(FAR struct adc_dev_s *dev);
static int  t113_adc_setup(FAR struct adc_dev_s *dev);
static void t113_adc_shutdown(FAR struct adc_dev_s *dev);
static void t113_adc_rxint(FAR struct adc_dev_s *dev, bool enable);
static int  t113_adc_ioctl(FAR struct adc_dev_s *dev, int cmd,
                           unsigned long arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct adc_ops_s g_adc_ops =
{
  .ao_bind     = t113_adc_bind,
  .ao_reset    = t113_adc_reset,
  .ao_setup    = t113_adc_setup,
  .ao_shutdown = t113_adc_shutdown,
  .ao_rxint    = t113_adc_rxint,
  .ao_ioctl    = t113_adc_ioctl,
};

static struct t113_adc_s g_adc_priv =
{
  .ops      = &g_adc_ops,
  .chanmask = 0x01,
};

static struct adc_dev_s g_adc_dev =
{
  .ad_ops  = &g_adc_ops,
  .ad_priv = &g_adc_priv,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int t113_adc_bind(FAR struct adc_dev_s *dev,
                         FAR const struct adc_callback_s *callback)
{
  return OK;
}

static void t113_adc_reset(FAR struct adc_dev_s *dev)
{
  uint32_t val;

  /* Deassert GPADC reset and enable clock via CCU */

  val = getreg32(T113_CCU_GPADC_BGR);
  val |= (1 << 16) | (1 << 0);
  putreg32(val, T113_CCU_GPADC_BGR);

  /* Enable ADC LDO and ADC function */

  val = T113_GPADC_CTRL_LDO_EN | T113_GPADC_CTRL_ADC_EN |
        T113_GPADC_CTRL_CALI_EN;
  putreg32(val, T113_GPADC_CTRL);
}

static int t113_adc_setup(FAR struct adc_dev_s *dev)
{
  struct t113_adc_s *priv = (struct t113_adc_s *)dev->ad_priv;

  t113_adc_reset(dev);

  /* Select channels */

  putreg32(priv->chanmask, T113_GPADC_CS_EN);

  return OK;
}

static void t113_adc_shutdown(FAR struct adc_dev_s *dev)
{
  putreg32(0, T113_GPADC_CTRL);
}

static void t113_adc_rxint(FAR struct adc_dev_s *dev, bool enable)
{
}

static int t113_adc_ioctl(FAR struct adc_dev_s *dev, int cmd,
                          unsigned long arg)
{
  return -ENOTTY;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void t113_adc_initialize(void)
{
  adc_register("/dev/adc0", &g_adc_dev);
}
