/****************************************************************************
 * arch/arm/src/t113/t113_boot0.S
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
 * Portions derived from Allwinner r528_boot0.S (Apache 2.0 + xboot MIT)
 * and vendor/allwinnertech/chips/r528/r528_boot0.S
 *
 ****************************************************************************/

/* ARM mode constants */
#define ARMV7_SVC_MODE     0x13
#define ARMV7_MODE_MASK    0x1f
#define ARMV7_IRQ_MASK     0x80
#define ARMV7_FIQ_MASK     0x40

.arm
.globl _boot0_start
.globl sys_copyself
.text

_boot0_start:
    /* BROM eGON header — must be at offset 0 of the binary.
     * BROM verifies: magic "eGON.BT0", checksum at offset 8,
     * spl_size at offset 12.  mksunxi patches the checksum. */
    .long  0xea00000e               /* b _boot0_start+64 (branch past header) */
    .byte  'e','G','O','N','.','B','T','0'
    .long  0x12345678               /* checksum placeholder — patched by mksunxi */
    .long  __spl_size               /* BROM loads this many bytes to SRAM 0x30000 */
    .long  0x30                     /* header size */
    .long  0x30303033               /* header version */
    .long  0x00020000               /* return value */
    .long  0x00028000               /* run address (unused — BROM loads to 0x30000) */
    .long  0x0                      /* eGON version */
    .byte  0x00, 0x00, 0x00, 0x00   /* platform info */
    .byte  0x34, 0x2e, 0x30, 0x00

    /* CPU reset code at offset 64 (.align 5 = 32-byte align → offset 0x40) */
    .align 5

    /* Set SVC mode, disable FIQ and IRQ */
    mrs r0, cpsr
    bic r0, r0, #ARMV7_MODE_MASK
    orr r0, r0, #ARMV7_SVC_MODE
    orr r0, r0, #(ARMV7_IRQ_MASK | ARMV7_FIQ_MASK)
    bic r0, r0, #(1<<9)             /* little-endian */
    msr cpsr_c, r0

    /* Set VBAR to our vector table (low vectors) */
    ldr r0, =_boot0_vector
    mcr p15, 0, r0, c12, c0, 0
    mrc p15, 0, r0, c1, c0, 0
    bic r0, #(1 << 13)
    mcr p15, 0, r0, c1, c0, 0

    /* Disable I-cache, disable MMU */
    mrc p15, 0, r0, c1, c0, 0
    bic r0, r0, #0x00002000         /* clear V bit */
    bic r0, r0, #0x00000007         /* clear C, A, M */
    orr r0, r0, #0x00000800         /* set BTB */
    bic r0, r0, #0x00001000         /* clear I-cache */
    mcr p15, 0, r0, c1, c0, 0

    /* Enable VFP/NEON */
    mrc p15, 0, r0, c1, c0, 2
    orr r0, r0, #(0xf << 20)
    mcr p15, 0, r0, c1, c0, 2
    isb
    mov r0, #0x40000000
    vmsr fpexc, r0

    /* Set stack pointer — use SRAM below load address 0x30000 */
    ldr sp, =0x00037000

    /* Set ARM Generic Timer frequency to 24 MHz */
    ldr r0, =24000000
    mcr p15, 0, r0, c14, c0, 0

    /* Run clock init + DDR init + copy full firmware to DDR */
    bl  sys_copyself

    /* Jump to NuttX __start via the pointer stored at _t113_start */
    ldr r0, =_t113_start
    ldr pc, [r0]

_boot0_vector:
    b   _boot0_start
    ldr pc, _not_used
    ldr pc, _not_used
    ldr pc, _not_used
    ldr pc, _not_used
    ldr pc, _not_used
    ldr pc, _not_used
    ldr pc, _not_used

_not_used:
    .word not_used

    .align 5
not_used:
    b .
