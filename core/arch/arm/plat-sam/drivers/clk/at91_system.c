// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Copyright (C) 2013 Boris BREZILLON <b.brezillon@overkiz.com>
 */

#include <io.h>
#include <kernel/delay.h>
#include <kernel/panic.h>
#include <mm/core_memprot.h>
#include <types_ext.h>

#include "at91_clk.h"

#define SYSTEM_MAX_ID		31

#define SYSTEM_MAX_NAME_SZ	32

struct clk_system {
	vaddr_t base;
	uint8_t id;
};

static inline int is_pck(int id)
{
	return (id >= 8) && (id <= 15);
}

static inline bool clk_system_ready(vaddr_t base, int id)
{
	unsigned int status = io_read32(base + AT91_PMC_SR);

	return !!(status & (1 << id));
}

static int clk_system_enable(struct clk *clk)
{
	struct clk_system *sys = clk->priv;

	io_write32(sys->base + AT91_PMC_SCER, 1 << sys->id);

	if (!is_pck(sys->id))
		return 0;

	while (!clk_system_ready(sys->base, sys->id))
		;

	return 0;
}

static void clk_system_disable(struct clk *clk)
{
	struct clk_system *sys = clk->priv;

	io_write32(sys->base + AT91_PMC_SCDR, 1 << sys->id);
}

static int clk_system_is_enabled(struct clk *clk)
{
	struct clk_system *sys = clk->priv;
	unsigned int status = io_read32(sys->base + AT91_PMC_SCSR);

	if (!(status & (1 << sys->id)))
		return 0;

	if (!is_pck(sys->id))
		return 1;

	status = io_read32(sys->base + AT91_PMC_SR);

	return !!(status & (1 << sys->id));
}

static const struct clk_ops system_ops = {
	.enable = clk_system_enable,
	.disable = clk_system_disable,
	.is_enabled = clk_system_is_enabled,
};

struct clk *
at91_clk_register_system(struct pmc_data *pmc, const char *name,
			 struct clk *parent, uint8_t id)
{
	struct clk_system *sys;
	struct clk *clk;

	if (!parent || id > SYSTEM_MAX_ID)
		return NULL;

	clk = clk_alloc(name, &system_ops, &parent, 1);
	if (!clk)
		return NULL;

	sys = malloc(sizeof(*sys));
	if (!sys) {
		clk_free(clk);
		return NULL;
	}

	sys->id = id;
	sys->base = pmc->base;

	clk->flags = CLK_SET_RATE_PARENT;
	clk->priv = sys;

	if (clk_register(clk)) {
		clk_free(clk);
		free(sys);
		return NULL;
	}

	return clk;
}
