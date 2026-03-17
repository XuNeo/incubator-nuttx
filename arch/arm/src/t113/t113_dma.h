/****************************************************************************
 * arch/arm/src/t113/t113_dma.h
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

#ifndef __ARCH_ARM_SRC_T113_T113_DMA_H
#define __ARCH_ARM_SRC_T113_T113_DMA_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>
#include <stdbool.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define T113_DMA_NCHANNELS 16

/****************************************************************************
 * Public Types
 ****************************************************************************/

typedef void *DMA_HANDLE;
typedef void (*dma_callback_t)(DMA_HANDLE handle, uint8_t status, void *arg);

/* DMA transfer config passed to t113_dmasetup() */

struct t113_dma_config_s
{
  uint8_t  src_drq;       /* Source DRQ type (DRQ_* from hardware/t113_dma.h) */
  uint8_t  dst_drq;       /* Destination DRQ type */
  uint8_t  src_width;     /* Source data width (DMAC_WIDTH_*) */
  uint8_t  dst_width;     /* Destination data width */
  uint8_t  src_burst;     /* Source burst length (DMAC_BURST_*) */
  uint8_t  dst_burst;     /* Destination burst length */
  bool     src_linear;    /* true=linear (memory), false=IO (peripheral) */
  bool     dst_linear;    /* true=linear (memory), false=IO (peripheral) */
  uint8_t  wait_cyc;      /* Wait cycles between packets (0-255) */
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

void t113_dma_initialize(void);

DMA_HANDLE t113_dmachannel(void);
void       t113_dmafree(DMA_HANDLE handle);

int  t113_dmasetup(DMA_HANDLE handle,
                   uintptr_t src, uintptr_t dst, size_t len,
                   const struct t113_dma_config_s *cfg);

int  t113_dmastart(DMA_HANDLE handle,
                   dma_callback_t callback, void *arg);

void t113_dmastop(DMA_HANDLE handle);

size_t t113_dmaresidual(DMA_HANDLE handle);

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __ARCH_ARM_SRC_T113_T113_DMA_H */
