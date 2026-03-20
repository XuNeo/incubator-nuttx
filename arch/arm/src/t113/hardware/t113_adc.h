/****************************************************************************
 * arch/arm/src/t113/hardware/t113_adc.h
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

#ifndef __ARCH_ARM_SRC_T113_HARDWARE_T113_ADC_H
#define __ARCH_ARM_SRC_T113_HARDWARE_T113_ADC_H

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* T113 GPADC — 12-bit SAR ADC, up to 4 channels on this SoC variant. */

#define T113_GPADC_BASE         0x02009000

#define T113_GPADC_SR           (T113_GPADC_BASE + 0x00)
#define T113_GPADC_CTRL         (T113_GPADC_BASE + 0x04)
#define T113_GPADC_CS_EN        (T113_GPADC_BASE + 0x08)
#define T113_GPADC_FIFO_INTC    (T113_GPADC_BASE + 0x0c)
#define T113_GPADC_FIFO_INTS    (T113_GPADC_BASE + 0x10)
#define T113_GPADC_FIFO_DATA    (T113_GPADC_BASE + 0x14)
#define T113_GPADC_CB_DATA      (T113_GPADC_BASE + 0x18)

/* Per-channel data registers */

#define T113_GPADC_CH_DATA(n)   (T113_GPADC_BASE + 0x80 + (n) * 4)

/* CTRL register bits */

#define T113_GPADC_CTRL_ADC_EN      (1 << 16)
#define T113_GPADC_CTRL_CALI_EN     (1 << 17)
#define T113_GPADC_CTRL_MODE_SHIFT  18
#define T113_GPADC_CTRL_MODE_MASK   (3 << 18)
#define T113_GPADC_CTRL_MODE_SINGLE 0
#define T113_GPADC_CTRL_MODE_CYCLE  1
#define T113_GPADC_CTRL_MODE_CONT   2
#define T113_GPADC_CTRL_LDO_EN     (1 << 0)

/* CS_EN register: bit N = select channel N for conversion */

/* FIFO_INTC register */

#define T113_GPADC_FIFO_DATA_IRQ_EN (1 << 16)
#define T113_GPADC_FIFO_FLUSH       (1 << 4)

/* FIFO_INTS register */

#define T113_GPADC_FIFO_DATA_PEND   (1 << 16)

/* Data mask: 12-bit resolution */

#define T113_GPADC_DATA_MASK        0xfff

/* Voltage range 0-1.8V, reference is internal 1.8V */

#define T113_GPADC_VREF_MV          1800
#define T113_GPADC_MAX_VALUE        4095

#define T113_GPADC_NCHANNELS        4

#endif /* __ARCH_ARM_SRC_T113_HARDWARE_T113_ADC_H */
