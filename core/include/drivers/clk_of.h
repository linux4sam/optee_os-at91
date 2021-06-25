/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021, Bootlin
 */

#ifndef CLK_OF_H
#define CLK_OF_H

#include <kernel/dt.h>
#include <stdint.h>
#include <sys/queue.h>

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

TEE_Result clk_of_register_clk_provider(void *fdt, int nodeoffset,
					struct clk *(*get_of_clk)(struct clk_of_phandle_args *args, void *data),
					void *data);

struct clk *clk_of_get_simple_clk(struct clk_of_phandle_args *args, void *data);


#endif /* CLK_OF_H */
