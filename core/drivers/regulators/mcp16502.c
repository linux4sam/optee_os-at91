// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright 2023 Microchip
 */

#include "../pm/sam/at91_pm.h"
#include <assert.h>
#include <drivers/gpio.h>
#include <drivers/i2c.h>
#include <drivers/regulator.h>
#include <io.h>
#include <kernel/delay.h>
#include <kernel/pm.h>
#include <libfdt.h>
#include <platform_config.h>
#include <kernel/dt_driver.h>
#include <string.h>
#include <sys/queue.h>

#define MCP16502_MODE_AUTO_PFM 0
#define MCP16502_MODE_FPWM BIT32(6)

#define VDD_LOW_SEL 0x0D
#define VDD_HIGH_SEL 0x3F
#define VSET_COUNT (VDD_HIGH_SEL - VDD_LOW_SEL)

#define MCP16502_VSET_MASK 0x3F
#define MCP16502_EN BIT32(7)
#define MCP16502_MODE BIT32(6)

#define MCP16502_REG_BASE(i, r) ((((i) + 1) << 4) + MCP16502_REG_##r)
#define MCP16502_STAT_BASE(i) ((i) + 5)

#define MCP16502_OPMODE_ACTIVE 0x2
#define MCP16502_OPMODE_LPM 0x4
#define MCP16502_OPMODE_HIB 0x8

enum mcp16502_reg_id
{
	BUCK1 = 0,
	BUCK2,
	BUCK3,
	BUCK4,
	BUCK_COUNT = BUCK4,
	LDO1,
	LDO2,
	MCP16502_REG_COUNT
};

/**
 * enum mcp16502_reg_type - MCP16502 regulators's registers
 * @MCP16502_REG_A: active state register
 * @MCP16502_REG_LPM: low power mode state register
 * @MCP16502_REG_HIB: hibernate state register
 * @MCP16502_REG_SEQ: startup sequence register
 * @MCP16502_REG_CFG: configuration register
 */
enum mcp16502_reg_type
{
	MCP16502_REG_A,
	MCP16502_REG_LPM,
	MCP16502_REG_HIB,
	MCP16502_REG_HPM,
	MCP16502_REG_SEQ,
	MCP16502_REG_CFG,
};

struct mcp16502
{
	struct i2c_dev *i2c_dev;
	struct gpio *lpm_gpio;
};

struct mcp16502_vset_range
{
	unsigned long uv_min;
	unsigned long uv_max;
	unsigned int uv_step;
};

struct mcp16502_reg_desc
{
	const char *name;
	enum mcp16502_reg_id id;
	const struct mcp16502_vset_range *vset_range;
};

struct mcp16502_reg
{
	const struct mcp16502_reg_desc *desc;
	struct mcp16502 *mcp;
};

static TEE_Result mcp16502_rmw(struct mcp16502 *mcp, unsigned int reg_off,
			       uint8_t mask, uint8_t value)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	uint8_t byte;

	res = i2c_smbus_read_byte(mcp->i2c_dev, reg_off, &byte);
	if (res)
		return res;

	byte &= ~mask;
	byte |= value;

	EMSG("Setting reg %d to value %x", reg_off, byte);
	return TEE_SUCCESS;
	// return i2c_smbus_write_byte(mcp->i2c_dev, reg_off, byte);
}

/*
 * mcp16502_get_reg() - get the PMIC's state configuration register for opmode
 *
 * @rdev: the regulator whose register we are searching
 * @opmode: the PMIC's operating mode ACTIVE, Low-power, Hibernate
 */
static int mcp16502_get_state_reg(enum mcp16502_reg_id reg_id, int opmode)
{
	switch (opmode)
	{
	case MCP16502_OPMODE_ACTIVE:
		return MCP16502_REG_BASE(reg_id, A);
	case MCP16502_OPMODE_LPM:
		return MCP16502_REG_BASE(reg_id, LPM);
	case MCP16502_OPMODE_HIB:
		return MCP16502_REG_BASE(reg_id, HIB);
	default:
		return -1;
	}
}

