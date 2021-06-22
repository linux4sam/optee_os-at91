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

#if defined(CFG_DT)

struct clk *clk_of_get_simple_clk(struct clk_of_phandle_args *args __unused,
				  void *data)
{
	return data;
}

struct clk_of_provider {
	int nodeoffset;
	struct clk *(*get_of_clk)(struct clk_of_phandle_args *args, void *data);
	void *data;
	SLIST_ENTRY(clk_of_provider) link;
};

static SLIST_HEAD(, clk_of_provider) clk_of_provider_list =
				SLIST_HEAD_INITIALIZER(clk_of_provider_list);

int clk_of_register_clk_provider(int nodeoffset,
				 struct clk *(get_of_clk)(struct clk_of_phandle_args *, void *),
				 void *data)
{
	struct clk_of_provider *prv;

	prv = malloc(sizeof(*prv));
	if (!prv)
		return -1;

	prv->get_of_clk = get_of_clk;
	prv->data = data;
	prv->nodeoffset = nodeoffset;

	SLIST_INSERT_HEAD(&clk_of_provider_list, prv, link);

	return 0;
}

static struct clk_of_provider *clk_get_provider(int nodeoffset)
{
	struct clk_of_provider *prv;

	SLIST_FOREACH(prv, &clk_of_provider_list, link) {
		if (prv->nodeoffset == nodeoffset)
			return prv;
	}

	return NULL;
}

static uint32_t fdt_clock_cells(void *fdt, int nodeoffset)
{
	const fdt32_t *c;
	int len;

	c = fdt_getprop(fdt, nodeoffset, "#clock-cells", &len);
	if (!c)
		return len;

	if (len != sizeof(*c))
		return -FDT_ERR_BADNCELLS;

	return fdt32_to_cpu(*c);
}

int clk_of_get_idx_by_name(void *fdt, int nodeoffset, const char *name)
{
	int idx;

	idx = fdt_stringlist_search(fdt, nodeoffset, "clock-names", name);
	if (idx < 0)
		return -1;

	return idx;
}

static struct clk *clk_of_get_from_provider(int nodeoffset,
					    unsigned int clock_cells,
					    const uint32_t *prop)
{
	unsigned int arg;
	struct clk_of_phandle_args pargs;
	struct clk_of_provider *prv;

	pargs.args_count = clock_cells;
	for (arg = 0; arg < clock_cells; arg++)
		pargs.args[arg] = fdt32_to_cpu(prop[arg + 1]);

	prv = clk_get_provider(nodeoffset);
	if (!prv)
		return NULL;

	return prv->get_of_clk(&pargs, prv->data);
}

struct clk *clk_of_get_by_name(void *fdt, int nodeoffset,
			       const char *name)
{
	int clk_id;

	clk_id = clk_of_get_idx_by_name(fdt, nodeoffset, name);
	if (clk_id < 0)
		return NULL;

	return clk_of_get_by_idx(fdt, nodeoffset, clk_id);
}

struct clk *clk_of_get_by_idx(void *fdt, int nodeoffset, int clk_idx)
{
	uint32_t phandle;
	const uint32_t *prop;
	int len, idx = 0, parent_node, clock_cells, idx32;

	prop = fdt_getprop(fdt, nodeoffset, "clocks", &len);
	if (!prop)
		return NULL;

	while (idx < len) {
		idx32 = idx / sizeof(uint32_t);
		phandle = fdt32_to_cpu(prop[idx32]);

		parent_node = fdt_node_offset_by_phandle(fdt, phandle);
		if (parent_node < 0)
			return NULL;

		clock_cells = fdt_clock_cells(fdt, parent_node);
		if (clk_idx == 0)
			return clk_of_get_from_provider(parent_node,
							clock_cells,
							&prop[idx32]);

		clk_idx--;
		idx += sizeof(phandle) + clock_cells * sizeof(uint32_t);
	}

	return NULL;
}

static const struct clk_driver *clk_get_compatible_driver(const char *compat)
{
	const struct dt_device_match *dm;
	const struct dt_driver *drv;
	const struct clk_driver *clk_drv;

	for_each_dt_driver(drv) {
		if (drv->type != DT_DRIVER_CLK)
			continue;

		clk_drv = (const struct clk_driver *)drv->driver;
		for (dm = drv->match_table; dm; dm++) {
			if (!dm->compatible)
				break;
			if (strcmp(dm->compatible, compat) == 0)
				return clk_drv;
		}
	}

	return NULL;
}

static void clk_setup_compatible(void *fdt, const char *compatible,
				 const struct clk_driver *clk_drv);

