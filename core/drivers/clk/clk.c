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
	size_t alloc_size = sizeof(struct clk) +
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
	clk->lock = SPINLOCK_UNLOCK;
	refcount_set(&clk->enabled_count, 0);

	return clk;
}

void clk_free(struct clk *clk)
{
	free(clk);
}

static bool clk_check(struct clk *clk)
{
	if (!clk->ops)
		return false;

	if (clk->ops->set_parent && !clk->ops->get_parent)
		return false;

	if (clk->num_parents > 1 && !clk->ops->get_parent)
		return false;

	return true;
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

	switch (clk->num_parents) {
	case 0:
		break;
	case 1:
		clk->parent = clk->parents[0];
		break;
	default:
		pidx = clk->ops->get_parent(clk);
		if (pidx >= clk->num_parents) {
			EMSG("get_parent returned an invalid value");
			panic();
		}

		clk->parent = clk->parents[pidx];
		break;
	}
}

TEE_Result clk_register(struct clk *clk)
{
	if (!clk_check(clk))
		return TEE_ERROR_BAD_PARAMETERS;

	SLIST_INSERT_HEAD(&clk_list, clk, link);

	clk_init_parent(clk);
	clk_compute_rate(clk);

	DMSG("Registered clock %s, freq %ld", clk->name, clk_get_rate(clk));

	return TEE_SUCCESS;
}

unsigned long clk_get_rate(struct clk *clk)
{
	return clk->rate;
}

static TEE_Result clk_enable_no_lock(struct clk *clk)
{
	struct clk *parent;

	if (refcount_inc(&clk->enabled_count))
		return TEE_SUCCESS;

	parent = clk_get_parent(clk);
	if (parent)
		clk_enable(parent);

	if (clk->ops->enable) {
		if (clk->ops->enable(clk))
			return TEE_ERROR_GENERIC;
	}

	refcount_set(&clk->enabled_count, 1);

	return TEE_SUCCESS;
}

TEE_Result clk_enable(struct clk *clk)
{
	TEE_Result ret;

	cpu_spin_lock(&clk->lock);
	ret = clk_enable_no_lock(clk);
	cpu_spin_unlock(&clk->lock);

	return ret;
}

static void clk_disable_no_lock(struct clk *clk)
{
	struct clk *parent;

	if (!refcount_dec(&clk->enabled_count))
		return;

	if (clk->ops->disable)
		clk->ops->disable(clk);

	parent = clk_get_parent(clk);
	if (parent)
		clk_disable(parent);
}

void clk_disable(struct clk *clk)
{
	cpu_spin_lock(&clk->lock);
	clk_disable_no_lock(clk);
	cpu_spin_unlock(&clk->lock);
}

static TEE_Result clk_set_rate_no_lock(struct clk *clk, unsigned long rate)
{
	unsigned long parent_rate = 0;

	if (clk->ops->set_rate) {
		if (clk->parent)
			parent_rate = clk->parent->rate;

		if (clk->ops->set_rate(clk, rate, parent_rate))
			return TEE_ERROR_GENERIC;
	}

	clk_compute_rate(clk);

	return TEE_SUCCESS;
}

TEE_Result clk_set_rate(struct clk *clk, unsigned long rate)
{
	TEE_Result ret = TEE_SUCCESS;

	cpu_spin_lock(&clk->lock);
	if (rate == clk->rate)
		goto out;

	if (clk->flags & CLK_SET_RATE_GATE) {
		if (clk_is_enabled(clk)) {
			ret = TEE_ERROR_BAD_STATE;
			goto out;
		}
	}

	ret = clk_set_rate_no_lock(clk, rate);
out:
	cpu_spin_unlock(&clk->lock);

	return ret;
}

static TEE_Result clk_set_parent_no_lock(struct clk *clk, unsigned int pidx)
{
	TEE_Result ret = TEE_SUCCESS;
	bool was_enabled = false;

	/* Requested parent is already the one set */
	if (clk->parent == clk->parents[pidx])
		return TEE_SUCCESS;

	was_enabled = clk_is_enabled(clk);
	if (was_enabled)
		clk_disable_no_lock(clk);

	if (clk->ops->set_parent(clk, pidx)) {
		ret = TEE_ERROR_GENERIC;
		goto out;
	}

	clk->parent = clk->parents[pidx];

	/* The parent changed and the rate might also have changed */
	clk_compute_rate(clk);

out:
	if (was_enabled)
		clk_enable_no_lock(clk);
	
	return ret;
}

TEE_Result clk_set_parent(struct clk *clk, unsigned int pidx)
{
	TEE_Result ret = TEE_SUCCESS;

	if (pidx >= clk->num_parents || !clk->ops->set_parent)
		return TEE_ERROR_BAD_PARAMETERS;

	cpu_spin_lock(&clk->lock);
	if (clk->flags & CLK_SET_PARENT_GATE) {
		if (clk_is_enabled(clk)) {
			ret = TEE_ERROR_BAD_STATE;
			goto out;
		}
	}

	ret = clk_set_parent_no_lock(clk, pidx);
out:
	cpu_spin_unlock(&clk->lock);

	return ret;
}