/*
 * _mcp16502_set_mode() - helper for set_mode and set_suspend_mode
 *
 * @rdev: the regulator for which we are setting the mode
 * @mode: the regulator's mode (the one from MODE bit)
 * @opmode: the PMIC's operating mode: Active/Low-power/Hibernate
 */
static TEE_Result _mcp16502_set_mode(struct regulator *reg, unsigned int mode,
				     unsigned int opmode)
{
	struct mcp16502_reg *mcp_reg = reg->priv;
	uint8_t val = 0;
	int reg_off = -1;

	DMSG("_mcp16502_set_mode, %s, mode %u, opmode %u",
			mcp_reg->desc->name, mode, opmode);

	reg_off = mcp16502_get_state_reg(mcp_reg->desc->id, opmode);
	if (reg_off < 0)
		return TEE_ERROR_BAD_PARAMETERS;

	switch (mode)
	{
	case REGULATOR_MODE_NORMAL:
		val = MCP16502_MODE_FPWM;
		break;
	case REGULATOR_MODE_IDLE:
		val = MCP16502_MODE_AUTO_PFM;
		break;
	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}

	return mcp16502_rmw(mcp_reg->mcp, reg_off, MCP16502_MODE, val);
}

static const char *mcp16502_get_name(struct regulator *reg)
{
	struct mcp16502_reg *mcp_reg = reg->priv;

	return mcp_reg->desc->name;
}

static TEE_Result mcp16502_enable(struct regulator *reg)
{
	struct mcp16502_reg *mcp_reg = reg->priv;
	uint32_t reg_off = MCP16502_REG_BASE(mcp_reg->desc->id, A);

	return mcp16502_rmw(mcp_reg->mcp, reg_off, MCP16502_EN, MCP16502_EN);
}

static TEE_Result mcp16502_disable(struct regulator *reg)
{
	struct mcp16502_reg *mcp_reg = reg->priv;
	uint32_t reg_off = MCP16502_REG_BASE(mcp_reg->desc->id, A);

	return mcp16502_rmw(mcp_reg->mcp, reg_off, MCP16502_EN, 0);
}

static TEE_Result mcp16502_map_voltage(struct mcp16502_reg *mcp_reg,
				       unsigned long uv, uint8_t *vset)
{
	const struct mcp16502_vset_range *vset_r = mcp_reg->desc->vset_range;

	if (uv < vset_r->uv_min ||
	    uv > vset_r->uv_max)
		return TEE_ERROR_BAD_PARAMETERS;

	*vset = VDD_LOW_SEL + (uv - vset_r->uv_min) / vset_r->uv_step;

	return TEE_SUCCESS;
}

static TEE_Result vsel_to_voltage(struct mcp16502_reg *mcp_reg,
				  uint8_t vset, unsigned long *uv)
{
	const struct mcp16502_vset_range *vset_r = mcp_reg->desc->vset_range;

	*uv = vset_r->uv_min + vset * vset_r->uv_step;

	return TEE_SUCCESS;
}

static TEE_Result mcp16502_list_voltage(struct regulator *reg,
					unsigned int *count,
					unsigned long *voltage,
					unsigned int offset)
{
	struct mcp16502_reg *mcp_reg = reg->priv;
	const struct mcp16502_vset_range *vset_r = mcp_reg->desc->vset_range;
	unsigned int i = 0;

	if (offset > VSET_COUNT)
		return TEE_ERROR_BAD_PARAMETERS;

	for (i = 0; i < *count; i++)
		voltage[i] = vset_r->uv_min + (offset + i) * vset_r->uv_step;

	return TEE_SUCCESS;
}

static TEE_Result _mcp16502_set_voltage(struct mcp16502_reg *mcp_reg,
					unsigned long uv,
					uint32_t reg_off)
{
	uint8_t vset = 0;
	TEE_Result res = TEE_ERROR_GENERIC;

	DMSG("_mcp16502_set_voltage, %s, uv %lu, reg_off %u",
			mcp_reg->desc->name, uv, reg_off);
	res = mcp16502_map_voltage(mcp_reg, uv, &vset);
	if (res)
		return res;

	return mcp16502_rmw(mcp_reg->mcp, reg_off, MCP16502_VSET_MASK, vset);
}

