// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright 2023 Microchip
 */

#include <drivers/regulator.h>
#include <initcall.h>
#include <kernel/pm.h>
#include <libfdt.h>
#include <sys/queue.h>
#include <tee_api_defines.h>
#include <tee_api_types.h>
#include <util.h>

enum reg_state_flags {
	REG_ON_IN_SUSPEND = BIT(0),
	REG_OFF_IN_SUSPEND = BIT(1),
	REG_SET_MODE = BIT(3),
};

enum regulator_flags {
	REG_ALWAYS_ON = BIT(0),
};

static SLIST_HEAD(,regulator) regulators = SLIST_HEAD_INITIALIZER(regulators);

const char *regulator_state_name[] = {
	[REG_STATE_STANDBY] = "regulator-state-standby",
	[REG_STATE_SUSPEND_TO_MEM] = "regulator-state-mem",
};

const char *regulator_get_name(struct regulator *reg)
{
	return reg->ops->get_name(reg);
}

bool regulator_is_enabled(struct regulator *reg)
{
	return reg->enabled;
}

TEE_Result regulator_enable(struct regulator *reg)
{
	return reg->ops->enable(reg);
}

TEE_Result regulator_disable(struct regulator *reg)
{
	return reg->ops->disable(reg);
}

TEE_Result regulator_set_voltage(struct regulator *reg, unsigned long uv)
{
	TEE_Result res = TEE_ERROR_GENERIC;

	if (uv > reg->max_uv || uv < reg->min_uv)
		return TEE_ERROR_BAD_PARAMETERS;

	res = reg->ops->set_voltage(reg, uv);
	if (res)
		return res;

	reg->uv = uv;

	return TEE_SUCCESS;
}

TEE_Result regulator_get_voltage(struct regulator *reg, unsigned long *uv)
{
	if (!reg->enabled)
		return TEE_ERROR_BAD_STATE;

	if (reg->ops->get_voltage)
		reg->ops->get_voltage(reg, &reg->uv);

	*uv = reg->uv;

	return TEE_SUCCESS;
}

static TEE_Result dt_get_u32(const void *fdt, int node, const char *property,
			     uint32_t *value)
{
	const fdt32_t *p = NULL;
	int len = 0;

	p = fdt_getprop(fdt, node, property, &len);
	if (!p || len != sizeof(*p))
		return TEE_ERROR_ITEM_NOT_FOUND;

	*value = fdt32_to_cpu(*p);

	return TEE_SUCCESS;
}

static TEE_Result regulator_dt_parse_state(const void *fdt, int node,
					   struct regulator *reg,
					   struct regulator_state *state)
{
	if (fdt_getprop(fdt, node, "regulator-on-in-suspend", NULL))
		state->flags |= REG_ON_IN_SUSPEND;

	if (fdt_getprop(fdt, node, "regulator-off-in-suspend", NULL))
		state->flags |= REG_OFF_IN_SUSPEND;

	if ((state->flags & REG_ON_IN_SUSPEND) &&
	    (state->flags & REG_OFF_IN_SUSPEND)) {
		EMSG("Invalid regulator dt (suspend on & off)");
		return TEE_ERROR_BAD_PARAMETERS;
	}

