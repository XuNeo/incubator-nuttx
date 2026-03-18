/****************************************************************************
 * boards/arm/t113/t113-evb/scripts/sdram.ld.S
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

#include <nuttx/config.h>

/* T113-S3 DDR linker script.
 *
 * When CONFIG_T113_BOOT0=y:
 *   The binary starts with the boot0 BROM header at CONFIG_RAM_START.
 *   BROM loads spl_size bytes to SRAM 0x30000, runs clock/DDR init,
 *   copies the full bin to CONFIG_RAM_START, then jumps to _t113_start
 *   which holds the address of NuttX __start.
 *
 * When CONFIG_T113_BOOT0=n (xfel debug):
 *   Standard layout: vectors at CONFIG_RAM_START, __start entry.
 */

MEMORY
{
    sdram (W!RX) : ORIGIN = CONFIG_RAM_START, LENGTH = CONFIG_RAM_SIZE
}

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_stext)

SECTIONS
{
    .text :
    {
        __image_start = .;

        __spl_start = .;
#ifdef CONFIG_T113_BOOT0
        KEEP(*t113_boot0*(.text*))
        KEEP(*t113_load*(.text*))
#endif
        __spl_end = .;
        PROVIDE(__spl_size = __spl_end - __spl_start);

        _stext = ABSOLUTE(.);
        *(.vectors)
        *(.text .text.*)
        *(.fixup)
        *(.gnu.warning)
        *(.rodata .rodata.*)
        *(.gnu.linkonce.t.*)
        *(.glue_7)
        *(.glue_7t)
        *(.got)
        *(.gcc_except_table)
        *(.gnu.linkonce.r.*)
        *(.ARM.extab*)
        *(.gnu.linkonce.armextab.*)
        PROVIDE_HIDDEN (__exidx_start = .);
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        PROVIDE_HIDDEN (__exidx_end = .);
        _etext = ABSOLUTE(.);

    } > sdram

#ifdef CONFIG_T113_BOOT0
    .t113_start :
    {
        _t113_start = ABSOLUTE(.);
        LONG(__start)
    } > sdram
#endif

    .init_section :
    {
        _sinit = ABSOLUTE(.);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP(*(.init_array EXCLUDE_FILE(*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o) .ctors))
        _einit = ABSOLUTE(.);
    } > sdram

    .data :
    {
        _sdata = ABSOLUTE(.);
        _eronly = ABSOLUTE(.);
        *(.data .data.*)
        *(.gnu.linkonce.d.*)
        CONSTRUCTORS
        . = ALIGN(4);
        _edata = ABSOLUTE(.);
    } > sdram
    __image_end = _edata;

    .noinit :
    {
        _snoinit = ABSOLUTE(.);
        *(.noinit*)
        _enoinit = ABSOLUTE(.);
    } > sdram

    .bss :
    {
        _sbss = ABSOLUTE(.);
        *(.bss .bss.*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = ABSOLUTE(.);
    } > sdram

    .stab 0 : { *(.stab) }
    .stabstr 0 : { *(.stabstr) }
    .stab.excl 0 : { *(.stab.excl) }
    .stab.exclstr 0 : { *(.stab.exclstr) }
    .stab.index 0 : { *(.stab.index) }
    .stab.indexstr 0 : { *(.stab.indexstr) }
    .comment 0 : { *(.comment) }
    .debug_abbrev 0 : { *(.debug_abbrev) }
    .debug_info 0 : { *(.debug_info) }
    .debug_line 0 : { *(.debug_line) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    .debug_aranges 0 : { *(.debug_aranges) }
}
