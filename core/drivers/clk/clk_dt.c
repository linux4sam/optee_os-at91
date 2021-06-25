// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Bootlin
 */

#include <drivers/clk.h>
#include <drivers/clk_dt.h>
#include <initcall.h>
#include <kernel/boot.h>
#include <kernel/panic.h>
#include <libfdt.h>
#include <stddef.h>

struct clk_dt_provider {
	int nodeoffset;
	unsigned int clock_cells;
	uint32_t phandle;
	struct clk *(*get_of_clk)(struct clk_dt_phandle_args *args, void *data);
	void *data;
	SLIST_ENTRY(clk_dt_provider) link;
};

static SLIST_HEAD(, clk_dt_provider) clk_dt_provider_list =
				SLIST_HEAD_INITIALIZER(clk_dt_provider_list);

static uint32_t fdt_clock_cells(const void *fdt, int nodeoffset)
{
	const fdt32_t *c = NULL;
	int len = 0;

	c = fdt_getprop(fdt, nodeoffset, "#clock-cells", &len);
	if (!c)
		return len;

	if (len != sizeof(*c))
		return -FDT_ERR_BADNCELLS;

	return fdt32_to_cpu(*c);
}

TEE_Result clk_dt_register_clk_provider(const void *fdt, int nodeoffset,
					struct clk *(get_of_clk)(struct clk_dt_phandle_args *, void *),
					void *data)
{
	struct clk_dt_provider *prv = NULL;

	prv = malloc(sizeof(*prv));
	if (!prv)
		return TEE_ERROR_OUT_OF_MEMORY;

	prv->get_of_clk = get_of_clk;
	prv->data = data;
	prv->nodeoffset = nodeoffset;
	prv->clock_cells = fdt_clock_cells(fdt, nodeoffset);
	prv->phandle = fdt_get_phandle(fdt, nodeoffset);

	SLIST_INSERT_HEAD(&clk_dt_provider_list, prv, link);

	return TEE_SUCCESS;
}

static struct clk_dt_provider *clk_get_provider(int nodeoffset)
{
	struct clk_dt_provider *prv = NULL;

	SLIST_FOREACH(prv, &clk_dt_provider_list, link) {
		if (prv->nodeoffset == nodeoffset)
			return prv;
	}

	return NULL;
}

static struct clk_dt_provider *clk_get_provider_by_phandle(uint32_t phandle)
{
	struct clk_dt_provider *prv = NULL;

	SLIST_FOREACH(prv, &clk_dt_provider_list, link) {
		if (prv->phandle == phandle)
			return prv;
	}

	return NULL;
}

static int clk_dt_get_idx_by_name(const void *fdt, int nodeoffset, const char *name)
{
	int idx = -1;

	idx = fdt_stringlist_search(fdt, nodeoffset, "clock-names", name);
	if (idx < 0)
		return -1;

	return idx;
}

static struct clk *clk_dt_get_from_provider(struct clk_dt_provider *prv,
					    unsigned int clock_cells,
					    const uint32_t *prop)
{
	unsigned int arg = 0;
	struct clk_dt_phandle_args pargs = { };

	pargs.args_count = clock_cells;
	for (arg = 0; arg < clock_cells; arg++)
		pargs.args[arg] = fdt32_to_cpu(prop[arg + 1]);

	return prv->get_of_clk(&pargs, prv->data);
}

struct clk *clk_dt_get_by_name(const void *fdt, int nodeoffset,
			       const char *name)
{
	int clk_id = 0;

	clk_id = clk_dt_get_idx_by_name(fdt, nodeoffset, name);
	if (clk_id < 0)
		return NULL;

	return clk_dt_get_by_idx(fdt, nodeoffset, clk_id);
}

static struct clk *clk_dt_get_by_idx_prop(const char *prop_name,
					  const void *fdt, int nodeoffset,
					  unsigned int clk_idx)
{
	int len = 0;
	int idx = 0;
	int idx32 = 0;
	int clock_cells = 0;
	uint32_t phandle = 0;
	const uint32_t *prop = NULL;
	struct clk_dt_provider *prv = NULL;

	prop = fdt_getprop(fdt, nodeoffset, prop_name, &len);
	if (!prop)
		return NULL;

	while (idx < len) {
		idx32 = idx / sizeof(uint32_t);
		phandle = fdt32_to_cpu(prop[idx32]);

		prv = clk_get_provider_by_phandle(phandle);
		if (!prv)
			return NULL;

		clock_cells = prv->clock_cells;
		if (clk_idx) {
			clk_idx--;
			idx += sizeof(phandle) + clock_cells * sizeof(uint32_t);
			continue;
		}

		return clk_dt_get_from_provider(prv, clock_cells, &prop[idx32]);
	}

	return NULL;
}

struct clk *clk_dt_get_by_idx(const void *fdt, int nodeoffset,
			      unsigned int clk_idx)
{
	return clk_dt_get_by_idx_prop("clocks", fdt, nodeoffset, clk_idx);
}

static const struct clk_driver *clk_get_compatible_driver(const char *compat)
{
	const struct dt_driver *drv = NULL;
	const struct dt_device_match *dm = NULL;
	const struct clk_driver *clk_drv = NULL;

	for_each_dt_driver(drv) {
		if (drv->type != DT_DRIVER_CLK)
			continue;

		clk_drv = (const struct clk_driver *)drv->driver;
		for (dm = drv->match_table; dm && dm->compatible; dm++) {
			if (strcmp(dm->compatible, compat) == 0)
				return clk_drv;
		}
	}

	return NULL;
}

