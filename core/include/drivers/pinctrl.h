/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2022, Microchip
 */

#ifndef __DRIVERS_PINCTRL_H
#define __DRIVERS_PINCTRL_H

#include <sys/queue.h>
#include <tee_api_types.h>

enum pin_config_mode {
	PIN_MODE_BIAS_DISABLE,
	PIN_MODE_BIAS_PULL_UP,	
	PIN_MODE_BIAS_PULL_DOWN,
};

/*
 * struct pinctrl - Describe a pinctrl controller
 *
 * @node: Node offset of the controller in the device-tree
 * @apply_state: callback which apply the content of a specific pinctrl conf
 * @link: Link in list of pinctrl
 */
struct pinctrl {
	int node;
	TEE_Result (*apply_state)(struct pinctrl *pinctrl, const void *fdt,
				  int node);
	SLIST_ENTRY(pinctrl) link;
};

#ifdef CFG_DRIVERS_PINCTRL

void pinctrl_register(struct pinctrl *pinctrl);

TEE_Result pinctrl_apply_state(const void *fdt, int node, const char *name);

TEE_Result pinctrl_parse_dt_pin_mode_config(const void *fdt, int node,
					    uint32_t **cfg,
					    unsigned int *count);
#else

static inline void pinctrl_register(struct pinctrl *pinctrl) {}

TEE_Result pinctrl_apply_state(const void *fdt, int node, const char *name)
{
	return TEE_ERROR_NOT_SUPPORTED;
}

static inline
TEE_Result pinctrl_parse_dt_pin_mode_config(const void *fdt, int node,
					    uint32_t **cfg,
					    unsigned int *count)
{
	return TEE_ERROR_NOT_SUPPORTED;
}

#endif

#endif