/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2023, Microchip
 */

#ifndef __DRIVERS_REGULATOR_H
#define __DRIVERS_REGULATOR_H

#include <tee_api_types.h>
#include <sys/queue.h>

struct regulator;

#ifdef CFG_DRIVERS_REGULATOR

#define REGULATOR_MODE_INVALID			0x0
#define REGULATOR_MODE_FAST			0x1
#define REGULATOR_MODE_NORMAL			0x2
#define REGULATOR_MODE_IDLE			0x4
#define REGULATOR_MODE_STANDBY			0x8

struct regulator_ops {
	const char * (*get_name)(struct regulator *reg);
	TEE_Result (*enable)(struct regulator *reg);
	TEE_Result (*disable)(struct regulator *reg);
	TEE_Result (*set_voltage)(struct regulator *reg, unsigned long uv);
	TEE_Result (*get_voltage)(struct regulator *reg, unsigned long *uv);
	TEE_Result (*set_mode)(struct regulator *reg, uint32_t mode);
	TEE_Result (*list_voltage)(struct regulator *reg, unsigned int *count,
				   unsigned long *voltage, unsigned int offset);
#ifdef CFG_PM_ARM32
	TEE_Result (*suspend_enable)(struct regulator *reg, uint32_t suspend_hint);
	TEE_Result (*suspend_disable)(struct regulator *reg, uint32_t suspend_hint);
	TEE_Result (*suspend_set_voltage)(struct regulator *reg, uint32_t suspend_hint, unsigned long uv);
	TEE_Result (*suspend_set_mode)(struct regulator *reg, uint32_t suspend_hint, uint32_t mode);
#endif
};

struct regulator_state {
	unsigned int uv;
	uint32_t flags;
	uint32_t mode;
};

enum regulator_state_type {
	REG_STATE_STANDBY,
	REG_STATE_SUSPEND_TO_MEM,
	REG_STATE_COUNT,
};

struct regulator {
	bool enabled;
	uint32_t flags;
	unsigned long min_uv;
	unsigned long max_uv;
	unsigned long uv;
	uint32_t mode;
	uint32_t allowed_mode;
	struct regulator_state *states[REG_STATE_COUNT];
	const struct regulator_ops *ops;
	void *priv;
	SLIST_ENTRY(regulator) link;
};

typedef TEE_Result (*regulator_dt_match_reg)(const void *fdt, int node,
					  struct regulator *reg, void *data);

typedef TEE_Result (*regulator_dt_map_mode)(unsigned int dt_mode,
					 unsigned int *reg_mode);

struct regulator_dt_drv_ops {
	regulator_dt_match_reg dt_match_reg;
	regulator_dt_map_mode dt_map_mode;
};

TEE_Result regulator_register(const void *fdt, int node,
			      const struct regulator_dt_drv_ops *ops, void *data);

const char *regulator_get_name(struct regulator *reg);
bool regulator_is_enabled(struct regulator *reg);
TEE_Result regulator_enable(struct regulator *reg);
TEE_Result regulator_disable(struct regulator *reg);
TEE_Result regulator_set_voltage(struct regulator *reg,
					       unsigned long  uv);
TEE_Result regulator_get_voltage(struct regulator *reg,
					       unsigned long  *uv );

#else

static inline const char *regulator_get_name(struct regulator *reg __unused)
{
	return NULL;
}

static inline TEE_Result regulator_is_enabled(struct regulator *reg __unused,
					      bool *enabled __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

static inline TEE_Result regulator_enable(struct regulator *reg __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

static inline TEE_Result regulator_disable(struct regulator *reg __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

static inline TEE_Result regulator_set_voltage(struct regulator *reg __unused,
					       unsigned long uv __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

static inline TEE_Result regulator_get_voltage(struct regulator *reg __unused,
					       unsigned long *uv __unused)
{
	return TEE_ERROR_NOT_IMPLEMENTED;
}

#endif

#endif /* __DRIVERS_REGULATOR_H */