	dt_get_u32(fdt, node, "regulator-mode", &state->mode);
	dt_get_u32(fdt, node, "regulator-suspend-microvolt", &state->uv);
	if (state->uv && (state->uv > reg->max_uv || state->uv < reg->min_uv)) {
		EMSG("Invalid regulator voltage %u", state->uv);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	return TEE_SUCCESS;
}

static TEE_Result regulator_init(struct regulator *reg, uint32_t initial_mode)
{
	TEE_Result res = TEE_ERROR_GENERIC;

	if (reg->ops->get_voltage)
		reg->ops->get_voltage(reg, &reg->uv);

	if (reg->ops->set_mode && initial_mode) {
		res = reg->ops->set_mode(reg, initial_mode);
		if (res)
			return res;
	}

	if (reg->flags & REG_ALWAYS_ON) {
		res = regulator_enable(reg);
		if (res)
			return res;

		reg->enabled = true;
	}

	return TEE_SUCCESS;
}

static void regulator_parse_allowed_mode(const void *fdt, int node,
					 struct regulator *reg,
					 const struct regulator_dt_drv_ops *drv_ops)
{
	const fdt32_t *p = NULL;
	uint32_t val = 0, reg_mode = 0;
	int len = 0;
	unsigned int i = 0;

	p = fdt_getprop(fdt, node, "regulator-allowed-modes", &len);
	if (!p)
		return;

	for (i = 0; i < len / sizeof(uint32_t); i++) {

		val = fdt32_to_cpu(*p);
		drv_ops->dt_map_mode(val, &reg_mode);
		if (reg_mode == REGULATOR_MODE_INVALID) {
			EMSG("Failed to translate dt mode");
			continue;
		}

		reg->allowed_mode |= reg_mode;
		p++;
	}

}

static TEE_Result regulator_dt_parse_single(const void *fdt, int node,
		 const struct regulator_dt_drv_ops *drv_ops, void *data)
{
	struct regulator *reg = NULL;
	int state_node = -1, i = 0;
	TEE_Result res = TEE_ERROR_GENERIC;
	uint32_t initial_mode = 0;
	unsigned int uv;

	reg = calloc(1, sizeof(*reg));
	if (!reg)
		return TEE_ERROR_OUT_OF_MEMORY;

	if (!dt_get_u32(fdt, node, "regulator-min-microvolt", &uv))
		reg->min_uv = uv;
	if (!dt_get_u32(fdt, node, "regulator-max-microvolt", &uv))
		reg->max_uv = uv;
	dt_get_u32(fdt, node, "regulator-initial-mode", &initial_mode);
	if (fdt_getprop(fdt, node, "regulator-always-on", NULL))
		reg->flags |= REG_ALWAYS_ON;

	regulator_parse_allowed_mode(fdt, node, reg, drv_ops);

	/* Parse states */
	for(i = 0; i < (int) ARRAY_SIZE(regulator_state_name); i++) {
		state_node = fdt_subnode_offset(fdt, node,
						regulator_state_name[i]);
		if (state_node < 0)
			continue;

		reg->states[i] = calloc(1, sizeof(struct regulator_state));
		if (!reg->states[i]) {
			res = TEE_ERROR_OUT_OF_MEMORY;
			goto out;
		}

		res = regulator_dt_parse_state(fdt, state_node, reg,
					       reg->states[i]);
		if (res) {
			free(reg->states[i]);
			goto out;
		}
	}

	res = drv_ops->dt_match_reg(fdt, node, reg, data);
	if (res)
		goto out;

	res = regulator_init(reg, initial_mode);
	if (res)
		goto out;

	SLIST_INSERT_HEAD(&regulators, reg, link);

	return TEE_SUCCESS;

out:
	while(--i >= 0)
		free(reg->states[i]);
	free(reg);

	return res;
}

TEE_Result regulator_register(const void *fdt, int node,
			      const struct regulator_dt_drv_ops *drv_ops,
			      void *data)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	int regs_node = -1, reg = -1;

	regs_node = fdt_subnode_offset(fdt, node, "regulators");
	if (regs_node < 0)
		return TEE_ERROR_GENERIC;

	fdt_for_each_subnode(reg, fdt, regs_node) {
		res = regulator_dt_parse_single(fdt, reg, drv_ops, data);
		if (res)
			return res;
	}

	return TEE_SUCCESS;
}

#ifdef CFG_PM_ARM32
static int suspend_type_to_reg_state(enum pm_suspend_type suspend_type)
{
	switch(suspend_type) {
		case PM_SUSPEND_STANDBY: return REG_STATE_STANDBY;
		case PM_SUSPEND_TO_MEM: return REG_STATE_SUSPEND_TO_MEM;
		default: return -1;
	}
}

static TEE_Result regulator_suspend(uint32_t suspend_hint)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	struct regulator *reg = NULL;
	struct regulator_state *s = NULL;
	int suspend_type = PM_HINT_SUSPEND_TYPE(suspend_hint);
	int state_type = suspend_type_to_reg_state(suspend_type);

	if (state_type < 0)
		return TEE_ERROR_BAD_PARAMETERS;

	SLIST_FOREACH(reg, &regulators, link) {
		s = reg->states[state_type];
		if (!s)
			continue;

		if ((s->flags & REG_OFF_IN_SUSPEND) &&
		     reg->ops->suspend_disable) {
			res = reg->ops->suspend_disable(reg, suspend_hint);
			if (res)
				EMSG("Failed to disable regulator for suspend");
		} else if ((s->flags & REG_ON_IN_SUSPEND) &&
			   reg->ops->suspend_enable) {
			res = reg->ops->suspend_enable(reg, suspend_hint);
			if (res)
				EMSG("Failed to enable regulator for suspend");
		}
	
		if (s->mode > 0 && reg->ops->suspend_set_mode) {
			res = reg->ops->suspend_set_mode(reg, suspend_hint,
							 s->mode);
			if (res)
				EMSG("Failed to set suspend mode");
		}

		if (s->uv > 0 && reg->ops->suspend_set_voltage) {
			res = reg->ops->suspend_set_voltage(reg, suspend_hint,
							    s->uv);
			if (res)
				EMSG("Failed to set suspend voltage");
		}

	}

	return TEE_SUCCESS;
}

static TEE_Result regulator_pm(enum pm_op op, uint32_t pm_hint __unused,
			       const struct pm_callback_handle *hdl __unused)
{ 
	switch (op) {
	case PM_OP_SUSPEND:
		return regulator_suspend(pm_hint);
	default:
		break;
	}

	return TEE_SUCCESS;
}

static TEE_Result regulator_pm_init(void)
{
	register_pm_driver_cb(regulator_pm, NULL, "regulator");

	return TEE_SUCCESS;
}

early_init(regulator_pm_init);
#endif
