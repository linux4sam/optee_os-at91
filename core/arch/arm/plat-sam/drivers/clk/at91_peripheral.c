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

#define PERIPHERAL_ID_MIN	2
#define PERIPHERAL_ID_MAX	31
#define PERIPHERAL_MASK(id)	(1 << ((id) & PERIPHERAL_ID_MAX))

#define PERIPHERAL_MAX_SHIFT	3

struct clk_sam9x5_peripheral {
	vaddr_t base;
	struct clk_range range;
	uint32_t id;
	uint32_t div;
	const struct clk_pcr_layout *layout;
	bool auto_div;
};

static void clk_sam9x5_peripheral_autodiv(struct clk *clk)
{
	struct clk *parent;
	struct clk_sam9x5_peripheral *periph = clk->priv;
	unsigned long parent_rate;
	int shift = 0;

	if (!periph->auto_div)
		return;

	if (periph->range.max) {
		parent = clk_get_parent_by_index(clk, 0);
		parent_rate = clk_get_rate(parent);
		if (!parent_rate)
			return;

		for (; shift < PERIPHERAL_MAX_SHIFT; shift++) {
			if (parent_rate >> shift <= periph->range.max)
				break;
		}
	}

	periph->auto_div = false;
	periph->div = shift;
}

static int clk_sam9x5_peripheral_enable(struct clk *clk)
{
	struct clk_sam9x5_peripheral *periph = clk->priv;

	if (periph->id < PERIPHERAL_ID_MIN)
		return 0;

	io_write32(periph->base + periph->layout->offset,
		   (periph->id & periph->layout->pid_mask));
	io_clrsetbits32(periph->base + periph->layout->offset,
			periph->layout->div_mask | periph->layout->cmd |
			AT91_PMC_PCR_EN,
			field_prep(periph->layout->div_mask, periph->div) |
			periph->layout->cmd |
			AT91_PMC_PCR_EN);

	return 0;
}

static void clk_sam9x5_peripheral_disable(struct clk *clk)
{
	struct clk_sam9x5_peripheral *periph = clk->priv;

	if (periph->id < PERIPHERAL_ID_MIN)
		return;

	io_write32(periph->base + periph->layout->offset,
		   (periph->id & periph->layout->pid_mask));
	io_clrsetbits32(periph->base + periph->layout->offset,
			AT91_PMC_PCR_EN | periph->layout->cmd,
			periph->layout->cmd);
}

static int clk_sam9x5_peripheral_is_enabled(struct clk *clk)
{
	struct clk_sam9x5_peripheral *periph = clk->priv;
	unsigned int status;

	if (periph->id < PERIPHERAL_ID_MIN)
		return 1;

	io_write32(periph->base + periph->layout->offset,
		   (periph->id & periph->layout->pid_mask));
	status = io_read32(periph->base + periph->layout->offset);

	return !!(status & AT91_PMC_PCR_EN);
}

static unsigned long
clk_sam9x5_peripheral_get_rate(struct clk *clk,
			       unsigned long parent_rate)
{
	struct clk_sam9x5_peripheral *periph = clk->priv;
	unsigned int status;

	if (periph->id < PERIPHERAL_ID_MIN)
		return parent_rate;

	io_write32(periph->base + periph->layout->offset,
		   (periph->id & periph->layout->pid_mask));
	status = io_read32(periph->base + periph->layout->offset);

	if (status & AT91_PMC_PCR_EN) {
		periph->div = field_get(periph->layout->div_mask, status);
		periph->auto_div = false;
	} else {
		clk_sam9x5_peripheral_autodiv(clk);
	}

	return parent_rate >> periph->div;
}

static int clk_sam9x5_peripheral_set_rate(struct clk *clk,
					  unsigned long rate,
					  unsigned long parent_rate)
{
	int shift;
	struct clk_sam9x5_peripheral *periph = clk->priv;

	if (periph->id < PERIPHERAL_ID_MIN || !periph->range.max) {
		if (parent_rate == rate)
			return 0;
		else
			return -1;
	}

	if (periph->range.max && rate > periph->range.max)
		return -1;

	for (shift = 0; shift <= PERIPHERAL_MAX_SHIFT; shift++) {
		if (parent_rate >> shift == rate) {
			periph->auto_div = false;
			periph->div = shift;
			return 0;
		}
	}

	return -1;
}

static const struct clk_ops sam9x5_peripheral_ops = {
	.enable = clk_sam9x5_peripheral_enable,
	.disable = clk_sam9x5_peripheral_disable,
	.is_enabled = clk_sam9x5_peripheral_is_enabled,
	.get_rate = clk_sam9x5_peripheral_get_rate,
	.set_rate = clk_sam9x5_peripheral_set_rate,
};

struct clk *
at91_clk_register_sam9x5_peripheral(struct pmc_data *pmc,
				    const struct clk_pcr_layout *layout,
				    const char *name, struct clk *parent,
				    uint32_t id, const struct clk_range *range)
{
	struct clk_sam9x5_peripheral *periph;
	struct clk *clk;

	if (!name || !parent)
		return NULL;

	clk = clk_alloc(name, &sam9x5_peripheral_ops, &parent, 1);
	if (!clk)
		return NULL;

	periph = malloc(sizeof(*periph));
	if (!periph) {
		clk_free(clk);
		return NULL;
	}

	periph->id = id;
	periph->div = 0;
	periph->base = pmc->base;
	if (layout->div_mask)
		periph->auto_div = true;
	periph->layout = layout;
	periph->range = *range;

	clk->priv = periph;

	if (clk_register(clk)) {
		clk_free(clk);
		free(periph);
		return 0;
	}

	clk_sam9x5_peripheral_autodiv(clk);

	return clk;
}
