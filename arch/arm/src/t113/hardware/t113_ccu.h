/****************************************************************************
 * arch/arm/src/t113/hardware/t113_ccu.h
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

#ifndef __ARCH_ARM_SRC_T113_HARDWARE_T113_CCU_H
#define __ARCH_ARM_SRC_T113_HARDWARE_T113_CCU_H

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define T113_CCU_BASE           0x02001000

/* PLL registers */

#define T113_CCU_PLL_CPUX       (T113_CCU_BASE + 0x000)
#define T113_CCU_PLL_DDR0       (T113_CCU_BASE + 0x010)
#define T113_CCU_PLL_PERIPH0    (T113_CCU_BASE + 0x020)
#define T113_CCU_PLL_VE         (T113_CCU_BASE + 0x058)
#define T113_CCU_PLL_VIDEO0     (T113_CCU_BASE + 0x040)
#define T113_CCU_PLL_VIDEO1     (T113_CCU_BASE + 0x048)
#define T113_CCU_PLL_AUDIO0     (T113_CCU_BASE + 0x078)
#define T113_CCU_PLL_AUDIO1     (T113_CCU_BASE + 0x080)

/* MBUS bandwidth control */

#define T113_CCU_MBUS_MAT       (T113_CCU_BASE + 0x804)

/* Bus Gating Reset (BGR) registers.
 * Each BGR register controls clock gating and reset for a peripheral bus.
 * bit[N+16] = reset deassert (1 = running, 0 = held in reset)
 * bit[N]    = clock gate     (1 = enabled, 0 = gated off)
 */

#define T113_CCU_PWM_BGR        (T113_CCU_BASE + 0x07ac)
#define T113_CCU_IOMMU_BGR      (T113_CCU_BASE + 0x07bc)
#define T113_CCU_DMA_BGR        (T113_CCU_BASE + 0x070c)
#define T113_CCU_MMC_BGR        (T113_CCU_BASE + 0x084c)
#define T113_CCU_UART_BGR       (T113_CCU_BASE + 0x090c)
#define T113_CCU_TWI_BGR        (T113_CCU_BASE + 0x091c)
#define T113_CCU_SPI_BGR        (T113_CCU_BASE + 0x096c)
#define T113_CCU_EMAC_BGR       (T113_CCU_BASE + 0x097c)
#define T113_CCU_IR_TX_BGR      (T113_CCU_BASE + 0x09cc)
#define T113_CCU_GPADC_BGR      (T113_CCU_BASE + 0x09ec)
#define T113_CCU_THS_BGR        (T113_CCU_BASE + 0x09fc)
#define T113_CCU_I2S_BGR        (T113_CCU_BASE + 0x0a20)
#define T113_CCU_SPDIF_BGR      (T113_CCU_BASE + 0x0a2c)
#define T113_CCU_DMIC_BGR       (T113_CCU_BASE + 0x0a4c)
#define T113_CCU_AUDIO_BGR      (T113_CCU_BASE + 0x0a5c)
#define T113_CCU_USB_BGR        (T113_CCU_BASE + 0x0a8c)
#define T113_CCU_LRADC_BGR      (T113_CCU_BASE + 0x0a9c)
#define T113_CCU_TPADC_BGR      (T113_CCU_BASE + 0x0c5c)

/* Peripheral clock source / divider registers */

#define T113_CCU_MMC0_CLK       (T113_CCU_BASE + 0x0830)
#define T113_CCU_MMC1_CLK       (T113_CCU_BASE + 0x0834)
#define T113_CCU_MMC2_CLK       (T113_CCU_BASE + 0x0838)
#define T113_CCU_SPI0_CLK       (T113_CCU_BASE + 0x0940)
#define T113_CCU_SPI1_CLK       (T113_CCU_BASE + 0x0944)
#define T113_CCU_USB0_CLK       (T113_CCU_BASE + 0x0a70)
#define T113_CCU_USB1_CLK       (T113_CCU_BASE + 0x0a74)

#endif /* __ARCH_ARM_SRC_T113_HARDWARE_T113_CCU_H */
