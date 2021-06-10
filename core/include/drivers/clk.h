/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021, Bootlin
 */

#ifndef CLK_H
#define CLK_H

#include <kernel/dt.h>
#include <kernel/refcount.h>
#include <kernel/spinlock.h>
#include <stdint.h>
#include <sys/queue.h>
#include <tee_api_types.h>

#define CLK_NO_SCMI_ID	-1

struct clk;

/**
 * struct clk_ops
 *
 * @enable: Enable the clock
 * @disable: Disable the clock
 * @set_parent: Set the clock parent based on index
 * @get_parent: Get the current parent index of the clock
 * @set_rate: Set the clock rate
 * @get_rate: Get the clock rate
 * @get_rates_array: Get the supported clock rates as array
 */
struct clk_ops {
	int (*enable)(struct clk *clk);
	void (*disable)(struct clk *clk);
	int (*set_parent)(struct clk *clk, uint8_t index);
	uint8_t (*get_parent)(struct clk *clk);
	int (*set_rate)(struct clk *clk, unsigned long rate,
			unsigned long parent_rate);
	unsigned long (*get_rate)(struct clk *clk,
				  unsigned long parent_rate);
	int (*get_rates_array)(struct clk *clk, size_t start_index,
			       unsigned long *rates, size_t *nb_elts);
};

/* Flags for clock */
#define CLK_SET_RATE_GATE	BIT(0) /* must be gated across rate change */
#define CLK_SET_PARENT_GATE	BIT(1) /* must be gated across re-parent */

/**
 * struct clk - Clock structure
 *
 * @name: Clock name
 * @priv: private data for the clock provider
 * @ops: Clock operations
 * @parents: Array of parents of the clock
 * @parent: Current parent
 * @num_parents: Number of parents
 * @rate: Current clock rate (cached after init or rate change)
 * @scmi_id: SCMI identifier for the clock (used by the generic SCMI clock
 *	     support)
 * @scmi_channel_id: SCMI channel for the clock (used by the generic SCMI clock
 *	     support)
 * @flags: Specific clock flags
 * @enabled: Clock state (cached)
 * @link: Entry in the linked list of clocks
 */
struct clk {
	const char *name;
	void *priv;
	const struct clk_ops *ops;
	struct clk **parents;
	struct clk *parent;
	uint8_t num_parents;
	unsigned long rate;
	unsigned int flags;
	unsigned int lock;
	struct refcount enabled_count;
	SLIST_ENTRY(clk) link;
};

/**
 * Return the clock name
 *
 * @clk: Clock for which the name is needed
 * Return a const char* pointing to the clock name
 */
static inline const char *clk_get_name(struct clk *clk)
{
	return clk->name;
}

/**
 * clk_alloc - Allocate a clock structure
 *
 * @name: Clock name
 * @ops: Clock operations
 * @parent_clks: Parents of the clock
 * @parent_count: Number of parents of the clock
 *
 * Returns a clock struct properly initialized or NULL if allocation failed
 */
struct clk *clk_alloc(const char *name, const struct clk_ops *ops,
		      struct clk **parent_clks, uint8_t parent_count);

/**
 * clk_free - Free a clock structure
 *
 * @clk: Clock to be freed
 */
void clk_free(struct clk *clk);

/**
 * clk_register - Register a clock with the clock framework
 *
 * @clk: Clock struct to be registered
 * Returns 0 on success or a negative value on error.
 */
TEE_Result clk_register(struct clk *clk);

/**
 * clk_get_rate - Get clock rate
 *
 * @clk: Clock for which the rate is needed
 * Returns the clock rate in Hz
 */
unsigned long clk_get_rate(struct clk *clk);

/**
 * clk_set_rate - Set a clock rate
 *
 * @clk: Clock to be set with the rate
 * @rate: Rate to set
 * Returns 0 on success or a negative value on error.
 */
TEE_Result clk_set_rate(struct clk *clk, unsigned long rate);

/**
 * clk_enable - Enable a clock
 *
 * @clk: Clock to be enabled
 * Returns 0 on success or a negative value on error.
 *
 * Note: This function will also enable parents if needed.
 */
TEE_Result clk_enable(struct clk *clk);

/**
 * clk_disable - Disable a clock
 *
 * @clk: Clock to be disabled
 * Returns 0 on success or a negative value on error.
 */
void clk_disable(struct clk *clk);

/**
 * clk_is_enabled - Check if a clock is enabled
 *
 * @clk: Clock to be disabled
 * Returns 0 on success or a negative value on error.
 */

static inline int clk_is_enabled(struct clk *clk)
{
	return refcount_val(&clk->enabled_count);
}

/**
 * clk_get_parent - Get the current clock parent
 *
 * @clk: Clock for which the parent is needed
 * Return the clock parent or NULL if there is no parent
 */
static inline struct clk *clk_get_parent(struct clk *clk)
{
	return clk->parent;
}

/**
 * clk_get_num_parents - Get the number of parent for a clock
 *
 * @clk: Clock for which the number of parents is needed
 * Return the number of parents
 */
uint8_t clk_get_num_parents(struct clk *clk);

/**
 * Get a clock parent by its index
 *
 * @clk: Clock for which the parent is needed
 * @pidx: parent index for the clock
 * Return the clock parent at index pidx or NULL if out of bound
 */
struct clk *clk_get_parent_by_index(struct clk *clk, unsigned int pidx);

/**
 * clk_set_parent - Set the current clock parent
 *
 * @clk: Clock for which the parent should be set
 * @pidx: parent index to set
 */
TEE_Result clk_set_parent(struct clk *clk, unsigned int pidx);

#endif /* CLK_H */
