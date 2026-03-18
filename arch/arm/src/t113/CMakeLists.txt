# ##############################################################################
# arch/arm/src/t113/CMakeLists.txt
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed to the Apache Software Foundation (ASF) under one or more contributor
# license agreements.  See the NOTICE file distributed with this work for
# additional information regarding copyright ownership.  The ASF licenses this
# file to you under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations under
# the License.
#
# ##############################################################################

set(SRCS t113_boot.c t113_irq.c t113_lowputc.c t113_serial.c)

if(NOT CONFIG_SCHED_TICKLESS)
  list(APPEND SRCS t113_timerisr.c)
endif()

if(CONFIG_T113_BOOT0)
  list(APPEND SRCS t113_load.c)
  set_source_files_properties(
    t113_load.c PROPERTIES COMPILE_OPTIONS "-fno-lto;-O3;-fno-stack-protector")
  list(APPEND SRCS t113_boot0.S)
endif()

if(CONFIG_T113_DMA)
  list(APPEND SRCS t113_dma.c)
endif()

if(CONFIG_SMP)
  list(APPEND SRCS t113_cpuboot.c)
endif()

if(CONFIG_T113_SPI0 OR CONFIG_T113_SPI1)
  list(APPEND SRCS t113_spi.c)
endif()

if(CONFIG_T113_TWI0
   OR CONFIG_T113_TWI1
   OR CONFIG_T113_TWI2
   OR CONFIG_T113_TWI3)
  list(APPEND SRCS t113_i2c.c)
endif()

if(CONFIG_T113_USBDEV)
  list(APPEND SRCS t113_usbdev.c)
endif()

target_sources(arch PRIVATE ${SRCS})
