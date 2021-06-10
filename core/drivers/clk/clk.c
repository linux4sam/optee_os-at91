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
	struct clk *clk = NULL;
	unsigned int parent = 0;
	unsigned int alloc_size = sizeof(struct clk) +
				  parent_count * sizeof(struct clk *);

	clk = calloc(1, alloc_size);
	if (!clk)
		return NULL;

	if (parent_count) {
		clk->parents = (struct clk **)(clk + 1);
		clk->num_parents = parent_count;
		for (parent = 0; parent < parent_count; parent++)
			clk->parents[parent] = parent_clks[parent];
	}

	clk->name = name;
	clk->ops = ops;

	return clk;
}

void clk_free(struct clk *clk)
{
	free(clk);
}

static int clk_check(struct clk *clk)
{
	if (!clk->ops)
		return 1;

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
	uint8_t pidx = 0;

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

TEE_Result clk_register(struct clk *clk)
{
	if (clk_check(clk))
		return TEE_ERROR_BAD_PARAMETERS;

	SLIST_INSERT_HEAD(&clk_list, clk, link);

	clk_init_parent(clk);
	clk_compute_rate(clk);

	DMSG("Registered clock %s, freq %ld\n", clk->name, clk_get_rate(clk));

	return TEE_SUCCESS;
}

unsigned long clk_get_rate(struct clk *clk)
{
	return clk->rate;
}

static TEE_Result clk_enable_no_count(struct clk *clk)
{
	if (clk->ops->enable) {
		if (clk->ops->enable(clk))
			return TEE_ERROR_GENERIC;
	}

	return TEE_SUCCESS;
}

TEE_Result clk_enable(struct clk *clk)
{
	int ret;
	struct clk *parent;

	if (clk->enabled_count == ULONG_MAX)
		return TEE_ERROR_OVERFLOW;

	if (clk->enabled_count) {
		clk->enabled_count++;
		return TEE_SUCCESS;
	}

	parent = clk_get_parent(clk);
	if (parent)
		clk_enable(parent);

	ret = clk_enable_no_count(clk);
	if (ret == TEE_SUCCESS)
		clk->enabled_count = 1;

	return ret;
}

static void clk_disable_no_count(struct clk *clk)
{
	if (clk->ops->disable)
		clk->ops->disable(clk);
}

void clk_disable(struct clk *clk)
{
	struct clk *parent;

	if (clk->enabled_count == 0) {
		EMSG("Unbalanced clk_enable/disable");
		return;
	}

	if (clk->enabled_count) {
		clk->enabled_count--;
		if (clk->enabled_count)
			return;
	}

	clk_disable_no_count(clk);

	parent = clk_get_parent(clk);
	if (parent)
		clk_disable(parent);

	clk->enabled_count = 0;
}

int clk_is_enabled(struct clk *clk)
{
	if (clk->ops->is_enabled)
		return clk->ops->is_enabled(clk);

	return 0;
}

TEE_Result clk_set_rate(struct clk *clk, unsigned long rate)
{
	int was_enabled = 0;
	unsigned long parent_rate = 0;

	if (clk->flags & CLK_SET_RATE_GATE) {
		was_enabled = clk_is_enabled(clk);
		if (was_enabled)
			clk_disable_no_count(clk);
	}

	/*
	 * No need to check for clk->parent since this is checked at clock
	 * registration
	 */
	if (clk->flags & CLK_SET_RATE_PARENT)
		clk_set_rate(clk->parent, rate);

	if (clk->ops->set_rate) {
		if (clk->parent)
			parent_rate = clk->parent->rate;

		if (clk->ops->set_rate(clk, rate, parent_rate))
			return TEE_ERROR_GENERIC;
	}

	if (clk->flags & CLK_SET_RATE_GATE && was_enabled)
		clk_enable_no_count(clk);

	clk_compute_rate(clk);

	return TEE_SUCCESS;
}

TEE_Result clk_set_parent(struct clk *clk, unsigned int pidx)
{
	int ret = TEE_SUCCESS;
	int was_enabled = 0;

	if (pidx >= clk->num_parents || !clk->ops->set_parent)
		return TEE_ERROR_BAD_PARAMETERS;

	/* Requested parent is already the one set */
	if (clk->parent == clk->parents[pidx])
		return TEE_SUCCESS;

	if (clk->flags & CLK_SET_PARENT_GATE) {
		was_enabled = clk->enabled_count;
		if (was_enabled)
			clk_disable(clk);
	}

	if (clk->ops->set_parent(clk, pidx)) {
		ret = TEE_ERROR_GENERIC;
		goto err_out;
	}

	clk->parent = clk->parents[pidx];

	if ((clk->flags & CLK_SET_PARENT_GATE) && was_enabled)
		clk_enable(clk->parent);

	/* The parent changed and the rate might also change */
	clk_compute_rate(clk);

err_out:
	if (clk->flags & CLK_SET_PARENT_GATE && was_enabled)
		clk_enable(clk);

	return ret;
}

struct clk *clk_get_parent(struct clk *clk)
{
	return clk->parent;
}
