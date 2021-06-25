/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021, Bootlin
 */

#ifndef CLK_DT_H
#define CLK_DT_H

#include <kernel/dt.h>
#include <stdint.h>
#include <sys/queue.h>

#if defined(CFG_DT) && defined(CFG_GENERIC_CLK)

#define MAX_CLK_PHANDLE_ARGS 3

/**
 * struct clk_dt_phandle_args - Devicetree clock args
 * @nodeoffset: Clock consumer node offset
 * @args_count: Count of cells for the clock
 * @args: Clocks consumer specifiers
 */
struct clk_dt_phandle_args {
	int nodeoffset;
	int args_count;
	uint32_t args[MAX_CLK_PHANDLE_ARGS];
};

/**
 * struct clk_driver - clk driver setup struct 
 * setup:
 */
struct clk_driver {
	TEE_Result (*setup)(const void *fdt, int nodeoffset);
};

/**
 * CLK_DT_DECLARE - Declare a clock driver 
 * @__name: Clock driver name
 * @__compat: Compatible string
 * @__init: Clock setup function
 */
#define CLK_DT_DECLARE(__name, __compat, __init) \
	static const struct clk_driver __name ## _driver = { \
		.setup = __init, \
	}; \
	static const struct dt_device_match __name ## _match_table[] = { \
		{ .compatible = __compat }, \
		{ 0 } \
	}; \
	const struct dt_driver __name ## _dt_driver __dt_driver = { \
		.name = # __name, \
		.type = DT_DRIVER_CLK, \
		.match_table = __name ## _match_table, \
		.driver = &__name ## _driver, \
	}

/**
 * clk_dt_get_by_idx - Get a clock at a specific index in "clocks" proeprty
 * 
 * @fdt: Device tree to work on
 * @nodeoffset: Node offset of the subnode containing a clock property
 * @clk_idx: Clock index to get
 * Returns a clk struct matching the clock at index clk_idx in clocks property
 */
struct clk *clk_dt_get_by_idx(const void *fdt, int nodeoffset, int clk_idx);

/**
 * clk_dt_get_by_name - Get a clock matching a name in the "clock-names"
 * property
 * 
 * @fdt: Device tree to work on
 * @nodeoffset: Node offset of the subnode containing a clock property
 * @name: Clock name to get
 * Returns a clk struct matching the name in "clock-names" property
 */
struct clk *clk_dt_get_by_name(const void *fdt, int nodeoffset,
			       const char *name);

/**
 * clk_dt_register_clk_provider - Register a clock provider
 * 
 * @fdt: Device tree to work on
 * @nodeoffset: Node offset of the clock
 * @get_dt_clk: Callback to match the devicetree clock with a clock struct
 * @data: Data which will be passed to the get_dt_clk callback
 * Returns TEE_Result 
 */
TEE_Result clk_dt_register_clk_provider(const void *fdt, int nodeoffset,
					struct clk *(*get_dt_clk)(struct clk_dt_phandle_args *args, void *data),
					void *data);

/**
 * clk_dt_get_simple_clk: simple clock matching function for mono clock providers
 */
struct clk *clk_dt_get_simple_clk(struct clk_dt_phandle_args *args, void *data);

/**
 * clk_dt_probe - Probe the clock tree from device tree
 * 
 * Returns a TEE_Result value reflecting the probe status
 */
TEE_Result clk_dt_probe(void);
#else
static inline TEE_Result clk_dt_probe(void)
{
	return TEE_SUCCESS;
}
#endif /* !CFG_DT */

#endif /* CLK_DT_H */
