/****************************************************************************
 * boards/arm/t113/t113-evb/src/t113_bringup.c
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

#include <stdio.h>
#include <debug.h>
#include <errno.h>

#ifdef CONFIG_T113_SPI0
#  include <nuttx/spi/spi.h>
#  include "t113_spi.h"
#endif

#ifdef CONFIG_MTD_MX35
#  include <nuttx/mtd/mtd.h>
extern FAR struct mtd_dev_s *mx35_initialize(FAR struct spi_dev_s *dev);
#endif

#ifdef CONFIG_T113_TWI0
#  include <nuttx/i2c/i2c_master.h>
#  include "t113_i2c.h"
#endif

#include "t113-evb.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int t113_bringup(void)
{
  int ret = 0;

#if defined(CONFIG_T113_SPI0) && defined(CONFIG_MTD_MX35)
  FAR struct spi_dev_s *spi;
  FAR struct mtd_dev_s *mtd;

  spi = t113_spibus_initialize(0);
  if (spi == NULL)
    {
      syslog(LOG_ERR, "t113_bringup: SPI0 init failed\n");
    }
  else
    {
      uint8_t tx[4] = { 0x9f, 0x00, 0x00, 0x00 };
      uint8_t rx[4] = { 0 };
      SPI_LOCK(spi, true);
      SPI_SETFREQUENCY(spi, 1000000);
      SPI_SETMODE(spi, SPIDEV_MODE0);
      SPI_SETBITS(spi, 8);
      SPI_SELECT(spi, SPIDEV_FLASH(0), true);
      SPI_EXCHANGE(spi, tx, rx, 4);
      SPI_SELECT(spi, SPIDEV_FLASH(0), false);
      SPI_LOCK(spi, false);
      printf("SPI NAND ID: %02x %02x %02x %02x\n",
             rx[0], rx[1], rx[2], rx[3]);
      mtd = mx35_initialize(spi);
      if (mtd == NULL)
        {
          syslog(LOG_ERR, "t113_bringup: MX35 MTD init failed\n");
          printf("ERROR: MX35 init failed - check SPI wiring\n");
        }
      else
        {
          ret = register_mtddriver("/dev/mtd0", mtd, 0755, NULL);
          if (ret < 0)
            {
              syslog(LOG_ERR, "register_mtddriver failed: %d\n", ret);
            }
          else
            {
              syslog(LOG_INFO, "MX35 SPI NAND registered at /dev/mtd0\n");
            }

#ifdef CONFIG_FS_LITTLEFS
          ret = nx_mount("/dev/mtd0", "/mnt", "littlefs", 0, NULL);
          if (ret < 0)
            {
              syslog(LOG_WARNING,
                     "LittleFS mount failed (%d), trying format\n", ret);
              ret = nx_mount("/dev/mtd0", "/mnt", "littlefs", 0,
                             "forceformat");
              if (ret == 0)
                {
                  syslog(LOG_INFO, "LittleFS formatted and mounted at /mnt\n");
                }
            }
          else
            {
              syslog(LOG_INFO, "LittleFS mounted at /mnt\n");
            }
#endif
        }
    }
#endif

#ifdef CONFIG_T113_TWI0
  FAR struct i2c_master_s *i2c;

  i2c = t113_i2cbus_initialize(0);
  if (i2c == NULL)
    {
      syslog(LOG_ERR, "t113_bringup: TWI0 init failed\n");
    }
  else
    {
#ifdef CONFIG_I2C_DRIVER
      i2c_register(i2c, 0);
      syslog(LOG_INFO, "TWI0 registered at /dev/i2c0\n");
#endif
    }
#endif

  return ret;
}