static void probe_parent_clock(void *fdt, int nodeoffset)
{
	const char *compat;
	int idx, count, len;
	const struct clk_driver *clk_drv;

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

static void parse_clock_property(void *fdt, int nodeoffset)
{
	uint32_t phandle;
	const uint32_t *prop;
	int len, idx = 0, parent_node, clock_cells;

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

static void clk_setup_compatible(void *fdt, const char *compatible,
				 const struct clk_driver *clk_drv)
{
	int status;
	int node = fdt_node_offset_by_compatible(fdt, -1, compatible);

	if (node < 0 || clk_get_provider(node))
		return;

	while (node >= 0) {
		status = _fdt_get_status(fdt, node);
		if (!(status & DT_STATUS_OK_SEC))
			continue;

		parse_clock_property(fdt, node);

		clk_drv->setup(fdt, node);

		node = fdt_node_offset_by_compatible(fdt, node, compatible);
	};
}

#if defined(CFG_SCMI_MSG_DRIVERS)

#define DT_MAX_PATH_LEN		200

static void clk_scmi_add_fragment(void *fdt, int offs,
				  uint32_t *scmi_clk_phandles,
				  uint32_t scmi_chan_count)
{
	struct clk *clk;
	uint32_t phandle;
	char path[DT_MAX_PATH_LEN];
	void *ext_fdt = get_external_dt();
	int node, clk_idx = 0, fixup_node, ret;

	/*
	 * If the node is a provider, then do not patch it, since it does not
	 * rely on a SCMI clock for sure...
	 */
	if (clk_get_provider(offs))
		return;

	ret = fdt_get_path(fdt, offs, path, DT_MAX_PATH_LEN);
	if (ret < 0)
		panic();

	node = dt_add_overlay_fragment(ext_fdt, path);
	if (node < 0)
		panic();

	while (1) {
		clk = clk_of_get_by_idx(fdt, offs, clk_idx);
		if (!clk)
			break;

		if (clk->scmi_channel_id >= scmi_chan_count)
			panic();

		phandle = scmi_clk_phandles[clk->scmi_channel_id];

		ret = fdt_appendprop_u32(ext_fdt, node, "clocks", phandle);
		if (ret < 0)
			panic();

		ret = fdt_appendprop_u32(ext_fdt, node, "clocks", clk->scmi_id);
		if (ret < 0)
			panic();
		clk_idx++;
	};

	if (clk_idx == 0)
		return;

	ret = fdt_get_path(ext_fdt, node, path, DT_MAX_PATH_LEN);
	if (ret < 0)
		panic();

	fixup_node = dt_add_fixup_node(ext_fdt, path);
	if (fixup_node < 0)
		panic();

	/* SCMI clock-cells is always equal to 1 */
	for (int i = 0; i < clk_idx; i++) {
		ret = fdt_appendprop_u32(ext_fdt, fixup_node, "clocks", i * 8);
		if (ret < 0)
			panic();
	}
}

static void clk_scmi_patch_dt_node(void *fdt, int offs,
				   uint32_t *scmi_clk_phandles,
				   uint32_t scmi_chan_count)
{
	int node, status;

	fdt_for_each_subnode(node, fdt, offs) {
		clk_scmi_patch_dt_node(fdt, node, scmi_clk_phandles,
				       scmi_chan_count);

		status = _fdt_get_status(fdt, node);
		if (!(status & DT_STATUS_OK_NSEC))
			continue;

		if (fdt_getprop(fdt, node, "clocks", NULL))
			clk_scmi_add_fragment(fdt, node, scmi_clk_phandles,
					      scmi_chan_count);
	}
}

static void clk_scmi_disable_secure_clocks(void *fdt)
{
	struct clk_of_provider *prv;
	char path[DT_MAX_PATH_LEN];
	int node, ret;
	void *ext_fdt = get_external_dt();

	SLIST_FOREACH(prv, &clk_of_provider_list, link) {
		/*
		 * We want to disable controllers that are enabled as secure
		 * only in the device tree.
		 */
		if (_fdt_get_status(fdt, prv->nodeoffset) != DT_STATUS_OK_SEC)
			continue;

		fdt_get_path(fdt, prv->nodeoffset, path, DT_MAX_PATH_LEN);

		node = dt_add_overlay_fragment(ext_fdt, path);
		if (node < 0)
			panic();

		ret = dt_enable_secure_status(ext_fdt, node);
		if (ret < 0)
			panic();
	}
}

int clk_scmi_update_dt(uint32_t *scmi_clk_phandles, uint32_t scmi_chan_count)
{
	void *fdt = get_embedded_dt();

	clk_scmi_disable_secure_clocks(fdt);

	clk_scmi_patch_dt_node(fdt, -1, scmi_clk_phandles, scmi_chan_count);

	return 0;
}

#endif

static TEE_Result clk_probe(void)
{
	const struct dt_device_match *dm;
	const struct dt_driver *drv;
	void *fdt = get_embedded_dt();
	const struct clk_driver *clk_drv;

	IMSG("Probing clocks from device tree\n");
	if (!fdt)
		panic();

	for_each_dt_driver(drv) {
		if (drv->type != DT_DRIVER_CLK)
			continue;

		clk_drv = (const struct clk_driver *)drv->driver;
		for (dm = drv->match_table; dm; dm++) {
			if (!dm->compatible)
				break;

			clk_setup_compatible(fdt, dm->compatible, clk_drv);
		}
	}

	return TEE_SUCCESS;
}
service_init(clk_probe);

#endif
