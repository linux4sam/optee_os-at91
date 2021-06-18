// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Copyright (C) 2013 Boris BREZILLON <b.brezillon@overkiz.com>
 */

#include <kernel/panic.h>
#include <malloc.h>
#include <string.h>
#include <types_ext.h>

#include "at91_clk.h"

#include <dt-bindings/clock/at91.h>

static struct clk *pmc_clk_get_by_id(struct pmc_clk *clks, unsigned int nclk,
				     unsigned int id)
{
	unsigned int i = 0;

	for (i = 0; i < nclk; i++) {
		if (clks[i].clk && clks[i].id == id)
			return clks[i].clk;
	}

	return NULL;
}

struct clk *pmc_clk_get_by_name(struct pmc_clk *clks, unsigned int nclk,
				const char *name)
{
	unsigned int i = 0;

	for (i = 0; i < nclk; i++) {
		if (strcmp(clks[i].clk->name, name) == 0)
			return clks[i].clk;
	}

	return NULL;
}

struct clk *clk_dt_pmc_get(struct clk_dt_phandle_args *clkspec, void *data)
{
	unsigned int type = clkspec->args[0];
	unsigned int idx = clkspec->args[1];
	struct pmc_data *pmc_data = data;
	struct pmc_clk *clks = NULL;
	unsigned int nclk = 0;

	switch (type) {
	case PMC_TYPE_CORE:
		nclk = pmc_data->ncore;
		clks = pmc_data->chws;
		break;
	case PMC_TYPE_SYSTEM:
		nclk = pmc_data->nsystem;
		clks = pmc_data->shws;
		break;
	case PMC_TYPE_PERIPHERAL:
		nclk = pmc_data->nperiph;
		clks = pmc_data->phws;
		break;
	case PMC_TYPE_GCK:
		nclk = pmc_data->ngck;
		clks = pmc_data->ghws;
		break;
	case PMC_TYPE_PROGRAMMABLE:
		nclk = pmc_data->npck;
		clks = pmc_data->pchws;
		break;
	default:
		return NULL;
	}

	return pmc_clk_get_by_id(clks, nclk, idx);
}

struct pmc_data *pmc_data_allocate(unsigned int ncore, unsigned int nsystem,
				   unsigned int nperiph, unsigned int ngck,
				   unsigned int npck)
{
	unsigned int num_clks = ncore + nsystem + nperiph + ngck + npck;
	unsigned int alloc_size = sizeof(struct pmc_data) +
				  num_clks * sizeof(struct pmc_clk);
	struct pmc_data *pmc_data;

	pmc_data = malloc(alloc_size);
	if (!pmc_data)
		return NULL;

	memset(pmc_data, 0, alloc_size);
	pmc_data->ncore = ncore;
	pmc_data->chws = pmc_data->hwtable;

	pmc_data->nsystem = nsystem;
	pmc_data->shws = pmc_data->chws + ncore;

	pmc_data->nperiph = nperiph;
	pmc_data->phws = pmc_data->shws + nsystem;

	pmc_data->ngck = ngck;
	pmc_data->ghws = pmc_data->phws + nperiph;

	pmc_data->npck = npck;
	pmc_data->pchws = pmc_data->ghws + ngck;

	return pmc_data;
}
