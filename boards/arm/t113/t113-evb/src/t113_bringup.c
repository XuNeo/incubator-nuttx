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

#ifdef CONFIG_T113_RTC
void t113_rtc_initialize(void);
#endif

#ifdef CONFIG_T113_PWM
void t113_pwm_initialize(int channel);
#endif

#ifdef CONFIG_T113_GPADC
void t113_adc_initialize(void);
#endif

#ifdef CONFIG_T113_WDT
void t113_wdt_initialize(void);
#endif

#include "t113-evb.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int t113_bringup(void)
{
  int ret = 0;

#ifdef CONFIG_T113_RTC
  t113_rtc_initialize();
#endif

#ifdef CONFIG_T113_PWM
  t113_pwm_initialize(0);
#endif

#ifdef CONFIG_T113_GPADC
  t113_adc_initialize();
#endif

#ifdef CONFIG_T113_WDT
  t113_wdt_initialize();
#endif

#if defined(CONFIG_T113_SPI0) && defined(CONFIG_MTD_MX35)
  FAR struct spi_dev_s *spi;
  FAR struct mtd_dev_s *mtd;
#if defined(CONFIG_FS_LITTLEFS) && defined(CONFIG_MTD_PARTITION)
  FAR struct mtd_dev_s *part;
  struct mtd_geometry_s geo;
  int pages_per_erase;
  int start_page;
  int npart_pages;
#endif

  spi = t113_spibus_initialize(0);
  if (spi != NULL)
    {
      mtd = mx35_initialize(spi);
      if (mtd != NULL)
        {
          ret = register_mtddriver("/dev/mtd0", mtd, 0755, NULL);
          if (ret < 0)
            {
              syslog(LOG_ERR, "register_mtddriver failed: %d\n", ret);
            }

#if defined(CONFIG_FS_LITTLEFS) && defined(CONFIG_MTD_PARTITION)
          mtd->ioctl(mtd, MTDIOC_GEOMETRY, (unsigned long)&geo);

          pages_per_erase = geo.erasesize / geo.blocksize;
          start_page = (geo.neraseblocks - 4) * pages_per_erase;
          npart_pages = 4 * pages_per_erase;
          part = mtd_partition(mtd, start_page, npart_pages);
          if (part != NULL)
            {
              register_mtddriver("/dev/mtd1", part, 0755, NULL);
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
#endif
    }
#endif

  return ret;
}
