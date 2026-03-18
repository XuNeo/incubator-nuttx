/****************************************************************************
 * arch/arm/src/t113/t113_spi.c
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
#include <nuttx/clock.h>
#include <nuttx/mutex.h>
#include <nuttx/semaphore.h>
#include <nuttx/spi/spi.h>

#include "arm_internal.h"
#include "hardware/t113_spi.h"
#include "hardware/t113_dma.h"
#include "t113_dma.h"
#include "t113_spi.h"
#include "chip.h"

#if defined(CONFIG_T113_SPI0) || defined(CONFIG_T113_SPI1)

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define SPI_DMA_THRESHOLD   8    /* Use DMA for transfers larger than this */

#define SPI_INPUT_CLK_HZ    24000000   /* 24 MHz APB clock */

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct t113_spidev_s
{
  struct spi_dev_s  spidev;     /* Externally visible SPI interface */
  uint32_t          base;       /* SPI controller base address */
  mutex_t           lock;       /* Mutual exclusion mutex */
  uint32_t          frequency;  /* Requested clock frequency */
  uint32_t          actual;     /* Actual clock frequency */
  uint8_t           nbits;      /* Width of word in bits (8 or 16) */
  uint8_t           mode;       /* Mode 0,1,2,3 */

#ifdef CONFIG_T113_SPI0_DMA
  DMA_HANDLE        rxdma;      /* RX DMA channel */
  DMA_HANDLE        txdma;      /* TX DMA channel */
  sem_t             rxsem;      /* RX DMA completion */
  sem_t             txsem;      /* TX DMA completion */
  volatile uint8_t  rxresult;
  volatile uint8_t  txresult;
  uint8_t           src_drq;    /* DMA DRQ for this SPI (RX) */
  uint8_t           dst_drq;    /* DMA DRQ for this SPI (TX) */
#endif
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int      spi_lock(FAR struct spi_dev_s *dev, bool lock);
static uint32_t spi_setfrequency(FAR struct spi_dev_s *dev,
                                 uint32_t frequency);
static void     spi_setmode(FAR struct spi_dev_s *dev,
                             enum spi_mode_e mode);
static void     spi_setbits(FAR struct spi_dev_s *dev, int nbits);
static uint32_t spi_send(FAR struct spi_dev_s *dev, uint32_t wd);
static void     spi_exchange(FAR struct spi_dev_s *dev,
                              FAR const void *txbuffer,
                              FAR void *rxbuffer, size_t nwords);
#ifndef CONFIG_SPI_EXCHANGE
static void spi_sndblock(FAR struct spi_dev_s *dev, FAR const void *buffer,
                         size_t nwords);
static void spi_recvblock(FAR struct spi_dev_s *dev, FAR void *buffer,
                          size_t nwords);
#endif

/****************************************************************************
 * Private Data
  ****************************************************************************/

static inline void spi_putreg(struct t113_spidev_s *priv,
                               uint32_t offset, uint32_t val)
{
  putreg32(val, priv->base + offset);
}

static inline uint32_t spi_getreg(struct t113_spidev_s *priv,
                                    uint32_t offset)
{
  return getreg32(priv->base + offset);
}

static void spi_select(FAR struct spi_dev_s *dev, uint32_t devid,
                        bool selected)
{
  FAR struct t113_spidev_s *priv = (FAR struct t113_spidev_s *)dev;
  uint32_t tcr;

  tcr = spi_getreg(priv, SPI_TCR_REG);
  tcr &= ~((0x3 << 4) | SPI_TCR_SS_LEVEL);
  tcr |= ((0 & 0x3) << 4);
  if (!selected)
    {
      tcr |= SPI_TCR_SS_LEVEL;
    }
  else
    {
      spi_putreg(priv, SPI_FCR_REG,
                 SPI_FCR_TX_FIFO_RST | SPI_FCR_RF_RST);
    }

  spi_putreg(priv, SPI_TCR_REG, tcr);
}

static const struct spi_ops_s g_spiops =
{
  .lock              = spi_lock,
  .select            = spi_select,
  .setfrequency      = spi_setfrequency,
  .setmode           = spi_setmode,
  .setbits           = spi_setbits,
  .status            = NULL,
#ifdef CONFIG_SPI_CMDDATA
  .cmddata           = NULL,
#endif
  .send              = spi_send,
#ifdef CONFIG_SPI_EXCHANGE
  .exchange          = spi_exchange,
#else
  .sndblock          = spi_sndblock,
  .recvblock         = spi_recvblock,
#endif
};

#ifdef CONFIG_T113_SPI0
static struct t113_spidev_s g_spi0dev =
{
  .spidev   = { .ops = &g_spiops },
  .base     = T113_SPI0_BASE,
  .lock     = NXMUTEX_INITIALIZER,
  .frequency= 0,
  .nbits    = 8,
  .mode     = SPIDEV_MODE0,
#ifdef CONFIG_T113_SPI0_DMA
  .rxsem    = SEM_INITIALIZER(0),
  .txsem    = SEM_INITIALIZER(0),
  .src_drq  = DRQ_SPI0_RX,
  .dst_drq  = DRQ_SPI0_TX,
#endif
};
#endif

#ifdef CONFIG_T113_SPI1
static struct t113_spidev_s g_spi1dev =
{
  .spidev   = { .ops = &g_spiops },
  .base     = T113_SPI1_BASE,
  .lock     = NXMUTEX_INITIALIZER,
  .frequency= 0,
  .nbits    = 8,
  .mode     = SPIDEV_MODE0,
#ifdef CONFIG_T113_SPI1_DMA
  .rxsem    = SEM_INITIALIZER(0),
  .txsem    = SEM_INITIALIZER(0),
  .src_drq  = DRQ_SPI1_RX,
  .dst_drq  = DRQ_SPI1_TX,
#endif
};
#endif

/****************************************************************************
  * Private Functions
  ****************************************************************************/


static void spi_reset(struct t113_spidev_s *priv)
{
  uint32_t reg;

  reg = spi_getreg(priv, SPI_GCR_REG);
  reg |= SPI_GCR_SRST | SPI_GCR_TP_EN | SPI_GCR_MASTER | SPI_GCR_EN;
  spi_putreg(priv, SPI_GCR_REG, reg);
  while (spi_getreg(priv, SPI_GCR_REG) & SPI_GCR_SRST);

  reg = spi_getreg(priv, SPI_TCR_REG);
  reg &= ~0x3;
  reg |= SPI_TCR_SS_OWNER | SPI_TCR_SPOL;
  spi_putreg(priv, SPI_TCR_REG, reg);

  reg = spi_getreg(priv, SPI_FCR_REG);
  reg |= SPI_FCR_TX_FIFO_RST | SPI_FCR_RF_RST;
  spi_putreg(priv, SPI_FCR_REG, reg);

  spi_putreg(priv, SPI_IER_REG, 0);
  spi_putreg(priv, SPI_ISR_REG, 0xffffffff);
}

static void spi_clock_enable(struct t113_spidev_s *priv)
{
  uint32_t reg;
  int i;
  int bit = (priv->base == T113_SPI0_BASE) ? 0 : 1;
  uint32_t clk_reg = (priv->base == T113_SPI0_BASE) ?
                     T113_CCU_SPI0_CLK_REG : T113_CCU_SPI1_CLK_REG;

  /* 1. Deassert SPI reset (SPI_BGR_REG bit16/17) */

  reg = getreg32(T113_CCU_SPI_BGR_REG);
  reg |= (1 << (16 + bit));
  putreg32(reg, T113_CCU_SPI_BGR_REG);

  /* 2. Open SPI_CLK gate (SPI_CLK_REG bit31) */

  reg = getreg32(clk_reg);
  reg |= (1 << 31);
  putreg32(reg, clk_reg);

  /* 3. Open SPI bus gate (SPI_BGR_REG bit0/1) */

  reg = getreg32(T113_CCU_SPI_BGR_REG);
  reg |= (1 << bit);
  putreg32(reg, T113_CCU_SPI_BGR_REG);

  /* 4. Select PLL_PERI(1X) as clock source (bits26:24 = 001) */

  reg = getreg32(clk_reg);
  reg &= ~(0x7 << 24);
  reg |=  (0x1 << 24);
  putreg32(reg, clk_reg);

  /* 5. Pre-divide by 1 (bits9:8 = 0) */

  reg = getreg32(clk_reg);
  reg &= ~(0x3 << 8);
  putreg32(reg, clk_reg);

  /* 6. Divide by 6 (bits3:0 = 5) => ~100MHz / 6 = ~100MHz */

  reg = getreg32(clk_reg);
  reg &= ~(0xf << 0);
  reg |=  (6 - 1) << 0;
  putreg32(reg, clk_reg);

  /* 7. SPI_CCR: further divide by 2 */

  putreg32(0x1000, priv->base + SPI_CCR_REG);

  /* 8. SPI0 pinmux: PC2=CLK(2), PC3=CS0(2), PC4=MOSI(2), PC5=MISO(2)
   * PC_CFG0 = GPIO_BASE + 2*0x30 = 0x02000060
   */

  if (priv->base == T113_SPI0_BASE)
    {
      reg = getreg32(0x02000060);
      reg &= ~(0xffffu << 8);
      reg |=  (0x2222u << 8);
      putreg32(reg, 0x02000060);
    }

  for (i = 0; i < 1000; i++);
}

static int spi_lock(FAR struct spi_dev_s *dev, bool lock)
{
  FAR struct t113_spidev_s *priv = (FAR struct t113_spidev_s *)dev;

  if (lock)
    {
      return nxmutex_lock(&priv->lock);
    }

  return nxmutex_unlock(&priv->lock);
}

static uint32_t spi_setfrequency(FAR struct spi_dev_s *dev,
                                   uint32_t frequency)
{
  FAR struct t113_spidev_s *priv = (FAR struct t113_spidev_s *)dev;

  if (frequency == priv->frequency)
    {
      return priv->actual;
    }

  priv->frequency = frequency;
  priv->actual    = frequency;

  spiinfo("frequency=%lu\n", frequency);
  return frequency;
}

static void spi_setmode(FAR struct spi_dev_s *dev, enum spi_mode_e mode)
{
  FAR struct t113_spidev_s *priv = (FAR struct t113_spidev_s *)dev;
  uint32_t tcr;

  if (mode == priv->mode)
    {
      return;
    }

  tcr = spi_getreg(priv, SPI_TCR_REG);
  tcr &= ~(SPI_TCR_CPOL | SPI_TCR_CPHA);

  switch (mode)
    {
      case SPIDEV_MODE0: break;
      case SPIDEV_MODE1: tcr |= SPI_TCR_CPHA; break;
      case SPIDEV_MODE2: tcr |= SPI_TCR_CPOL; break;
      case SPIDEV_MODE3: tcr |= SPI_TCR_CPOL | SPI_TCR_CPHA; break;
      default: return;
    }

  spi_putreg(priv, SPI_TCR_REG, tcr);
  priv->mode = mode;
}

static void spi_setbits(FAR struct spi_dev_s *dev, int nbits)
{
  FAR struct t113_spidev_s *priv = (FAR struct t113_spidev_s *)dev;
  priv->nbits = nbits;
}

static void spi_exchange_pio(FAR struct t113_spidev_s *priv,
                              FAR const void *txbuf,
                              FAR void *rxbuf, size_t nwords)
{
  const uint8_t *tx = (const uint8_t *)txbuf;
  uint8_t       *rx = (uint8_t *)rxbuf;
  size_t         pos = 0;
  size_t         n;
  size_t         i;

  while (pos < nwords)
    {
      n = nwords - pos;
      if (n > 64)
        {
          n = 64;
        }

      spi_putreg(priv, SPI_MBC_REG, n);
      spi_putreg(priv, SPI_MTC_REG, n);
      spi_putreg(priv, SPI_BCC_REG, n);

      for (i = 0; i < n; i++)
        {
          putreg8(tx ? tx[pos + i] : 0xff, priv->base + SPI_TXD_REG);
        }

      spi_putreg(priv, SPI_TCR_REG,
                 spi_getreg(priv, SPI_TCR_REG) | SPI_TCR_XCH);

      {
        int timeout = 1000000;
        while ((spi_getreg(priv, SPI_TCR_REG) & SPI_TCR_XCH) &&
               --timeout > 0);
        if (timeout <= 0)
          {
            spierr("SPI XCH timeout pos=%lu n=%lu\n",
                   (unsigned long)pos, (unsigned long)n);
            return;
          }
      }

      {
        int timeout = 1000000;
        while (((spi_getreg(priv, SPI_FSR_REG) & 0xff) < n) &&
               --timeout > 0);
        if (timeout <= 0)
          {
            spierr("SPI RX timeout pos=%lu n=%lu\n",
                   (unsigned long)pos, (unsigned long)n);
            return;
          }
      }

      for (i = 0; i < n; i++)
        {
          uint8_t val = getreg8(priv->base + SPI_RXD_REG);
          if (rx)
            {
              rx[pos + i] = val;
            }
        }

      pos += n;
    }
}


#if defined(CONFIG_T113_SPI0_DMA) || defined(CONFIG_T113_SPI1_DMA)
static void spi_rxcallback(DMA_HANDLE handle, uint8_t status, void *arg)
{
  FAR struct t113_spidev_s *priv = (FAR struct t113_spidev_s *)arg;
  priv->rxresult = status | 0x80;
  nxsem_post(&priv->rxsem);
}

static void spi_txcallback(DMA_HANDLE handle, uint8_t status, void *arg)
{
  FAR struct t113_spidev_s *priv = (FAR struct t113_spidev_s *)arg;
  priv->txresult = status | 0x80;
  nxsem_post(&priv->txsem);
}

static void spi_exchange_dma(FAR struct t113_spidev_s *priv,
                              FAR const void *txbuf,
                              FAR void *rxbuf, size_t nwords)
{
  struct t113_dma_config_s rxcfg;
  struct t113_dma_config_s txcfg;
  uint32_t dummy_tx = 0xffffffff;
  uint32_t dummy_rx;

  rxcfg.src_drq    = priv->src_drq;
  rxcfg.dst_drq    = DRQ_DRAM;
  rxcfg.src_width  = DMAC_WIDTH_8BIT;
  rxcfg.dst_width  = DMAC_WIDTH_8BIT;
  rxcfg.src_burst  = DMAC_BURST_1;
  rxcfg.dst_burst  = DMAC_BURST_1;
  rxcfg.src_linear = false;   /* SPI FIFO = IO */
  rxcfg.dst_linear = (rxbuf != NULL);
  rxcfg.wait_cyc   = 0;

  txcfg.src_drq    = DRQ_DRAM;
  txcfg.dst_drq    = priv->dst_drq;
  txcfg.src_width  = DMAC_WIDTH_8BIT;
  txcfg.dst_width  = DMAC_WIDTH_8BIT;
  txcfg.src_burst  = DMAC_BURST_1;
  txcfg.dst_burst  = DMAC_BURST_1;
  txcfg.src_linear = (txbuf != NULL);  /* dummy_tx if no txbuf */
  txcfg.dst_linear = false;   /* SPI FIFO = IO */
  txcfg.wait_cyc   = 0;

  priv->rxresult = 0;
  priv->txresult = 0;

  /* Flush cache before DMA reads */

  if (txbuf)
    {
      up_flush_dcache((uintptr_t)txbuf, (uintptr_t)txbuf + nwords);
    }

  if (rxbuf)
    {
      up_invalidate_dcache((uintptr_t)rxbuf, (uintptr_t)rxbuf + nwords);
    }

  t113_dmasetup(priv->rxdma,
                priv->base + SPI_RXD_REG,
                rxbuf ? (uintptr_t)rxbuf : (uintptr_t)&dummy_rx,
                nwords, &rxcfg);

  t113_dmasetup(priv->txdma,
                txbuf ? (uintptr_t)txbuf : (uintptr_t)&dummy_tx,
                priv->base + SPI_TXD_REG,
                nwords, &txcfg);

  /* Enable DMA requests in SPI controller */

  spi_putreg(priv, SPI_FCR_REG,
             SPI_FCR_RF_DRQ_EN | SPI_FCR_TF_DRQ_EN |
             SPI_FCR_RX_TRIG(1) | SPI_FCR_TX_TRIG(0x20));

  spi_putreg(priv, SPI_MBC_REG, nwords);
  spi_putreg(priv, SPI_MTC_REG, nwords);
  spi_putreg(priv, SPI_BCC_REG, nwords);

  t113_dmastart(priv->rxdma, spi_rxcallback, priv);
  t113_dmastart(priv->txdma, spi_txcallback, priv);

  spi_putreg(priv, SPI_TCR_REG,
             spi_getreg(priv, SPI_TCR_REG) | SPI_TCR_XCH);

  /* Wait for RX DMA completion (RX done means transfer done) */

  nxsem_wait_uninterruptible(&priv->rxsem);
  nxsem_wait_uninterruptible(&priv->txsem);

  /* Disable DMA requests */

  spi_putreg(priv, SPI_FCR_REG, 0);
}
#endif

static void spi_exchange(FAR struct spi_dev_s *dev,
                          FAR const void *txbuffer,
                          FAR void *rxbuffer, size_t nwords)
{
  FAR struct t113_spidev_s *priv = (FAR struct t113_spidev_s *)dev;

#if defined(CONFIG_T113_SPI0_DMA) || defined(CONFIG_T113_SPI1_DMA)
  if (nwords > SPI_DMA_THRESHOLD && priv->rxdma != NULL)
    {
      spi_exchange_dma(priv, txbuffer, rxbuffer, nwords);
      return;
    }
#endif

  spi_exchange_pio(priv, txbuffer, rxbuffer, nwords);
}

static uint32_t spi_send(FAR struct spi_dev_s *dev, uint32_t wd)
{
  uint8_t txbyte = (uint8_t)wd;
  uint8_t rxbyte = 0;
  spi_exchange(dev, &txbyte, &rxbyte, 1);
  return rxbyte;
}

#ifndef CONFIG_SPI_EXCHANGE
static void spi_sndblock(FAR struct spi_dev_s *dev, FAR const void *buffer,
                          size_t nwords)
{
  spi_exchange(dev, buffer, NULL, nwords);
}

static void spi_recvblock(FAR struct spi_dev_s *dev, FAR void *buffer,
                           size_t nwords)
{
  spi_exchange(dev, NULL, buffer, nwords);
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

FAR struct spi_dev_s *t113_spibus_initialize(int bus)
{
  FAR struct t113_spidev_s *priv;

  switch (bus)
    {
#ifdef CONFIG_T113_SPI0
      case 0:
        priv = &g_spi0dev;
        break;
#endif
#ifdef CONFIG_T113_SPI1
      case 1:
        priv = &g_spi1dev;
        break;
#endif
      default:
        return NULL;
    }

  spi_clock_enable(priv);
  spi_reset(priv);

  /* Configure as master, 8-bit, mode 0 */

  spi_putreg(priv, SPI_GCR_REG, SPI_GCR_MASTER | SPI_GCR_EN);
  spi_putreg(priv, SPI_TCR_REG,
             SPI_TCR_SS_OWNER | SPI_TCR_SS_LEVEL | SPI_TCR_SS(0));
  spi_putreg(priv, SPI_FCR_REG,
             SPI_FCR_TX_TRIG(0x40) | SPI_FCR_RX_TRIG(1));

#if defined(CONFIG_T113_SPI0_DMA) || defined(CONFIG_T113_SPI1_DMA)
  if (priv->src_drq != 0)
    {
      priv->rxdma = t113_dmachannel();
      priv->txdma = t113_dmachannel();
    }
#endif

  return (FAR struct spi_dev_s *)priv;
}

#endif /* CONFIG_T113_SPI0 || CONFIG_T113_SPI1 */
