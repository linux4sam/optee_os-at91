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

#define SAM9X5_USB_DIV_SHIFT	8
#define	SAM9X5_USB_DIV_COUNT	BIT32(4)
#define SAM9X5_USB_MAX_DIV	(SAM9X5_USB_DIV_COUNT - 1)

#define SAM9X5_USBS_MASK	GENMASK_32(0, 0)

struct at91sam9x5_clk_usb {
	vaddr_t base;
	uint32_t usbs_mask;
};

static unsigned long at91sam9x5_clk_usb_get_rate(struct clk *clk,
						 unsigned long parent_rate)
{
	struct at91sam9x5_clk_usb *usb = clk->priv;
	uint8_t usbdiv;
	unsigned int usbr = io_read32(usb->base + AT91_PMC_USB);

	usbdiv = (usbr & AT91_PMC_OHCIUSBDIV) >> SAM9X5_USB_DIV_SHIFT;

	return UDIV_ROUND_NEAREST(parent_rate, (usbdiv + 1));
}

static int at91sam9x5_clk_usb_set_parent(struct clk *clk, uint8_t index)
{
	struct at91sam9x5_clk_usb *usb = clk->priv;

	if (index >= clk_get_num_parents(clk))
		return -1;

	io_clrsetbits32(usb->base + AT91_PMC_USB, usb->usbs_mask, index);

	return 0;
}

static uint8_t at91sam9x5_clk_usb_get_parent(struct clk *clk)
{
	struct at91sam9x5_clk_usb *usb = clk->priv;
	unsigned int usbr = io_read32(usb->base + AT91_PMC_USB);

	return usbr & usb->usbs_mask;
}

static int at91sam9x5_clk_usb_set_rate(struct clk *clk, unsigned long rate,
				       unsigned long parent_rate)
{
	struct at91sam9x5_clk_usb *usb = clk->priv;
	unsigned long div;

	if (!rate)
		return -1;

	div = UDIV_ROUND_NEAREST(parent_rate, rate);
	if (div > SAM9X5_USB_MAX_DIV + 1 || !div)
		return -1;

	io_clrsetbits32(usb->base + AT91_PMC_USB, AT91_PMC_OHCIUSBDIV,
			(div - 1) << SAM9X5_USB_DIV_SHIFT);

	return 0;
}

static int at91sam9x5_clk_usb_get_rates_array(struct clk *clk,
					      size_t start_index,
					      unsigned long *rates,
					      size_t *nb_elts)
{
	int div;
	unsigned int rate_idx = 0;
	unsigned long parent_rate = 0;
	struct clk *parent = clk_get_parent(clk);

	if (!rates) {
		*nb_elts = SAM9X5_USB_DIV_COUNT;
		return 0;
	}

	parent_rate = clk_get_rate(parent);

	for (div = SAM9X5_USB_MAX_DIV; div >= 0; div--) {
		if (start_index == 0) {
			rates[rate_idx] = parent_rate / (div + 1);
			rate_idx++;
			if (rate_idx == *nb_elts)
				return 0;
		} else {
			start_index--;
		}
	}

	*nb_elts = rate_idx;

	return 0;
}

static const struct clk_ops at91sam9x5_usb_ops = {
	.get_rate = at91sam9x5_clk_usb_get_rate,
	.get_parent = at91sam9x5_clk_usb_get_parent,
	.set_parent = at91sam9x5_clk_usb_set_parent,
	.set_rate = at91sam9x5_clk_usb_set_rate,
	.get_rates_array = at91sam9x5_clk_usb_get_rates_array,
};

static struct clk *
_at91sam9x5_clk_register_usb(struct pmc_data *pmc, const char *name,
			     struct clk **parents, uint8_t num_parents,
			     uint32_t usbs_mask)
{
	struct at91sam9x5_clk_usb *usb;
	struct clk *clk;

	clk = clk_alloc(name, &at91sam9x5_usb_ops, parents, num_parents);
	if (!clk)
		return NULL;

	usb = malloc(sizeof(*usb));
	if (!usb)
		return NULL;

	usb->base = pmc->base;
	usb->usbs_mask = usbs_mask;

	clk->priv = usb;
	clk->flags = CLK_SET_RATE_GATE | CLK_SET_PARENT_GATE |
		     CLK_SET_RATE_PARENT;

	if (clk_register(clk)) {
		clk_free(clk);
		free(usb);
		return NULL;
	}

	return clk;
}

struct clk *
at91sam9x5_clk_register_usb(struct pmc_data *pmc, const char *name,
			    struct clk **parents, uint8_t num_parents)
{
	return _at91sam9x5_clk_register_usb(pmc, name, parents,
					    num_parents, SAM9X5_USBS_MASK);
}
