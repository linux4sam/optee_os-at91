// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Bootlin
 */

#include <assert.h>
#include <drivers/clk.h>
#include <drivers/clk_dt.h>
#include <initcall.h>
#include <io.h>
#include <libfdt.h>
#include <kernel/boot.h>
#include <kernel/dt.h>
#include <malloc.h>
#include <stdint.h>
#include <util.h>

struct fixed_clock_data {
	unsigned long rate;
};

static unsigned long fixed_clk_get_rate(struct clk *clk,
					unsigned long parent_rate __unused)
{
	struct fixed_clock_data *d = clk->priv;

	return d->rate;
}

static const struct clk_ops fixed_clk_clk_ops = {
	.get_rate = fixed_clk_get_rate,
};

static TEE_Result fixed_clock_setup(const void *fdt, int offs)
{
	const uint32_t *freq;
	const char *name;
	struct clk *clk;
	TEE_Result res;
	struct fixed_clock_data *fcd;

	name = fdt_get_name(fdt, offs, NULL);
	if (!name)
		name = "fixed-clock";

	clk = clk_alloc(name, &fixed_clk_clk_ops, NULL, 0);

	fcd = malloc(sizeof(struct fixed_clock_data));
	if (!clk || !fcd)
		return TEE_ERROR_OUT_OF_MEMORY;

	freq = fdt_getprop(fdt, offs, "clock-frequency", NULL);
	if (!freq)
		return TEE_ERROR_BAD_FORMAT;

	fcd->rate = fdt32_to_cpu(*freq);
	clk->priv = fcd;

	res = clk_register(clk);
	if (res)
		return res;

	return clk_dt_register_clk_provider(fdt, offs, clk_dt_get_simple_clk, clk);
}

CLK_DT_DECLARE(fixed_clock, "fixed-clock", fixed_clock_setup);
