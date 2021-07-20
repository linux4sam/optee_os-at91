// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Copyright (C) 2013 Boris BREZILLON <b.brezillon@overkiz.com>
 */

#include <io.h>
#include <kernel/delay.h>
#include <kernel/panic.h>
#include <util.h>
#include <mm/core_memprot.h>
#include <types_ext.h>

#include "at91_clk.h"

#define PLL_STATUS_MASK(id)	(1 << (1 + (id)))
#define PLL_REG(id)		(AT91_CKGR_PLLAR + ((id) * 4))
#define PLL_DIV_MASK		0xff
#define PLL_DIV_MAX		PLL_DIV_MASK
#define PLL_DIV(reg)		((reg) & PLL_DIV_MASK)
#define PLL_MUL(reg, layout)	(((reg) >> (layout)->mul_shift) & \
				 (layout)->mul_mask)
#define PLL_MUL_MIN		2
#define PLL_MUL_MASK(layout)	((layout)->mul_mask)
#define PLL_MUL_MAX(layout)	(PLL_MUL_MASK(layout) + 1)
#define PLL_ICPR_SHIFT(id)	((id) * 16)
#define PLL_ICPR_MASK(id)	(0xffff << PLL_ICPR_SHIFT(id))
#define PLL_MAX_COUNT		0x3f
#define PLL_COUNT_SHIFT		8
#define PLL_OUT_SHIFT		14
#define PLL_MAX_ID		1

struct clk_pll {
	vaddr_t base;
	uint8_t id;
	uint8_t div;
	uint8_t range;
	uint16_t mul;
	const struct clk_pll_layout *layout;
	const struct clk_pll_characteristics *characteristics;
};

static inline bool clk_pll_ready(vaddr_t base, int id)
{
	unsigned int status = io_read32(base + AT91_PMC_SR);

	return status & PLL_STATUS_MASK(id) ? 1 : 0;
}

static TEE_Result clk_pll_enable(struct clk *clk)
{
	struct clk_pll *pll = clk->priv;
	const struct clk_pll_layout *layout = pll->layout;
	const struct clk_pll_characteristics *characteristics =
							pll->characteristics;
	uint8_t id = pll->id;
	uint32_t mask = PLL_STATUS_MASK(id);
	int offset = PLL_REG(id);
	uint8_t out = 0;
	unsigned int pllr;
	unsigned int status;
	uint8_t div;
	uint16_t mul;

	pllr = io_read32(pll->base + offset);
	div = PLL_DIV(pllr);
	mul = PLL_MUL(pllr, layout);

	status = io_read32(pll->base + AT91_PMC_SR);
	if ((status & mask) &&
	    (div == pll->div && mul == pll->mul))
		return TEE_SUCCESS;

	if (characteristics->out)
		out = characteristics->out[pll->range];

	if (characteristics->icpll)
		io_clrsetbits32(pll->base + AT91_PMC_PLLICPR, PLL_ICPR_MASK(id),
				characteristics->icpll[pll->range] << PLL_ICPR_SHIFT(id));

	io_clrsetbits32(pll->base + offset, layout->pllr_mask,
			pll->div | (PLL_MAX_COUNT << PLL_COUNT_SHIFT) |
			(out << PLL_OUT_SHIFT) |
			((pll->mul & layout->mul_mask) << layout->mul_shift));

	while (!clk_pll_ready(pll->base, pll->id))
		;

	return TEE_SUCCESS;
}

static void clk_pll_disable(struct clk *clk)
{
	struct clk_pll *pll = clk->priv;
	unsigned int mask = pll->layout->pllr_mask;

	io_clrsetbits32(pll->base + PLL_REG(pll->id), mask, ~mask);
}

static unsigned long clk_pll_get_rate(struct clk *clk,
				      unsigned long parent_rate)
{
	struct clk_pll *pll = clk->priv;

	if (!pll->div || !pll->mul)
		return 0;

	return (parent_rate / pll->div) * (pll->mul + 1);
}