static TEE_Result mcp16502_set_voltage(struct regulator *reg, unsigned long uv)
{
	struct mcp16502_reg *mcp_reg = reg->priv;
	uint32_t reg_off = MCP16502_REG_BASE(mcp_reg->desc->id, A);

	return _mcp16502_set_voltage(mcp_reg, uv, reg_off);
}

static TEE_Result _mcp16502_get_voltage(struct mcp16502_reg *mcp_reg,
					unsigned long *uv,
					uint32_t reg_off)
{
	uint8_t vset = 0;
	TEE_Result res = TEE_ERROR_GENERIC;

	res = i2c_smbus_read_byte(mcp_reg->mcp->i2c_dev, reg_off, &vset);
	if (res)
		return res;

	vset &= MCP16502_VSET_MASK;

	return vsel_to_voltage(mcp_reg, vset, uv);
}

static TEE_Result mcp16502_get_voltage(struct regulator *reg, unsigned long *uv)
{
	struct mcp16502_reg *mcp_reg = reg->priv;
	uint32_t reg_off = MCP16502_REG_BASE(mcp_reg->desc->id, A);

	return _mcp16502_get_voltage(mcp_reg, uv, reg_off);
}

static TEE_Result mcp16502_set_mode(struct regulator *reg, uint32_t mode)
{
	return _mcp16502_set_mode(reg, mode, MCP16502_OPMODE_ACTIVE);
}

static void mcp16502_gpio_set_lpm_mode(struct mcp16502 *mcp, bool lpm)
{
	gpio_set_value(mcp->lpm_gpio, !lpm);
}

/*
 * mcp16502_gpio_set_mode() - set the GPIO corresponding value
 *
 * Used to prepare transitioning into hibernate or resuming from it.
 */
static void mcp16502_gpio_set_mode(struct mcp16502 *mcp, int mode)
{
	switch (mode)
	{
	case MCP16502_OPMODE_ACTIVE:
		mcp16502_gpio_set_lpm_mode(mcp, false);
		break;
	case MCP16502_OPMODE_LPM:
	case MCP16502_OPMODE_HIB:
		mcp16502_gpio_set_lpm_mode(mcp, true);
		break;
	default:
		EMSG("Invalid mode for mcp16502_gpio_set_mode");
	}
}

/*
 * mcp16502_suspend_get_target_reg() - get the reg of the target suspend PMIC
 *				       mode
 */
static int mcp16502_suspend_get_target_reg(enum mcp16502_reg_id reg_id,
					   int suspend_type)
{
	switch (suspend_type)
	{
	case PM_SUSPEND_STANDBY:
		return mcp16502_get_state_reg(reg_id, MCP16502_OPMODE_LPM);
	case PM_SUSPEND_TO_MEM:
		return mcp16502_get_state_reg(reg_id, MCP16502_OPMODE_HIB);
	default:
		EMSG("Invalid suspend state");
	}

	return -1;
}

static TEE_Result mcp16502_suspend_enable(struct regulator *reg,
					  uint32_t suspend_hint)
{
	struct mcp16502_reg *mcp_reg = reg->priv;
	enum pm_suspend_type suspend_type = PM_HINT_SUSPEND_TYPE(suspend_hint);
	int reg_off = -1;

	reg_off = mcp16502_suspend_get_target_reg(mcp_reg->desc->id,
						  suspend_type);
	if (reg_off < 0)
		return TEE_ERROR_GENERIC;

	return mcp16502_rmw(mcp_reg->mcp, reg_off, MCP16502_EN, MCP16502_EN);
}

static TEE_Result mcp16502_suspend_disable(struct regulator *reg,
					   uint32_t suspend_hint)
{
	struct mcp16502_reg *mcp_reg = reg->priv;
	enum pm_suspend_type suspend_type = PM_HINT_SUSPEND_TYPE(suspend_hint);
	int reg_off = -1;

	reg_off = mcp16502_suspend_get_target_reg(mcp_reg->desc->id,
						  suspend_type);
	if (reg_off < 0)
		return TEE_ERROR_GENERIC;

	return mcp16502_rmw(mcp_reg->mcp, reg_off, MCP16502_EN, 0);
}

