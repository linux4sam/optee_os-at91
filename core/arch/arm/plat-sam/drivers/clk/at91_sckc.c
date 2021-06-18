// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Copyright (C) 2013 Boris BREZILLON <b.brezillon@overkiz.com>
 */

#include <dt-bindings/clock/at91.h>

#include <assert.h>
#include <drivers/clk.h>
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

static void sckc_pmc_setup(void *fdt __unused, int offs)
{
	struct clk *clk;

	clk = clk_alloc("slowck", &sckc_clk_ops, NULL, 0);
	if (!clk)
		return;

	if (clk_register(clk)) {
		clk_free(clk);
		return;
	}
	clk_scmi_set_ids(clk, 0, AT91_SCMI_CLK_SCKC_32K);
	clk_of_register_clk_provider(offs, clk_of_get_simple_clk, clk);
}

CLK_OF_DECLARE(at91_sckc, "atmel,sama5d4-sckc", sckc_pmc_setup);
