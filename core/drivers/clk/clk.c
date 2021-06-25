// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Bootlin
 */

#include <drivers/clk.h>
#include <kernel/boot.h>
#include <kernel/panic.h>
#include <libfdt.h>
#include <mm/core_memprot.h>
#include <platform_config.h>
#include <stddef.h>

#include <drivers/scmi.h>

#define SCMI_MAX_CLK_NAME_LEN	16

static SLIST_HEAD(, clk) clk_list = SLIST_HEAD_INITIALIZER(clk_list);

struct clk *clk_alloc(const char *name, const struct clk_ops *ops,
		      struct clk **parent_clks, uint8_t parent_count)
{
	struct clk *clk;
	unsigned int parent;
	unsigned int alloc_size = sizeof(struct clk) +
				  parent_count * sizeof(struct clk *);

	clk = malloc(alloc_size);
	if (!clk)
		panic();

	memset(clk, 0, alloc_size);

	if (parent_count) {
		clk->parents = (struct clk **)(clk + 1);
		clk->num_parents = parent_count;
		for (parent = 0; parent < parent_count; parent++)
			clk->parents[parent] = parent_clks[parent];
	}

	clk->name = name;
	clk->ops = ops;
	clk->scmi_id = CLK_NO_SCMI_ID;

	return clk;
}

void clk_free(struct clk *clk)
{
	free(clk);
}

static int clk_check(struct clk *clk)
{
	if (clk->ops->set_parent && !clk->ops->get_parent)
		return 1;

	if (clk->num_parents > 1 && !clk->ops->get_parent)
		return 1;

	if ((clk->flags & CLK_SET_RATE_PARENT) && !clk->num_parents)
		return 1;

	return 0;
}

static void clk_compute_rate(struct clk *clk)
{
	unsigned long parent_rate = 0;

	clk->rate = 0;

	if (clk->parent)
		parent_rate = clk->parent->rate;

	if (clk->ops->get_rate)
		clk->rate = clk->ops->get_rate(clk, parent_rate);
	else
		clk->rate = parent_rate;
}

uint8_t clk_get_num_parents(struct clk *clk)
{
	return clk->num_parents;
}

struct clk *clk_get_parent_by_index(struct clk *clk, unsigned int pidx)
{
	if (pidx >= clk->num_parents)
		return NULL;

	return clk->parents[pidx];
}

static void clk_init_parent(struct clk *clk)
{
	uint8_t pidx;

	if (clk->num_parents > 1) {
		pidx = clk->ops->get_parent(clk);
		if (pidx >= clk->num_parents) {
			EMSG("get_parent returned an invalid value\n");
			return;
		}

		clk->parent = clk->parents[pidx];
	} else {
		if (clk->num_parents)
			clk->parent = clk->parents[0];
	}
}

int clk_register(struct clk *clk)
{
	assert(!clk_check(clk));

	SLIST_INSERT_HEAD(&clk_list, clk, link);

	clk_init_parent(clk);
	clk_compute_rate(clk);

	if (clk->ops->is_enabled)
		clk->enabled = clk->ops->is_enabled(clk);
	else
		clk->enabled = 0;

	DMSG("Registered clock %s, freq %ld\n", clk->name, clk_get_rate(clk));

	return 0;
}

unsigned long clk_get_rate(struct clk *clk)
{
	if (!clk)
		return 0;

	return clk->rate;
}

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	int was_enabled;
	unsigned long parent_rate = 0;

	if (clk->flags & CLK_SET_RATE_GATE) {
		was_enabled = clk->enabled;
		if (was_enabled)
			clk_disable(clk);
	}

	/*
	 * No need to check for clk->parent since this is check at clock
	 * registration
	 */
	if (clk->flags & CLK_SET_RATE_PARENT)
		clk_set_rate(clk->parent, rate);

	if (clk->ops->set_rate) {
		if (clk->parent)
			parent_rate = clk->parent->rate;

		if (clk->ops->set_rate(clk, rate, parent_rate))
			return -1;

		return 0;
	}

	clk_compute_rate(clk);

	if (clk->flags & CLK_SET_RATE_GATE) {
		if (was_enabled)
			clk_enable(clk);
	}

	return -1;
}

