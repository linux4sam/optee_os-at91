/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021, Bootlin
 */

#ifndef CLK_H
#define CLK_H

#include <kernel/dt.h>
#include <stdint.h>
#include <sys/queue.h>

#define CLK_NO_SCMI_ID	-1

struct clk;

/**
 * struct clk_ops
 *
 * @enable: Enable the clock
 * @disable: Disable the clock
 * @is_enabled: Check if the clock is enabled
 * @set_parent: Set the clock parent based on index
 * @get_parent: Get the current parent index of the clock
 * @set_rate: Set the clock rate
 * @get_rate: Get the clock rate
 * @get_rates_array: Get the supported clock rates as array
 */
struct clk_ops {
	int (*enable)(struct clk *clk);
	void (*disable)(struct clk *clk);
	int (*is_enabled)(struct clk *clk);
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
#define CLK_SET_RATE_PARENT	BIT(2) /* propagate rate change up one level */

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
	int scmi_id;
	unsigned int scmi_channel_id;
	unsigned int flags;
	uint8_t enabled;
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
int clk_register(struct clk *clk);

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
int clk_set_rate(struct clk *clk, unsigned long rate);

/**
 * clk_enable - Enable a clock
 *
 * @clk: Clock to be enabled
 * Returns 0 on success or a negative value on error.
 *
 * Note: This function will also enable parents if needed.
 */
int clk_enable(struct clk *clk);

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
int clk_is_enabled(struct clk *clk);

/**
 * clk_get_parent - Get the current clock parent
 *
 * @clk: Clock for which the parent is needed
 * Return the clock parent or NULL if there is no parent
 */
struct clk *clk_get_parent(struct clk *clk);

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
int clk_set_parent(struct clk *clk, unsigned int pidx);

#if defined(CFG_SCMI_MSG_DRIVERS)

/**
 * clk_scmi_get_by_id - Get a clock by its SCMI id
 *
 * @channel_id: SCMI channel id for which the clock is needed
 * @scmi_id: SCMI id for which the clock is needed
 * Return a clock struct matching the SCMI ID or NULL if any.
 */
struct clk *clk_scmi_get_by_id(unsigned int channel_id, unsigned int scmi_id);

/**
 * clk_scmi_get_count - Return the count of SCMI clocks available
 * @channel_id: SCMI channel id for which the count of clock is needed
 */
unsigned int clk_scmi_get_count(unsigned int channel_id);

/**
 * clk_scmi_set_id - Set a clock SCMI ID
 *
 * @clk: Clock to be assigned the SCMI ID
 * @channel_id: SCMI channel ID to be associated with the clock
 * @scmi_id: SCMI ID to be associated with the clock
 * Returns 0 on success or a negative value if duplicated ID
 */
int clk_scmi_set_ids(struct clk *clk, unsigned int channel_id,
		     unsigned int scmi_id);

/**
 * clk_scmi_get_rates_array - Get rates array for SCMI clocks
 *
 * @clk: Clock for which the rates are needed
 * @start_index: Start index of the rate query
 * @rates: Array of rates that are filled by the function
 * @nb_elts: Maximum number of rates on input, filled with number of rates
 *	     that have been filled on output.
 * Returns 0 on success or a negative value on error.
 */
int clk_scmi_get_rates_array(struct clk *clk, size_t start_index,
			     unsigned long *rates, size_t *nb_elts);

#endif

#if defined(CFG_DT)

#define MAX_CLK_PHANDLE_ARGS 3
/**
 * struct clk_of_phandle_args - Devicetree clock args
 * @nodeoffset: Clock consumer node offset
 * @args_count: Count of cells for the clock
 * @args: Clocks consumer specifiers
 */
struct clk_of_phandle_args {
	int nodeoffset;
	int args_count;
	uint32_t args[MAX_CLK_PHANDLE_ARGS];
};

struct clk_driver {
	void (*setup)(void *fdt, int nodeoffset);
};

#define CLK_OF_DECLARE(__fn, __compat, __init) \
	static const struct clk_driver __fn ## _driver = { \
		.setup = __init, \
	}; \
	static const struct dt_device_match __fn ## _match_table[] = { \
		{ .compatible = __compat }, \
		{ 0 } \
	}; \
	const struct dt_driver __fn ## _dt_driver __dt_driver = { \
		.name = # __fn, \
		.type = DT_DRIVER_CLK, \
		.match_table = __fn ## _match_table, \
		.driver = &__fn ## _driver, \
	}

int clk_of_get_idx_by_name(void *fdt, int nodeoffset, const char *name);
struct clk *clk_of_get_by_idx(void *fdt, int nodeoffset, int clk_idx);
struct clk *clk_of_get_by_name(void *fdt, int nodeoffset,
			       const char *name);

int clk_of_register_clk_provider(int nodeoffset,
		struct clk *(*get_of_clk)(struct clk_of_phandle_args *args, void *data),
		void *data);

struct clk *clk_of_get_simple_clk(struct clk_of_phandle_args *args, void *data);

#if defined(CFG_SCMI_MSG_DRIVERS)

int clk_scmi_update_dt(uint32_t *scmi_clk_phandles, uint32_t scmi_chan_count);

#endif
#endif

#endif /* CLK_H */
