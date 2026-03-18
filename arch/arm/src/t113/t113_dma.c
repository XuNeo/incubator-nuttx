/****************************************************************************
 * arch/arm/src/t113/t113_dma.c
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
#include <debug.h>
#include <errno.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/semaphore.h>
#include <arch/irq.h>

#include "arm_internal.h"
#include "hardware/t113_dma.h"
#include "t113_dma.h"

#ifdef CONFIG_T113_DMA

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define DMA_STATUS_ERROR    0x01
#define DMA_STATUS_COMPLETE 0x02

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* DMA hardware descriptor (must be 4-byte aligned, allocated from SRAM) */

struct t113_dma_desc_s
{
  uint32_t config;
  uint32_t src;
  uint32_t dst;
  uint32_t len;
  uint32_t param;
  uint32_t link;
};

/* Per-channel state */

struct t113_dma_chan_s
{
  bool              inuse;
  dma_callback_t    callback;
  void             *arg;
  struct t113_dma_desc_s desc;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct t113_dma_chan_s g_dmachan[T113_DMA_NCHANNELS];
static mutex_t g_dmalock = NXMUTEX_INITIALIZER;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void dma_chan_irq(int irq, uint32_t pend_bits, int chan_base)
{
  struct t113_dma_chan_s *chan;
  uint32_t bits;
  int n;

  for (n = 0; n < 8; n++)
    {
      bits = (pend_bits >> (n * 4)) & 0xf;
      if (bits == 0)
        {
          continue;
        }

      chan = &g_dmachan[chan_base + n];
      if (chan->callback != NULL)
        {
          chan->callback((DMA_HANDLE)chan, DMA_STATUS_COMPLETE, chan->arg);
        }
    }
}

static int t113_dma_irq0(int irq, void *context, void *arg)
{
  uint32_t pend = getreg32(T113_DMAC_IRQ_PEND0);
  putreg32(pend, T113_DMAC_IRQ_PEND0);
  dma_chan_irq(irq, pend, 0);
  return OK;
}

static int t113_dma_irq1(int irq, void *context, void *arg)
{
  uint32_t pend = getreg32(T113_DMAC_IRQ_PEND1);
  putreg32(pend, T113_DMAC_IRQ_PEND1);
  dma_chan_irq(irq, pend, 8);
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void arm_dma_initialize(void)
{
  memset(g_dmachan, 0, sizeof(g_dmachan));

  /* DMA bus clock gate: deassert reset (bit 16), enable clock (bit 0) */

  putreg32(getreg32(T113_CCU_DMA_BGR) | (1 << 16), T113_CCU_DMA_BGR);
  up_udelay(20);
  putreg32(getreg32(T113_CCU_DMA_BGR) | (1 << 0), T113_CCU_DMA_BGR);

  /* MBUS master clock gating: use fixed value matching boot0 set_mbus() */

  putreg32(0x00000d87, T113_CCU_MBUS_MAT);

  /* Enable DMA auto-gating */

  putreg32(0x7, T113_DMAC_AUTO_GATE);

  /* Clear and disable all pending interrupts */

  putreg32(0xffffffff, T113_DMAC_IRQ_PEND0);
  putreg32(0xffffffff, T113_DMAC_IRQ_PEND1);
  putreg32(0, T113_DMAC_IRQ_EN0);
  putreg32(0, T113_DMAC_IRQ_EN1);

  irq_attach(T113_IRQ_DMA0, t113_dma_irq0, NULL);
  irq_attach(T113_IRQ_DMA1, t113_dma_irq1, NULL);
  up_enable_irq(T113_IRQ_DMA0);
  up_enable_irq(T113_IRQ_DMA1);
}

DMA_HANDLE t113_dmachannel(void)
{
  struct t113_dma_chan_s *chan = NULL;
  int n;

  nxmutex_lock(&g_dmalock);

  for (n = 0; n < T113_DMA_NCHANNELS; n++)
    {
      if (!g_dmachan[n].inuse)
        {
          chan = &g_dmachan[n];
          chan->inuse    = true;
          chan->callback = NULL;
          chan->arg      = NULL;
          break;
        }
    }

  nxmutex_unlock(&g_dmalock);
  return (DMA_HANDLE)chan;
}

void t113_dmafree(DMA_HANDLE handle)
{
  struct t113_dma_chan_s *chan = (struct t113_dma_chan_s *)handle;

  DEBUGASSERT(chan != NULL && chan->inuse);

  t113_dmastop(handle);

  nxmutex_lock(&g_dmalock);
  chan->inuse = false;
  nxmutex_unlock(&g_dmalock);
}

int t113_dmasetup(DMA_HANDLE handle,
                  uintptr_t src, uintptr_t dst, size_t len,
                  const struct t113_dma_config_s *cfg)
{
  struct t113_dma_chan_s *chan = (struct t113_dma_chan_s *)handle;
  uint32_t config;

  DEBUGASSERT(chan != NULL && chan->inuse && cfg != NULL);
  DEBUGASSERT(len > 0 && len <= 0x1ffffff);

  config  = (cfg->src_drq  << DMAC_CFG_SRC_DRQ_SHIFT);
  config |= (cfg->src_width << DMAC_CFG_SRC_WIDTH_SHIFT);
  config |= (cfg->src_burst << DMAC_CFG_SRC_BURST_SHIFT);
  config |= (cfg->src_linear ? DMAC_CFG_SRC_ADDR_LINEAR :
                                DMAC_CFG_SRC_ADDR_IO);
  config |= (cfg->dst_drq  << DMAC_CFG_DST_DRQ_SHIFT);
  config |= (cfg->dst_width << DMAC_CFG_DST_WIDTH_SHIFT);
  config |= (cfg->dst_burst << DMAC_CFG_DST_BURST_SHIFT);
  config |= (cfg->dst_linear ? DMAC_CFG_DST_ADDR_LINEAR :
                                DMAC_CFG_DST_ADDR_IO);

  chan->desc.config = config;
  chan->desc.src    = (uint32_t)src;
  chan->desc.dst    = (uint32_t)dst;
  chan->desc.len    = (uint32_t)len;
  chan->desc.param  = DMAC_PARA_NORMAL_WAIT;
  chan->desc.link   = DMAC_DESC_END;

  return OK;
}

int t113_dmastart(DMA_HANDLE handle,
                  dma_callback_t callback, void *arg)
{
  struct t113_dma_chan_s *chan = (struct t113_dma_chan_s *)handle;
  int n;
  uint32_t irq_en;
  irqstate_t flags;

  DEBUGASSERT(chan != NULL && chan->inuse);

  n = (int)(chan - g_dmachan);
  DEBUGASSERT(n >= 0 && n < T113_DMA_NCHANNELS);

  chan->callback = callback;
  chan->arg      = arg;

  /* Flush descriptor to memory before DMA reads it */

  up_flush_dcache((uintptr_t)&chan->desc,
                  (uintptr_t)&chan->desc + sizeof(chan->desc));

  flags = enter_critical_section();

  /* Enable pkg-done interrupt for this channel */

  if (n < 8)
    {
      irq_en = getreg32(T113_DMAC_IRQ_EN0);
      irq_en |= DMAC_IRQ_PKGDONE(n);
      putreg32(irq_en, T113_DMAC_IRQ_EN0);
    }
  else
    {
      irq_en = getreg32(T113_DMAC_IRQ_EN1);
      irq_en |= DMAC_IRQ_PKGDONE(n - 8);
      putreg32(irq_en, T113_DMAC_IRQ_EN1);
    }

  putreg32((uint32_t)(uintptr_t)&chan->desc, T113_DMAC_DESC(n));
  putreg32(DMAC_CHAN_ENABLE, T113_DMAC_EN(n));

  leave_critical_section(flags);

  return OK;
}

void t113_dmastop(DMA_HANDLE handle)
{
  struct t113_dma_chan_s *chan = (struct t113_dma_chan_s *)handle;
  int n;
  uint32_t irq_en;
  irqstate_t flags;

  DEBUGASSERT(chan != NULL);

  n = (int)(chan - g_dmachan);
  DEBUGASSERT(n >= 0 && n < T113_DMA_NCHANNELS);

  flags = enter_critical_section();

  putreg32(0, T113_DMAC_EN(n));

  if (n < 8)
    {
      irq_en = getreg32(T113_DMAC_IRQ_EN0);
      irq_en &= ~DMAC_IRQ_PKGDONE(n);
      putreg32(irq_en, T113_DMAC_IRQ_EN0);
    }
  else
    {
      irq_en = getreg32(T113_DMAC_IRQ_EN1);
      irq_en &= ~DMAC_IRQ_PKGDONE(n - 8);
      putreg32(irq_en, T113_DMAC_IRQ_EN1);
    }

  leave_critical_section(flags);
}

size_t t113_dmaresidual(DMA_HANDLE handle)
{
  struct t113_dma_chan_s *chan = (struct t113_dma_chan_s *)handle;
  int n = (int)(chan - g_dmachan);

  return (size_t)getreg32(T113_DMAC_CNT(n));
}

#endif /* CONFIG_T113_DMA */