static long clk_pll_get_best_div_mul(struct clk_pll *pll, unsigned long rate,
				     unsigned long parent_rate,
				     uint32_t *div, uint32_t *mul,
				     uint32_t *index)
{
	const struct clk_pll_layout *layout = pll->layout;
	const struct clk_pll_characteristics *characteristics =
							pll->characteristics;
	unsigned long bestremainder = ULONG_MAX;
	unsigned long maxdiv, mindiv, tmpdiv;
	long bestrate = -1;
	unsigned long bestdiv;
	unsigned long bestmul;
	int i = 0;

	/* Check if parent_rate is a valid input rate */
	if (parent_rate < characteristics->input.min)
		return -1;

	/*
	 * Calculate minimum divider based on the minimum multiplier, the
	 * parent_rate and the requested rate.
	 * Should always be 2 according to the input and output characteristics
	 * of the PLL blocks.
	 */
	mindiv = (parent_rate * PLL_MUL_MIN) / rate;
	if (!mindiv)
		mindiv = 1;

	if (parent_rate > characteristics->input.max) {
		tmpdiv = DIV_ROUND_UP(parent_rate, characteristics->input.max);
		if (tmpdiv > PLL_DIV_MAX)
			return -1;

		if (tmpdiv > mindiv)
			mindiv = tmpdiv;
	}

	/*
	 * Calculate the maximum divider which is limited by PLL register
	 * layout (limited by the MUL or DIV field size).
	 */
	maxdiv = DIV_ROUND_UP(parent_rate * PLL_MUL_MAX(layout), rate);
	if (maxdiv > PLL_DIV_MAX)
		maxdiv = PLL_DIV_MAX;

	/*
	 * Iterate over the acceptable divider values to find the best
	 * divider/multiplier pair (the one that generates the closest
	 * rate to the requested one).
	 */
	for (tmpdiv = mindiv; tmpdiv <= maxdiv; tmpdiv++) {
		unsigned long remainder;
		unsigned long tmprate;
		unsigned long tmpmul;

		/*
		 * Calculate the multiplier associated with the current
		 * divider that provide the closest rate to the requested one.
		 */
		tmpmul = UDIV_ROUND_NEAREST(rate, parent_rate / tmpdiv);
		tmprate = (parent_rate / tmpdiv) * tmpmul;
		if (tmprate > rate)
			remainder = tmprate - rate;
		else
			remainder = rate - tmprate;

		/*
		 * Compare the remainder with the best remainder found until
		 * now and elect a new best multiplier/divider pair if the
		 * current remainder is smaller than the best one.
		 */
		if (remainder < bestremainder) {
			bestremainder = remainder;
			bestdiv = tmpdiv;
			bestmul = tmpmul;
			bestrate = tmprate;
		}

		/*
		 * We've found a perfect match!
		 * Stop searching now and use this multiplier/divider pair.
		 */
		if (!remainder)
			break;
	}

	/* We haven't found any multiplier/divider pair => return -ERANGE */
	if (bestrate < 0)
		return bestrate;

	/* Check if bestrate is a valid output rate  */
	for (i = 0; i < characteristics->num_output; i++) {
		if (bestrate >= (long)characteristics->output[i].min &&
		    bestrate <= (long)characteristics->output[i].max)
			break;
	}

	if (i >= characteristics->num_output)
		return -1;

	if (div)
		*div = bestdiv;
	if (mul)
		*mul = bestmul - 1;
	if (index)
		*index = i;

	return bestrate;
}

static TEE_Result clk_pll_set_rate(struct clk *clk, unsigned long rate,
				   unsigned long parent_rate)
{
	struct clk_pll *pll = clk->priv;
	long ret;
	uint32_t div;
	uint32_t mul;
	uint32_t index;

	ret = clk_pll_get_best_div_mul(pll, rate, parent_rate,
				       &div, &mul, &index);
	if (ret < 0)
		return TEE_ERROR_BAD_PARAMETERS;

	pll->range = index;
	pll->div = div;
	pll->mul = mul;

	return TEE_SUCCESS;
}

static const struct clk_ops pll_ops = {
	.enable = clk_pll_enable,
	.disable = clk_pll_disable,
	.get_rate = clk_pll_get_rate,
	.set_rate = clk_pll_set_rate,
};

struct clk *
at91_clk_register_pll(struct pmc_data *pmc, const char *name,
		      struct clk *parent, uint8_t id,
		      const struct clk_pll_layout *layout,
		      const struct clk_pll_characteristics *characteristics)
{
	struct clk *clk;
	struct clk_pll *pll;
	int offset = PLL_REG(id);
	unsigned int pllr;

	if (!name || !parent)
		return NULL;

	clk = clk_alloc(name, &pll_ops, &parent, 1);
	if (!clk)
		return NULL;

	if (id > PLL_MAX_ID)
		return NULL;

	pll = malloc(sizeof(*pll));
	if (!pll) {
		clk_free(clk);
		return NULL;
	}

	pll->id = id;
	pll->layout = layout;
	pll->characteristics = characteristics;
	pll->base = pmc->base;
	pllr = io_read32(pmc->base + offset);
	pll->div = PLL_DIV(pllr);
	pll->mul = PLL_MUL(pllr, layout);

	clk->flags = CLK_SET_RATE_GATE;
	clk->priv = pll;

	if (clk_register(clk)) {
		clk_free(clk);
		free(pll);
		return NULL;
	}

	return clk;
}

const struct clk_pll_layout sama5d3_pll_layout = {
	.pllr_mask = 0x1FFFFFF,
	.mul_shift = 18,
	.mul_mask = 0x7F,
};
