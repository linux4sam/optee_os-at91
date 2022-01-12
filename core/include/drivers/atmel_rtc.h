/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021, Microchip
 */

#ifndef __DRIVERS_ATMEL_RTC_H
#define __DRIVERS_ATMEL_RTC_H

#ifdef CFG_ATMEL_RTC
void atmel_rtc_get_tamper_timestamp(void);
#else
static inline void atmel_rtc_get_tamper_timestamp(void) {}
#endif

#endif /* __DRIVERS_ATMEL_RTC_H */