static void clk_setup_compatible(const void *fdt, const char *compatible,
				 const struct clk_driver *clk_drv);

static void probe_parent_clock(const void *fdt, int nodeoffset)
{
	int idx = 0;
	int len = 0;
	int count = 0;
	const char *compat = NULL;
	const struct clk_driver *clk_drv = NULL;

	count = fdt_stringlist_count(fdt, nodeoffset, "compatible");
	if (count < 0)
		return;

	for (idx = 0; idx < count; idx++) {
		compat = fdt_stringlist_get(fdt, nodeoffset, "compatible", idx,
					    &len);
		if (!compat)
			return;

		clk_drv = clk_get_compatible_driver(compat);
		if (clk_drv)
			clk_setup_compatible(fdt, compat, clk_drv);
	}
}

static void parse_clock_property(const void *fdt, int nodeoffset)
{
	int len = 0;
	int idx = 0;
	int parent_node = 0;
	int clock_cells = 0;
	uint32_t phandle = 0;
	const uint32_t *prop = NULL;

	prop = fdt_getprop(fdt, nodeoffset, "clocks", &len);
	if (!prop)
		return;

	while (idx < len) {
		phandle = fdt32_to_cpu(prop[idx]);

		parent_node = fdt_node_offset_by_phandle(fdt, phandle);
		if (parent_node < 0)
			return;

		probe_parent_clock(fdt, parent_node);

		clock_cells = fdt_clock_cells(fdt, parent_node);
		idx += sizeof(phandle) + clock_cells * sizeof(uint32_t);
	}
}

static void clk_setup_compatible(const void *fdt, const char *compatible,
				 const struct clk_driver *clk_drv)
{
	int status = 0;
	TEE_Result res = TEE_ERROR_GENERIC;
	int node = fdt_node_offset_by_compatible(fdt, -1, compatible);

	if (node < 0 || clk_get_provider(node))
		return;

	while (node >= 0) {
		status = _fdt_get_status(fdt, node);
		if (!(status & DT_STATUS_OK_SEC))
			continue;

		parse_clock_property(fdt, node);

		res = clk_drv->setup(fdt, node);
		if (res != TEE_SUCCESS) {
			EMSG("Failed to setup clock driver for compatible %s",
			      compatible);
			panic();
		}

		node = fdt_node_offset_by_compatible(fdt, node, compatible);
	};
}

static bool clk_dt_set_parent(struct clk *clk, struct clk *parent)
{
	size_t i = 0;

	for (i = 0; i < clk_get_num_parents(clk); i++) {
		if (clk_get_parent_by_index(clk, i) == parent) {
			if (clk_set_parent(clk, i) != TEE_SUCCESS)
				panic();
			return true;
		}
	}

	EMSG("Clock %s is not a parent of clock %s", parent->name, clk->name);
	return false;
}

static void parse_assigned_clock(const void *fdt, int nodeoffset)
{
	int rate_len = 0;
	int clock_idx = 0;
	struct clk *clk = NULL;
	unsigned long rate = 0;
	struct clk *parent = NULL;
	const uint32_t *rate_prop = NULL;

	rate_prop = fdt_getprop(fdt, nodeoffset, "assigned-clock-rates",
				&rate_len);
	rate_len /= sizeof(uint32_t);

	while (1) {
		clk = clk_dt_get_by_idx_prop("assigned-clocks", fdt, nodeoffset,
					     clock_idx);
		if (!clk)
			return;

		parent = clk_dt_get_by_idx_prop("assigned-clock-parents", fdt,
						nodeoffset, clock_idx);
		if (parent) {
			if (!clk_dt_set_parent(clk, parent))
				return;
		}

		if (rate_prop && clock_idx <= rate_len) {
			rate = fdt32_to_cpu(rate_prop[clock_idx]);
			if (rate) {
				if (clk_set_rate(clk, rate) != TEE_SUCCESS)
					panic();
			}
		}

		clock_idx++;
	}
}

static void clk_probe_assigned(const void *fdt, int parent_node)
{
	int len = 0;
	int child = 0;
	int status = 0;

	fdt_for_each_subnode(child, fdt, parent_node) {
		clk_probe_assigned(fdt, child);

		status = _fdt_get_status(fdt, child);
		if (!status)
			continue;

		if (fdt_getprop(fdt, child, "assigned-clocks", &len))
			parse_assigned_clock(fdt, child);
	}
}

static TEE_Result clk_dt_probe(void)
{
	const struct dt_driver *drv = NULL;
	const void *fdt = get_embedded_dt();
	const struct dt_device_match *dm = NULL;
	const struct clk_driver *clk_drv = NULL;

	DMSG("Probing clocks from devicetree");
	if (!fdt)
		panic();

	for_each_dt_driver(drv) {
		if (drv->type != DT_DRIVER_CLK)
			continue;

		clk_drv = (const struct clk_driver *)drv->driver;
		for (dm = drv->match_table; dm && dm->compatible; dm++)
			clk_setup_compatible(fdt, dm->compatible, clk_drv);
	}

	clk_probe_assigned(fdt, -1);

	return TEE_SUCCESS;
}
early_init(clk_dt_probe);