/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2022, Microchip
 */
#ifndef __RTC_PTA_CLIENT_H
#define __RTC_PTA_CLIENT_H

#include <tee_api_types.h>

#define PTA_RTC_UUID { 0xf389f8c8, 0x845f, 0x496c, \
		{ 0x8b, 0xbe, 0xd6, 0x4b, 0xd2, 0x4c, 0x92, 0xfd } }

#define RTC_INFO_VERSION	0x1

struct pta_rtc_time {
	uint32_t tm_sec;
	uint32_t tm_min;
	uint32_t tm_hour;
	uint32_t tm_mday;
	uint32_t tm_mon;
	uint32_t tm_year;
	uint32_t tm_wday;
};

struct pta_rtc_info {
	uint64_t version;
	uint64_t features;
	struct pta_rtc_time range_min;
	struct pta_rtc_time range_max;
};

/*
 * PTA_CMD_RTC_GET_INFO - Get RTC information
 *
 * param[0] (out memref) - RTC buffer memory reference containing a struct
 *			   pta_rtc_info
 * param[1] unused
 * param[2] unused
 * param[3] unused
 *
 * Result:
 * TEE_SUCCESS - Invoke command success
 */
#define PTA_CMD_RTC_GET_INFO	0x0

/*
 * PTA_CMD_RTC_GET_TIME - Get time from RTC
 *
 * param[0] (out memref) - RTC buffer memory reference containing a struct
 *			   optee_rtc_time
 * param[1] unused
 * param[2] unused
 * param[3] unused
 *
 * Result:
 * TEE_SUCCESS - Invoke command success
 * TEE_ERROR_BAD_PARAMETERS - Incorrect struct size for RTC time
 */
#define PTA_CMD_RTC_GET_TIME		0x1

/*
 * PTA_CMD_RTC_GET_TIME - Get time from RTC
 *
 * param[0] (in memref) - RTC buffer memory reference containing a struct
 *			  optee_rtc_time
 * param[1] unused
 * param[2] unused
 * param[3] unused
 *
 * Result:
 * TEE_SUCCESS - Invoke command success
 * TEE_ERROR_BAD_PARAMETERS - Incorrect struct size for RTC time
 */
#define PTA_CMD_RTC_SET_TIME		0x2

/*
 * PTA_CMD_RTC_GET_OFFSET - Get RTC offset
 *
 * param[0] (out value) - value.a: RTC offset
 * param[1] unused
 * param[2] unused
 * param[3] unused
 *
 * Result:
 * TEE_SUCCESS - Invoke command success
 * TEE_ERROR_BAD_PARAMETERS - Incorrect input param
 */
#define PTA_CMD_RTC_GET_OFFSET		0x3

/*
 * PTA_CMD_RTC_SET_OFFSET - Set RTC offset
 *
 * param[0] (in value) - value.a: RTC offset to be set
 * param[1] unused
 * param[2] unused
 * param[3] unused
 *
 * Result:
 * TEE_SUCCESS - Invoke command success
 * TEE_ERROR_BAD_PARAMETERS - Incorrect input param
 */
#define PTA_CMD_RTC_SET_OFFSET		0x4

#endif /* __RTC_PTA_CLIENT_H */