static TEE_Result mcp16502_suspend_set_voltage(struct regulator *reg,
					       uint32_t suspend_hint,
					       unsigned long uv)
{
	struct mcp16502_reg *mcp_reg = reg->priv;
	enum pm_suspend_type suspend_type = PM_HINT_SUSPEND_TYPE(suspend_hint);
	int reg_off = -1;

	reg_off = mcp16502_suspend_get_target_reg(mcp_reg->desc->id,
						  suspend_type);
	if (reg_off < 0)
		return TEE_ERROR_GENERIC;

	return _mcp16502_set_voltage(mcp_reg, uv, reg_off);
}

static TEE_Result mcp16502_suspend_set_mode(struct regulator *reg,
					    uint32_t suspend_hint,
					    uint32_t mode)
{
	enum pm_suspend_type suspend_type = PM_HINT_SUSPEND_TYPE(suspend_hint);

	switch (suspend_type)
	{
	case PM_SUSPEND_STANDBY:
		return _mcp16502_set_mode(reg, mode, MCP16502_OPMODE_LPM);
	case PM_SUSPEND_TO_MEM:
		return _mcp16502_set_mode(reg, mode, MCP16502_OPMODE_HIB);
	default:
		EMSG("invalid suspend target: %d\n", suspend_type);
	}

	return TEE_ERROR_BAD_PARAMETERS;
}

static const struct regulator_ops mcp16502_buck_ops = {
	.get_name = mcp16502_get_name,
	.enable = mcp16502_enable,
	.disable = mcp16502_disable,
	.list_voltage = mcp16502_list_voltage,
	.set_voltage = mcp16502_set_voltage,
	.get_voltage = mcp16502_get_voltage,
	.set_mode = mcp16502_set_mode,
#ifdef CFG_PM_ARM32
	.suspend_enable = mcp16502_suspend_enable,
	.suspend_disable = mcp16502_suspend_disable,
	.suspend_set_voltage = mcp16502_suspend_set_voltage,
	.suspend_set_mode = mcp16502_suspend_set_mode,
#endif
};

static const struct regulator_ops mcp16502_ldo_ops = {
	.enable = mcp16502_enable,
	.disable = mcp16502_disable,
	.set_voltage = mcp16502_set_voltage,
	.get_voltage = mcp16502_get_voltage,
#ifdef CFG_PM_ARM32
	.suspend_enable = mcp16502_suspend_enable,
	.suspend_disable = mcp16502_suspend_disable,
	.suspend_set_voltage = mcp16502_suspend_set_voltage,
#endif
};

#define MCP16502_VSET_RANGE(_name, _min, _step)		\
const struct mcp16502_vset_range _name ## _range = {	\
	.uv_min = _min,					\
	.uv_max = _min + VSET_COUNT * _step,		\
	.uv_step = _step,				\
}

MCP16502_VSET_RANGE(buck1_ldo12, 1200000, 50000);
MCP16502_VSET_RANGE(buck234, 600000, 25000);

#define MCP16502_REGULATOR(_name, _id, _ranges) \
	{					\
		.name = _name,			\
		.id = _id,			\
		.vset_range = &_ranges,		\
	}

static const struct mcp16502_reg_desc mcp16502_desc[] = {
	/* MCP16502_REGULATOR(_name, _id, ranges, regulator_ops) */
	MCP16502_REGULATOR("VDD_IO", BUCK1, buck1_ldo12_range),
	MCP16502_REGULATOR("VDD_DDR", BUCK2, buck234_range),
	MCP16502_REGULATOR("VDD_CORE", BUCK3, buck234_range),
	MCP16502_REGULATOR("VDD_OTHER", BUCK4, buck234_range),
	MCP16502_REGULATOR("LDO1", LDO1, buck1_ldo12_range),
	MCP16502_REGULATOR("LDO2", LDO2, buck1_ldo12_range)
};

