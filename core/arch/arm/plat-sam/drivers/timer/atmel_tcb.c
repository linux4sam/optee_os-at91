// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Bootlin
 */

#include <drivers/clk.h>
#include <libfdt.h>
#include <kernel/boot.h>
#include <tee_api_defines.h>

static const char *tcb_clocks[] = {"t0_clk", "gclk", "slow_clk"};

static TEE_Result atmel_tcb_enable_clocks(void *fdt, int node)
{
	unsigned int i;
	struct clk *clk;

	for (i = 0; i < ARRAY_SIZE(tcb_clocks); i++) {
		clk = clk_of_get_by_name(fdt, node, tcb_clocks[i]);
		if (!clk)
			return TEE_ERROR_ITEM_NOT_FOUND;

		clk_enable(clk);
	}

	return TEE_SUCCESS;
}

static TEE_Result atmel_tcb_enable_init(void)
{
	int node = 0, ret;
	void *fdt = get_embedded_dt();

	while (1) {
		node = fdt_node_offset_by_compatible(fdt, node, "atmel,sama5d2-tcb");
		if (node < 0)
			break;

		ret = atmel_tcb_enable_clocks(fdt, node);
		if (ret)
			return ret;
	}

	return TEE_SUCCESS;
}
driver_init(atmel_tcb_enable_init);