int clk_enable(struct clk *clk)
{
	struct clk *parent;

	if (clk->enabled)
		return 0;

	parent = clk_get_parent(clk);
	if (parent)
		clk_enable(parent);

	if (clk->ops->enable) {
		if (clk->ops->enable(clk))
			return -1;
	}

	clk->enabled = 1;

	return 0;
}

void clk_disable(struct clk *clk)
{
	if (!clk->enabled)
		return;

	if (clk->ops->disable)
		clk->ops->disable(clk);

	clk->enabled = 0;
}

int clk_is_enabled(struct clk *clk)
{
	return clk->enabled;
}

int clk_set_parent(struct clk *clk, unsigned int pidx)
{
	int ret = 0;
	int was_enabled;

	if (pidx >= clk->num_parents || !clk->ops->set_parent)
		return -1;

	if (clk->flags & CLK_SET_PARENT_GATE) {
		was_enabled = clk->enabled;
		if (was_enabled)
			clk_disable(clk);
	}

	if (clk->ops->set_parent(clk, pidx)) {
		ret = -1;
		goto err_out;
	}

	clk->parent = clk->parents[pidx];

	if (clk->enabled)
		clk_enable(clk->parent);

	/* The parent changed and the rate might also change */
	clk_compute_rate(clk);

err_out:
	if (clk->flags & CLK_SET_PARENT_GATE) {
		if (was_enabled)
			clk_disable(clk);
	}

	return ret;
}

struct clk *clk_get_parent(struct clk *clk)
{
	return clk->parent;
}

#if defined(CFG_SCMI_MSG_DRIVERS)

static int clk_check_scmi_id(struct clk *clk, int scmi_id)
{
	struct clk *tclk;

	if (scmi_id == CLK_NO_SCMI_ID)
		return 0;

	SLIST_FOREACH(tclk, &clk_list, link) {
		if (clk->scmi_channel_id == tclk->scmi_channel_id &&
		    tclk->scmi_id == scmi_id) {
			EMSG("Clock for SCMI channel %d, id %d already registered !\n",
			     clk->scmi_channel_id, clk->scmi_id);
			return 1;
		}
	}

	if (strlen(clk->name) >= SCMI_MAX_CLK_NAME_LEN)
		return 1;

	return 0;
}

struct clk *clk_scmi_get_by_id(unsigned int scmi_channel_id, unsigned int scmi_id)
{
	struct clk *clk;

	SLIST_FOREACH(clk, &clk_list, link) {
		if (clk->scmi_id == (int)scmi_id)
			return clk;
	}

	return NULL;
}

unsigned int clk_scmi_get_count(unsigned int channel_id)
{
	struct clk *clk;
	int max_id = -1;

	SLIST_FOREACH(clk, &clk_list, link) {
		if (clk->scmi_channel_id != channel_id)
			continue;
		if (clk->scmi_id > max_id)
			max_id = clk->scmi_id;
	}

	return max_id + 1;
}

int clk_scmi_set_ids(struct clk *clk, unsigned int channel_id,
		     unsigned int scmi_id)
{
	int ret;

	ret = clk_check_scmi_id(clk, scmi_id);
	if (ret)
		return ret;

	clk->scmi_channel_id = channel_id;
	clk->scmi_id = scmi_id;
	return 0;
}

static int clk_scmi_dummy_rates(struct clk *clk, size_t start_index,
				unsigned long *rates, size_t *nb_elts)
{
	if (start_index)
		return SCMI_GENERIC_ERROR;

	if (!rates) {
		*nb_elts = 1;
		return SCMI_SUCCESS;
	}

	if (*nb_elts != 1)
		return SCMI_GENERIC_ERROR;

	rates[0] = clk_get_rate(clk);

	return SCMI_SUCCESS;
}

int clk_scmi_get_rates_array(struct clk *clk, size_t start_index,
			     unsigned long *rates, size_t *nb_elts)
{
	if (clk->flags & CLK_SET_RATE_PARENT)
		clk = clk->parent;

	/* Simply return the clock rate */
	if (!clk->ops->get_rates_array)
		return clk_scmi_dummy_rates(clk, start_index, rates, nb_elts);

	if (clk->ops->get_rates_array(clk, start_index, rates, nb_elts))
		return SCMI_GENERIC_ERROR;

	return SCMI_SUCCESS;
}
#endif
