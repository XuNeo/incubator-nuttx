/****************************************************************************
 * arch/arm/src/t113/t113_i2c.c
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

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/mutex.h>
#include <nuttx/semaphore.h>
#include <nuttx/i2c/i2c_master.h>

#include "arm_internal.h"
#include "hardware/t113_i2c.h"
#include "t113_i2c.h"
#include "chip.h"

#if defined(CONFIG_T113_TWI0) || defined(CONFIG_T113_TWI1) || \
    defined(CONFIG_T113_TWI2) || defined(CONFIG_T113_TWI3)

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define TWI_TIMEOUT_MS  100

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* I2C transfer state machine states */

enum twi_state_e
{
  TWI_IDLE = 0,
  TWI_START,
  TWI_ADDR,
  TWI_DATA_TX,
  TWI_DATA_RX,
  TWI_STOP,
  TWI_ERROR,
};

struct t113_i2cdev_s
{
  struct i2c_master_s    i2cdev;   /* Externally visible I2C interface */
  uint32_t               base;     /* TWI controller base address */
  int                    irq;      /* TWI interrupt number */
  mutex_t                lock;     /* Bus-level mutex */
  sem_t                  sem;      /* Transfer completion semaphore */
  FAR struct i2c_msg_s  *msgs;     /* Current message array */
  int                    nmsgs;    /* Number of messages */
  int                    msgidx;   /* Current message index */
  int                    byteidx;  /* Byte index within current message */
  int                    result;   /* Transfer result (0=OK, <0=error) */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int twi_transfer(FAR struct i2c_master_s *dev,
                         FAR struct i2c_msg_s *msgs, int count);
#ifdef CONFIG_I2C_RESET
static int twi_reset(FAR struct i2c_master_s *dev);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct i2c_ops_s g_twiops =
{
  .transfer = twi_transfer,
#ifdef CONFIG_I2C_RESET
  .reset    = twi_reset,
#endif
};

#define DEFINE_TWI(n, base_addr, irq_num)                \
  static struct t113_i2cdev_s g_twi##n##dev =            \
  {                                                       \
    .i2cdev = { .ops = &g_twiops },                      \
    .base   = base_addr,                                  \
    .irq    = irq_num,                                    \
    .lock   = NXMUTEX_INITIALIZER,                        \
    .sem    = SEM_INITIALIZER(0),                         \
  }

#ifdef CONFIG_T113_TWI0
DEFINE_TWI(0, T113_TWI0_BASE, T113_IRQ_TWI0);
#endif
#ifdef CONFIG_T113_TWI1
DEFINE_TWI(1, T113_TWI1_BASE, T113_IRQ_TWI1);
#endif
#ifdef CONFIG_T113_TWI2
DEFINE_TWI(2, T113_TWI2_BASE, T113_IRQ_TWI2);
#endif
#ifdef CONFIG_T113_TWI3
DEFINE_TWI(3, T113_TWI3_BASE, T113_IRQ_TWI3);
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static inline void twi_putreg(struct t113_i2cdev_s *priv,
                               uint32_t offset, uint32_t val)
{
  putreg32(val, priv->base + offset);
}

static inline uint32_t twi_getreg(struct t113_i2cdev_s *priv,
                                    uint32_t offset)
{
  return getreg32(priv->base + offset);
}

static void twi_reset_hw(struct t113_i2cdev_s *priv)
{
  twi_putreg(priv, TWI_SRST_REG, TWI_SRST_RESET);
  up_udelay(10);
  twi_putreg(priv, TWI_SRST_REG, 0);
}

static void twi_setclock(struct t113_i2cdev_s *priv, uint32_t freq)
{
  uint32_t n;
  uint32_t m;
  uint32_t div;
  uint32_t clk_in = 24000000;

  /* Fscl = Fin / (2^N * (M+1) * 10)
   * Try N=2, solve for M: M = (Fin / (10 * Fscl * 4)) - 1
   */

  for (n = 0; n <= 7; n++)
    {
      div = clk_in / (freq * 10 * (1 << n));
      if (div >= 1 && div <= 16)
        {
          m = div - 1;
          twi_putreg(priv, TWI_CCR_REG,
                     (m << TWI_CCR_CLK_M_SHIFT) |
                     (n << TWI_CCR_CLK_N_SHIFT));
          return;
        }
    }

  /* Fallback: use N=2, M=11 → ~50kHz */

  twi_putreg(priv, TWI_CCR_REG,
             (11 << TWI_CCR_CLK_M_SHIFT) | (2 << TWI_CCR_CLK_N_SHIFT));
}

static void twi_clock_enable(struct t113_i2cdev_s *priv)
{
  uint32_t reg;
  int i;
  int bit;

  /* TWI BGR at CCU + 0x091C: bits[3:0]=gate, bits[19:16]=reset */

  if (priv->base == T113_TWI0_BASE)
    {
      bit = 0;
    }
  else if (priv->base == T113_TWI1_BASE)
    {
      bit = 1;
    }
  else if (priv->base == T113_TWI2_BASE)
    {
      bit = 2;
    }
  else
    {
      bit = 3;
    }

  reg = getreg32(T113_CCU_TWI_BGR_REG);
  reg &= ~(1 << (16 + bit));
  putreg32(reg, T113_CCU_TWI_BGR_REG);
  for (i = 0; i < 100; i++);
  reg |= (1 << (16 + bit));
  putreg32(reg, T113_CCU_TWI_BGR_REG);

  reg &= ~(1 << bit);
  putreg32(reg, T113_CCU_TWI_BGR_REG);
  for (i = 0; i < 100; i++);
  reg |= (1 << bit);
  putreg32(reg, T113_CCU_TWI_BGR_REG);
}

static void twi_send_start(struct t113_i2cdev_s *priv)
{
  uint32_t cntr;

  cntr = TWI_CNTR_INT_EN | TWI_CNTR_BUS_EN |
         TWI_CNTR_START | TWI_CNTR_A_ACK;
  twi_putreg(priv, TWI_CNTR_REG, cntr);
}

static void twi_send_stop(struct t113_i2cdev_s *priv)
{
  uint32_t cntr;

  cntr = TWI_CNTR_INT_EN | TWI_CNTR_BUS_EN |
         TWI_CNTR_STOP | TWI_CNTR_A_ACK;
  twi_putreg(priv, TWI_CNTR_REG, cntr);
}

static void twi_clear_irq(struct t113_i2cdev_s *priv)
{
  uint32_t cntr;

  cntr = twi_getreg(priv, TWI_CNTR_REG);
  cntr &= ~TWI_CNTR_INT_FL;
  cntr |= TWI_CNTR_INT_EN | TWI_CNTR_BUS_EN | TWI_CNTR_A_ACK;
  twi_putreg(priv, TWI_CNTR_REG, cntr);
}

static int twi_irq_handler(int irq, void *context, void *arg)
{
  FAR struct t113_i2cdev_s *priv = (FAR struct t113_i2cdev_s *)arg;
  uint32_t stat;
  FAR struct i2c_msg_s *msg;
  bool done = false;

  stat = twi_getreg(priv, TWI_STAT_REG);

  switch (stat)
    {
      case TWI_STAT_START:
      case TWI_STAT_RSTART:
        msg = &priv->msgs[priv->msgidx];
        twi_putreg(priv, TWI_DATA_REG,
                   (msg->addr << 1) |
                   ((msg->flags & I2C_M_READ) ? 1 : 0));
        twi_clear_irq(priv);
        break;

      case TWI_STAT_ADDR_W_ACK:
        msg = &priv->msgs[priv->msgidx];
        priv->byteidx = 0;
        twi_putreg(priv, TWI_DATA_REG, msg->buffer[priv->byteidx++]);
        twi_clear_irq(priv);
        break;

      case TWI_STAT_DATA_T_ACK:
        msg = &priv->msgs[priv->msgidx];
        if (priv->byteidx < msg->length)
          {
            twi_putreg(priv, TWI_DATA_REG,
                       msg->buffer[priv->byteidx++]);
            twi_clear_irq(priv);
          }
        else
          {
            priv->msgidx++;
            if (priv->msgidx < priv->nmsgs)
              {
                twi_send_start(priv);
              }
            else
              {
                twi_send_stop(priv);
                done = true;
              }
          }
        break;

      case TWI_STAT_ADDR_R_ACK:
        priv->byteidx = 0;
        twi_clear_irq(priv);
        break;

      case TWI_STAT_DATA_R_ACK:
        msg = &priv->msgs[priv->msgidx];
        msg->buffer[priv->byteidx++] = (uint8_t)twi_getreg(priv,
                                                             TWI_DATA_REG);
        if (priv->byteidx < msg->length - 1)
          {
            twi_clear_irq(priv);
          }
        else
          {
            uint32_t cntr = twi_getreg(priv, TWI_CNTR_REG);
            cntr &= ~(TWI_CNTR_A_ACK | TWI_CNTR_INT_FL);
            cntr |= TWI_CNTR_INT_EN | TWI_CNTR_BUS_EN;
            twi_putreg(priv, TWI_CNTR_REG, cntr);
          }
        break;

      case TWI_STAT_DATA_R_NAK:
        msg = &priv->msgs[priv->msgidx];
        msg->buffer[priv->byteidx++] = (uint8_t)twi_getreg(priv,
                                                             TWI_DATA_REG);
        priv->msgidx++;
        if (priv->msgidx < priv->nmsgs)
          {
            twi_send_start(priv);
          }
        else
          {
            twi_send_stop(priv);
            done = true;
          }
        break;

      case TWI_STAT_ADDR_W_NAK:
      case TWI_STAT_ADDR_R_NAK:
      case TWI_STAT_DATA_T_NAK:
      case TWI_STAT_ARB_LOST:
      case TWI_STAT_BUS_ERR:
        priv->result = -EIO;
        twi_send_stop(priv);
        done = true;
        break;

      case TWI_STAT_IDLE:
        done = true;
        break;

      default:
        twi_clear_irq(priv);
        break;
    }

  if (done)
    {
      nxsem_post(&priv->sem);
    }

  return OK;
}

static int twi_transfer(FAR struct i2c_master_s *dev,
                         FAR struct i2c_msg_s *msgs, int count)
{
  FAR struct t113_i2cdev_s *priv = (FAR struct t113_i2cdev_s *)dev;
  int ret;

  DEBUGASSERT(msgs != NULL && count > 0);

  ret = nxmutex_lock(&priv->lock);
  if (ret < 0)
    {
      return ret;
    }

  priv->msgs    = msgs;
  priv->nmsgs   = count;
  priv->msgidx  = 0;
  priv->byteidx = 0;
  priv->result  = 0;

  twi_setclock(priv, msgs[0].frequency);
  twi_send_start(priv);

  ret = nxsem_tickwait_uninterruptible(&priv->sem,
            MSEC2TICK(TWI_TIMEOUT_MS));
  if (ret < 0)
    {
      twi_reset_hw(priv);
      ret = -ETIMEDOUT;
    }
  else
    {
      ret = priv->result;
    }

  nxmutex_unlock(&priv->lock);
  return ret;
}

#ifdef CONFIG_I2C_RESET
static int twi_reset(FAR struct i2c_master_s *dev)
{
  FAR struct t113_i2cdev_s *priv = (FAR struct t113_i2cdev_s *)dev;
  twi_reset_hw(priv);
  twi_putreg(priv, TWI_CNTR_REG,
             TWI_CNTR_INT_EN | TWI_CNTR_BUS_EN | TWI_CNTR_A_ACK);
  return OK;
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

FAR struct i2c_master_s *t113_i2cbus_initialize(int bus)
{
  FAR struct t113_i2cdev_s *priv;

  switch (bus)
    {
#ifdef CONFIG_T113_TWI0
      case 0: priv = &g_twi0dev; break;
#endif
#ifdef CONFIG_T113_TWI1
      case 1: priv = &g_twi1dev; break;
#endif
#ifdef CONFIG_T113_TWI2
      case 2: priv = &g_twi2dev; break;
#endif
#ifdef CONFIG_T113_TWI3
      case 3: priv = &g_twi3dev; break;
#endif
      default: return NULL;
    }

  twi_clock_enable(priv);
  twi_reset_hw(priv);
  twi_setclock(priv, 100000);   /* Default 100kHz */

  twi_putreg(priv, TWI_CNTR_REG,
             TWI_CNTR_INT_EN | TWI_CNTR_BUS_EN | TWI_CNTR_A_ACK);

  irq_attach(priv->irq, twi_irq_handler, priv);
  up_enable_irq(priv->irq);

  return (FAR struct i2c_master_s *)priv;
}

int t113_i2cbus_uninitialize(FAR struct i2c_master_s *dev)
{
  FAR struct t113_i2cdev_s *priv = (FAR struct t113_i2cdev_s *)dev;

  up_disable_irq(priv->irq);
  irq_detach(priv->irq);
  twi_putreg(priv, TWI_CNTR_REG, 0);

  return OK;
}

#endif /* CONFIG_T113_TWI* */
