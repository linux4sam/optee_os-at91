// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Copyright (C) 2013 Boris BREZILLON <b.brezillon@overkiz.com>
 */

#include <dt-bindings/clock/at91.h>

#include <assert.h>
#include <drivers/clk.h>
#include <drivers/clk_dt.h>
#include <initcall.h>
#include <io.h>
#include <libfdt.h>
#include <malloc.h>
#include <kernel/boot.h>
#include <kernel/dt.h>
#include <kernel/panic.h>
#include <stdint.h>
#include <util.h>

#define SLOW_CLOCK_FREQ			32768

static unsigned long sckc_get_rate(struct clk *clk __unused,
				   unsigned long parent_rate __unused)
{
	return SLOW_CLOCK_FREQ;
}

static const struct clk_ops sckc_clk_ops = {
	.get_rate = sckc_get_rate,
};

static TEE_Result sckc_pmc_setup(const void *fdt __unused, int offs)
{
	struct clk *clk = NULL;
	TEE_Result res;

	clk = clk_alloc("slowck", &sckc_clk_ops, NULL, 0);
	if (!clk)
		return TEE_ERROR_OUT_OF_MEMORY;

	res = clk_register(clk);
	if (res) {
		clk_free(clk);
		return res;
	}

	return clk_dt_register_clk_provider(fdt, offs, clk_dt_get_simple_clk, clk);
}

CLK_DT_DECLARE(at91_sckc, "atmel,sama5d4-sckc", sckc_pmc_setup);
