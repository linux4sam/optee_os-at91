// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2022, Microchip
 */

#include <assert.h>
#include <drivers/pinctrl.h>
#include <libfdt.h>
#include <stdio.h>
#include <sys/queue.h>
#include <tee_api_defines.h>
#include <tee_api_defines_extensions.h>
#include <tee_api_types.h>
#include <util.h>

SLIST_HEAD(pinctrl_list, pinctrl) pinctrl_list;

static const char *pin_modes[] = {
	[PIN_MODE_BIAS_DISABLE] = "bias-disable",
	[PIN_MODE_BIAS_PULL_UP] = "bias-pull-up",
	[PIN_MODE_BIAS_PULL_DOWN] = "bias-pull-down",
};

TEE_Result pinctrl_parse_dt_pin_mode_config(const void *fdt, int node,
					    uint32_t **cfg, unsigned int *count)
{
	unsigned int i = 0;
	unsigned int prop_count = 0;
	uint32_t *configs = NULL;

	assert(cfg && count);

	for (i = 0; i < ARRAY_SIZE(pin_modes); i++) {
		if (!fdt_getprop(fdt, node, pin_modes[i], NULL))
			continue;

		prop_count++;
	}

	configs = calloc(1, prop_count * sizeof(uint32_t));
	if (!configs)
		return TEE_ERROR_OUT_OF_MEMORY;

	prop_count = 0;
	for (i = 0; i < ARRAY_SIZE(pin_modes); i++) {
		if (!fdt_getprop(fdt, node, pin_modes[i], NULL))
			continue;

		configs[prop_count] = i;
		prop_count++;
	}

	*cfg = configs;
	*count = prop_count;

	return TEE_SUCCESS;
}

void pinctrl_register(struct pinctrl *pinctrl)
{
	assert(pinctrl->apply_state);

	SLIST_INSERT_HEAD(&pinctrl_list, pinctrl, link);
}

static struct pinctrl *pinctrl_find_by_node(int nodeoffset)
{
	struct pinctrl *pinctrl = NULL;

	SLIST_FOREACH(pinctrl, &pinctrl_list, link) {
		if (pinctrl->node == nodeoffset)
			return pinctrl;
	}

	return NULL;
}

static struct pinctrl *pinctrl_find_parent(const void *fdt, int node)
{
	int parent_node = -1;

	parent_node = fdt_parent_offset(fdt, node);
	if (parent_node < 0)
		return NULL;

	return pinctrl_find_by_node(parent_node);
}

TEE_Result pinctrl_apply_state(const void *fdt, int node, const char *name)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	struct pinctrl *pinctrl = NULL;
	const uint32_t *prop = NULL;
	int pinctrl_index = 0;
	uint32_t phandle = 0;
	int prop_count = 0;
	/* Enough char to hold "pinctrl-xx" (2 digit index) */
	char propname[11];
	int i = 0;

	if (name == NULL)
		name = "default";

	pinctrl_index = fdt_stringlist_search(fdt, node, "pinctrl-names",
					      name);
	if (pinctrl_index > 99)
		return TEE_ERROR_OVERFLOW;

	snprintf(propname, sizeof(propname), "pinctrl-%d", pinctrl_index);

	prop = fdt_getprop(fdt, node, propname, &prop_count);
	if (!prop)
		return TEE_ERROR_ITEM_NOT_FOUND;

	prop_count /= 4;
	for (i = 0; i < prop_count; i++) {
		phandle = fdt32_to_cpu(prop[i]);

		node = fdt_node_offset_by_phandle(fdt, phandle);
		if (!node)
			return TEE_ERROR_BAD_PARAMETERS;

		pinctrl = pinctrl_find_parent(fdt, node);
		if (!pinctrl)
			return TEE_ERROR_DEFER_DRIVER_INIT;

		res = pinctrl->apply_state(pinctrl, fdt, node);
		if (res)
			return res;
	}

	return TEE_SUCCESS;
}