/****************************************************************************
 * arch/arm/src/t113/t113_rtc.c
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

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/timers/rtc.h>

#include "arm_internal.h"
#include "hardware/t113_rtc.h"
#include "hardware/t113_ccu.h"

/****************************************************************************
 * Public Data
 ****************************************************************************/

volatile bool g_rtc_enabled = false;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int t113_rtc_rdtime(FAR struct rtc_lowerhalf_s *lower,
                           FAR struct rtc_time *rtctime);
static int t113_rtc_settime(FAR struct rtc_lowerhalf_s *lower,
                            FAR const struct rtc_time *rtctime);
static bool t113_rtc_havesettime(FAR struct rtc_lowerhalf_s *lower);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct rtc_ops_s g_rtc_ops =
{
  .rdtime      = t113_rtc_rdtime,
  .settime     = t113_rtc_settime,
  .havesettime = t113_rtc_havesettime,
};

static struct rtc_lowerhalf_s g_rtc_lowerhalf =
{
  .ops = &g_rtc_ops,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int t113_rtc_rdtime(FAR struct rtc_lowerhalf_s *lower,
                           FAR struct rtc_time *rtctime)
{
  uint32_t ymd;
  uint32_t hms;

  /* Read date and time registers.
   * Read HMS first, then YMD; re-read if second rolled over.
   */

  do
    {
      hms = getreg32(T113_RTC_HMS);
      ymd = getreg32(T113_RTC_YMD);
    }
  while (hms != getreg32(T113_RTC_HMS));

  rtctime->tm_sec  = (hms & T113_RTC_HMS_SEC_MASK) >>
                     T113_RTC_HMS_SEC_SHIFT;
  rtctime->tm_min  = (hms & T113_RTC_HMS_MIN_MASK) >>
                     T113_RTC_HMS_MIN_SHIFT;
  rtctime->tm_hour = (hms & T113_RTC_HMS_HOUR_MASK) >>
                     T113_RTC_HMS_HOUR_SHIFT;
  rtctime->tm_mday = (ymd & T113_RTC_YMD_DAY_MASK) >>
                     T113_RTC_YMD_DAY_SHIFT;
  rtctime->tm_mon  = ((ymd & T113_RTC_YMD_MON_MASK) >>
                     T113_RTC_YMD_MON_SHIFT) - 1;
  rtctime->tm_year = ((ymd & T113_RTC_YMD_YEAR_MASK) >>
                     T113_RTC_YMD_YEAR_SHIFT) +
                     T113_RTC_YEAR_BASE - 1900;

  return OK;
}

static int t113_rtc_settime(FAR struct rtc_lowerhalf_s *lower,
                            FAR const struct rtc_time *rtctime)
{
  uint32_t ymd;
  uint32_t hms;
  uint32_t ctrl;
  int year;

  year = (rtctime->tm_year + 1900) - T113_RTC_YEAR_BASE;
  if (year < 0 || year > 63)
    {
      return -EINVAL;
    }

  ymd = ((uint32_t)year << T113_RTC_YMD_YEAR_SHIFT) |
        ((uint32_t)(rtctime->tm_mon + 1) << T113_RTC_YMD_MON_SHIFT) |
        ((uint32_t)rtctime->tm_mday << T113_RTC_YMD_DAY_SHIFT);

  hms = ((uint32_t)rtctime->tm_hour << T113_RTC_HMS_HOUR_SHIFT) |
        ((uint32_t)rtctime->tm_min << T113_RTC_HMS_MIN_SHIFT) |
        ((uint32_t)rtctime->tm_sec << T113_RTC_HMS_SEC_SHIFT);

  /* Set access bits to allow writing */

  ctrl = getreg32(T113_RTC_LOSC_CTRL);
  putreg32(T113_RTC_LOSC_MAGIC | ctrl | T113_RTC_LOSC_YMD_ACC |
           T113_RTC_LOSC_HMS_ACC, T113_RTC_LOSC_CTRL);

  putreg32(ymd, T113_RTC_YMD);
  putreg32(hms, T113_RTC_HMS);

  return OK;
}

static bool t113_rtc_havesettime(FAR struct rtc_lowerhalf_s *lower)
{
  /* If year register is non-zero, time has been set */

  uint32_t ymd = getreg32(T113_RTC_YMD);
  return (ymd & T113_RTC_YMD_YEAR_MASK) != 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void t113_rtc_initialize(void)
{
  rtc_initialize(0, &g_rtc_lowerhalf);
}

int up_rtc_initialize(void)
{
  g_rtc_enabled = true;
  return OK;
}

int up_rtc_getdatetime(FAR struct tm *tp)
{
  struct rtc_time rtctime;
  t113_rtc_rdtime(&g_rtc_lowerhalf, &rtctime);
  tp->tm_sec  = rtctime.tm_sec;
  tp->tm_min  = rtctime.tm_min;
  tp->tm_hour = rtctime.tm_hour;
  tp->tm_mday = rtctime.tm_mday;
  tp->tm_mon  = rtctime.tm_mon;
  tp->tm_year = rtctime.tm_year;
  return OK;
}

#ifdef CONFIG_RTC_DATETIME
int up_rtc_settime(FAR const struct timespec *ts)
{
  struct rtc_time rtctime;
  struct tm t;

  gmtime_r(&ts->tv_sec, &t);
  rtctime.tm_sec  = t.tm_sec;
  rtctime.tm_min  = t.tm_min;
  rtctime.tm_hour = t.tm_hour;
  rtctime.tm_mday = t.tm_mday;
  rtctime.tm_mon  = t.tm_mon;
  rtctime.tm_year = t.tm_year;

  return t113_rtc_settime(&g_rtc_lowerhalf, &rtctime);
}
#endif
