// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Bootlin
 */

#include "at91_rstc.h"
#include <compiler.h>
#include <generic_driver.h>
#include <io.h>
#include <kernel/dt.h>
#include <stdbool.h>
#include <tee_api_defines.h>
#include <tee_api_types.h>
#include <trace.h>
#include <types_ext.h>

#define AT91_RSTC_CR		0x0
#define   AT91_RSTC_CR_KEY	(0xA5 << 24)
#define   AT91_RSTC_CR_PROCRST	BIT32(0)
#define   AT91_RSTC_CR_PERRST	BIT32(2)

static vaddr_t rstc_base = 0;

bool at91_rstc_available(void)
{
	return rstc_base != 0;
}

void __noreturn at91_rstc_reset(void)
{
	uint32_t val = AT91_RSTC_CR_KEY | AT91_RSTC_CR_PROCRST |
		       AT91_RSTC_CR_PERRST;

	io_write32(rstc_base + AT91_RSTC_CR, val);

	while(1);
}

static TEE_Result rstc_setup(const void *fdt, int nodeoffset,
				 int status __unused)
{
	size_t size = 0;

	if (dt_map_dev(fdt, nodeoffset, &rstc_base, &size) < 0)
		return TEE_ERROR_GENERIC;

	return TEE_SUCCESS;
}

static const struct generic_driver rstc_driver = {
	.setup = rstc_setup,
};

static const struct dt_device_match rstc_match_table[] = {
	{ .compatible = "atmel,sama5d3-rstc" },
	{ 0 }
};

const struct dt_driver rstc_dt_driver __dt_driver = {
	.name = "rstc",
	.type = DT_DRIVER_GENERIC,
	.match_table = rstc_match_table,
	.driver = &rstc_driver,
};
