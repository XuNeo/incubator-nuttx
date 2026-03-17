/****************************************************************************
 * boards/arm/t113/t113-evb/include/board.h
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

#ifndef __BOARDS_ARM_T113_T113_EVB_INCLUDE_BOARD_H
#define __BOARDS_ARM_T113_T113_EVB_INCLUDE_BOARD_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#define BOARD_UART0_BAUD     115200
#define BOARD_UART0_BITS     8
#define BOARD_UART0_PARITY   0
#define BOARD_UART0_2STOP    0

#define BOARD_UART0_TX_PIN   2
#define BOARD_UART0_RX_PIN   4
#define BOARD_UART0_PIN_FUNC 3

#endif /* __BOARDS_ARM_T113_T113_EVB_INCLUDE_BOARD_H */
