// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright 2022 Microchip
 */

#include <drivers/rtc.h>
#include <tee_api_types.h>

struct rtc *rtc_device;

TEE_Result rtc_register(struct rtc *rtc)
{
	/* RTC should *at least* allow to get the time */
	if (!rtc->ops->get_time)
		return TEE_ERROR_BAD_PARAMETERS;

	rtc_device = rtc;

	return TEE_SUCCESS;
}
