/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2022, Microchip
 */

#ifndef __DRIVERS_PINCTRL_H
#define __DRIVERS_PINCTRL_H

#include <bitstring.h>
#include <kernel/dt_driver.h>
#include <sys/queue.h>
#include <tee_api_types.h>

enum pinctrl_dt_prop {
	PINCTRL_DT_PROP_BIAS_DISABLE,
	PINCTRL_DT_PROP_BIAS_PULL_UP,
	PINCTRL_DT_PROP_BIAS_PULL_DOWN,
	PINCTRL_DT_PROP_MAX
};

struct pinconf {
	const struct pinctrl_ops *ops;
	void *priv;
};

struct pinctrl_state {
	unsigned int conf_count;
	struct pinconf *confs[];
};

struct pinctrl_ops {
	TEE_Result (*conf_apply)(struct pinconf *conf);
	void (*conf_free)(struct pinconf *conf);
};

typedef struct pinconf *(*pinctrl_dt_get_func)(struct dt_driver_phandle_args *a,
					       void *data, TEE_Result *res);

#ifdef CFG_DRIVERS_PINCTRL
/**
 * pinctrl_dt_register_provider - Register a pinctrl controller provider
 *
 * @fdt: Device tree to work on
 * @nodeoffset: Node offset of the pin controller
 * @get_dt_pinctrl: Callback to match the pin controller with a struct pinconf
 * @data: Data which will be passed to the get_dt_pinctrl callback
 * Return a TEE_Result compliant value
 */
static inline
TEE_Result pinctrl_register_provider(const void *fdt, int nodeoffset,
				     pinctrl_dt_get_func get_dt_pinctrl,
				     void *data)
{
	return dt_driver_register_provider(fdt, nodeoffset,
					   (get_of_device_func)get_dt_pinctrl,
					   data, DT_DRIVER_PINCTRL);
}

/**
 * pinctrl_get_state_by_name - Obtain a pinctrl state by name
 *
 * @fdt: Device tree to work on
 * @nodeoffset: Node offset of the pin controller
 * @name: name of the pinctrl state to obtain from device-tree
 * @state: Pointer filled with the retrieved state, must be freed after use
   using pinctrl_free_state()
 * Return a TEE_Result compliant value
 */
TEE_Result pinctrl_get_state_by_name(const void *fdt, int nodeoffset,
				     const char *name,
				     struct pinctrl_state **state);

/**
 * pinctrl_get_state_by_idx - Obtain a pinctrl state by index
 *
 * @fdt: Device tree to work on
 * @nodeoffset: Node offset of the pin controller
 * @pinctrl_id: Index of the pinctrl state to obtain from device-tree
 * @state: Pointer filled with the retrieved state, must be freed after use
   using pinctrl_free_state()
 * Return a TEE_Result compliant value
 */
TEE_Result pinctrl_get_state_by_idx(const void *fdt, int nodeoffset,
				    unsigned int pinctrl_id,
				    struct pinctrl_state **state);

/**
 * pinctrl_free_state - Free a pinctrl state that was previously obtained
 *
 * @state: State to be freed
 */
void pinctrl_free_state(struct pinctrl_state *state);

/**
 * pinctrl_apply_state - apply a pinctrl state
 *
 * @state: State to be applied
 * Return a TEE_Result compliant value
 */
TEE_Result pinctrl_apply_state(struct pinctrl_state *state);

TEE_Result pinctrl_parse_dt_pin_modes(const void *fdt, int node,
				      bitstr_t **modes);
#else /* CFG_DRIVERS_PINCTRL */
static inline
TEE_Result pinctrl_register_provider(const void *fdt __unused,
				     int nodeoffset __unused,
				     pinctrl_dt_get_func get_dt_pinctrl __unused,
				     void *data __unused)
{
	return TEE_ERROR_NOT_SUPPORTED;
}

static inline
TEE_Result pinctrl_get_state_by_name(const void *fdt __unused,
				     int nodeoffset __unused,
				     const char *name __unused,
				     struct pinctrl_state **state __unused)
{
	return TEE_ERROR_NOT_SUPPORTED;
}

static inline
TEE_Result pinctrl_get_state_by_idx(const void *fdt __unused,
				    int nodeoffset __unused,
				    unsigned int pinctrl_id __unused,
				    struct pinctrl_state **state __unused)
{
	return TEE_ERROR_NOT_SUPPORTED;
}

static inline
void pinctrl_free_state(struct pinctrl_state *state __unused)
{
	return TEE_ERROR_NOT_SUPPORTED;
}

static inline
TEE_Result pinctrl_apply_state(struct pinctrl_state *state __unused)
{
	return TEE_ERROR_NOT_SUPPORTED;
}

static inline
TEE_Result pinctrl_parse_dt_pin_modes(const void *fdt __unused,
				      int node __unused,
				      bitstr_t **modes __unused)
{
	return TEE_ERROR_NOT_SUPPORTED;
}

#endif /* CFG_DRIVERS_PINCTRL */
#endif /* __DRIVERS_PINCTRL_H */