#ifdef CFG_PM_ARM32
static TEE_Result mcp16502_pm(enum pm_op op, uint32_t pm_hint __unused,
			       const struct pm_callback_handle *hdl __unused)
{ 
	struct mcp16502 *mcp = hdl->handle;

	switch (op) {
	case PM_OP_RESUME:
		mcp16502_gpio_set_mode(mcp, MCP16502_OPMODE_ACTIVE);
		break;
	case PM_OP_SUSPEND:
		mcp16502_gpio_set_mode(mcp, MCP16502_OPMODE_LPM);
		break;
	default:
		break;
	}

	return TEE_SUCCESS;
}

static void mcp16502_pm_init(struct mcp16502 *mcp)
{
	register_pm_driver_cb(mcp16502_pm, mcp, "mcp16502");
}
#else
static void mcp16502_pm_init(struct mcp16502 *mcp)
{
}
#endif

static TEE_Result mcp16502_dt_match_reg(const void *fdt, int node,
					struct regulator *reg, void *data)
{
	int i = 0;
	struct mcp16502_reg *mcp_reg = calloc(1, sizeof(*reg));
	const char *name;

	if (!mcp_reg)
		return TEE_ERROR_OUT_OF_MEMORY;

	mcp_reg->mcp = data;

	name = fdt_getprop(fdt, node, "regulator-name", NULL);
	if (!name)
		return TEE_ERROR_BAD_PARAMETERS;

	for (i = 0; i < MCP16502_REG_COUNT; i++)
	{
		const struct mcp16502_reg_desc *desc = &mcp16502_desc[i];
		if (strcmp(desc->name, name) == 0)
		{
			mcp_reg->desc = desc;
			break;
		}
	}

	if (!mcp_reg->desc)
		return TEE_ERROR_GENERIC;

	if (mcp_reg->desc->id <= BUCK_COUNT)
		reg->ops = &mcp16502_buck_ops;
	else
		reg->ops = &mcp16502_ldo_ops;

	reg->priv = mcp_reg;

	return TEE_SUCCESS;
}

static TEE_Result mcp16502_dt_map_mode(unsigned int dt_mode,
				       unsigned int *reg_mode)
{
	if (dt_mode == REGULATOR_MODE_NORMAL || dt_mode == REGULATOR_MODE_IDLE)
	{
		*reg_mode = dt_mode;
		return TEE_SUCCESS;
	}

	return TEE_ERROR_GENERIC;
}
static const struct regulator_dt_drv_ops mcp16502_drv_ops = {
    .dt_match_reg = mcp16502_dt_match_reg,
    .dt_map_mode = mcp16502_dt_map_mode,
};

static TEE_Result mcp16502_probe_dt(const void *fdt, int node,
				    struct mcp16502 *mcp)
{
	/* The LPM gpio is described as optional in the bindings */
	gpio_dt_get_by_index(fdt, node, 0, "lpm", &mcp->lpm_gpio);
	if (mcp->lpm_gpio)
	{
		gpio_set_direction(mcp->lpm_gpio, GPIO_DIR_OUT);
		gpio_set_value(mcp->lpm_gpio, GPIO_LEVEL_LOW);
	}

	return TEE_SUCCESS;
}

static TEE_Result mcp16502_probe(struct i2c_dev *i2c_dev, const void *fdt,
				 int node, const void *compat_data __unused)
{
	struct mcp16502 *mcp = NULL;
	TEE_Result res = TEE_ERROR_GENERIC;

	mcp = calloc(1, sizeof(struct mcp16502));
	if (!mcp)
		return TEE_ERROR_OUT_OF_MEMORY;

	mcp->i2c_dev = i2c_dev;

	res = mcp16502_probe_dt(fdt, node, mcp);
	if (res)
		return res;

	mcp16502_gpio_set_lpm_mode(mcp, false);

	mcp16502_pm_init(mcp);

	return regulator_register(fdt, node, &mcp16502_drv_ops, mcp);
}

static const struct dt_device_match mcp16502_match_table[] = {
    {.compatible = "microchip,mcp16502"},
    {}};

DEFINE_I2C_DEV_DRIVER(mcp16502, mcp16502_match_table, mcp16502_probe);
