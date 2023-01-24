// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Microchip
 */

#include <drivers/regulator.h>
#include <drivers/scmi.h>
#include <drivers/scmi-msg.h>
#include <kernel/boot.h>
#include <libfdt.h>
#include <string.h>
#include <sys/queue.h>

struct regulator;

struct scmi_regulator {
	struct regulator *reg;
	unsigned int channel_id;
	unsigned int scmi_id;
	bool enabled;
	SLIST_ENTRY(scmi_regulator) link;
};

static SLIST_HEAD(, scmi_regulator) scmi_regulator_list =
				SLIST_HEAD_INITIALIZER(scmi_regulator_list);

#define SCMI_MAX_REGULATOR_NAME_LEN	16

static TEE_Result scmi_regulator_check_id(struct regulator *new_regulator,
				    unsigned int channel_id,
				    unsigned int scmi_id)
{
	struct scmi_regulator *regulator = NULL;

	SLIST_FOREACH(regulator, &scmi_regulator_list, link) {
		if (regulator->channel_id == channel_id && regulator->scmi_id == scmi_id) {
			EMSG("Clock for SCMI channel %d, id %d already registered !",
			     channel_id, scmi_id);
			return TEE_ERROR_BAD_PARAMETERS;
		}
	}

	if (strlen(regulator_get_name(new_regulator)) >= SCMI_MAX_REGULATOR_NAME_LEN)
		return TEE_ERROR_BAD_PARAMETERS;

	return TEE_SUCCESS;
}

TEE_Result scmi_regulator_add(struct regulator *reg,
			      unsigned int channel_id, unsigned int scmi_id)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	struct scmi_regulator *scmi_regulator = NULL;

	res = scmi_regulator_check_id(reg, channel_id, scmi_id);
	if (res)
		return res;

	scmi_regulator = calloc(1, sizeof(*scmi_regulator));
	if (!scmi_regulator)
		return TEE_ERROR_OUT_OF_MEMORY;

	scmi_regulator->reg = reg;
	scmi_regulator->channel_id = channel_id;
	scmi_regulator->scmi_id = scmi_id;
	scmi_regulator->enabled = 0;

	SLIST_INSERT_HEAD(&scmi_regulator_list, scmi_regulator, link);

	return TEE_SUCCESS;
}

static struct scmi_regulator *scmi_regulator_get_by_id(unsigned int channel_id,
					   unsigned int scmi_id)
{
	struct scmi_regulator *regulator = NULL;

	SLIST_FOREACH(regulator, &scmi_regulator_list, link) {
		if (regulator->channel_id == channel_id &&
		    regulator->scmi_id == scmi_id)
			return regulator;
	}

	return NULL;
}

size_t plat_scmi_voltd_count(unsigned int channel_id __unused)
{
	unsigned int count = 0;
	unsigned int max_id = 0;
	struct scmi_regulator *regulator = NULL;

	SLIST_FOREACH(regulator, &scmi_regulator_list, link) {
		if (regulator->channel_id != channel_id)
			continue;
		if (regulator->scmi_id > max_id)
			max_id = regulator->scmi_id;
		count++;
	}

	if (count == 0)
		return 0;

	/* IDs are starting from 0 so we need to return id + 1 for count */
	return max_id + 1;
}

const char *plat_scmi_voltd_get_name(unsigned int channel_id,
					    unsigned int scmi_id)
{
	struct scmi_regulator *regulator = NULL;

	regulator = scmi_regulator_get_by_id(channel_id, scmi_id);
	if (!regulator)
		return NULL;

	return regulator_get_name(regulator->reg);
}

int32_t plat_scmi_voltd_levels_array(unsigned int channel_id __unused,
					    unsigned int scmi_id __unused,
					    size_t start_index __unused,
					    long *levels __unused,
					    size_t *nb_elts __unused)
{
	return SCMI_NOT_SUPPORTED;
}

int32_t plat_scmi_voltd_levels_by_step(unsigned int channel_id __unused,
					      unsigned int scmi_id __unused,
					      long *steps __unused)
{
	return SCMI_NOT_SUPPORTED;
}

int32_t plat_scmi_voltd_get_level(unsigned int channel_id,
					 unsigned int scmi_id,
					 long *level)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	struct scmi_regulator *regulator = NULL;

	regulator = scmi_regulator_get_by_id(channel_id, scmi_id);
	if (!regulator)
		return SCMI_INVALID_PARAMETERS;

	res = regulator_get_voltage(regulator->reg, (unsigned long *) level);
	if (res)
		return SCMI_GENERIC_ERROR;

	return SCMI_SUCCESS;
}

int32_t plat_scmi_voltd_set_level(unsigned int channel_id,
					 unsigned int scmi_id,
					 long microvolt)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	struct scmi_regulator *regulator = NULL;

	regulator = scmi_regulator_get_by_id(channel_id, scmi_id);
	if (!regulator)
		return SCMI_INVALID_PARAMETERS;

	res = regulator_set_voltage(regulator->reg, (uint32_t) microvolt);
	if (res)
		return SCMI_GENERIC_ERROR;

	return SCMI_SUCCESS;
}

int32_t plat_scmi_voltd_get_config(unsigned int channel_id ,
				   unsigned int scmi_id,
				   uint32_t *config)
{
	bool enabled = false;
	struct scmi_regulator *regulator = NULL;

	regulator = scmi_regulator_get_by_id(channel_id, scmi_id);
	if (!regulator)
		return SCMI_INVALID_PARAMETERS;

	enabled = regulator_is_enabled(regulator->reg);
	*config = enabled ? SCMI_VOLTAGE_DOMAIN_CONFIG_ARCH_ON :
			    SCMI_VOLTAGE_DOMAIN_CONFIG_ARCH_OFF;

	return SCMI_SUCCESS;
}

int32_t plat_scmi_voltd_set_config(unsigned int channel_id,
					  unsigned int scmi_id,
					  uint32_t config)
{
	struct scmi_regulator *regulator = NULL;

	regulator = scmi_regulator_get_by_id(channel_id, scmi_id);
	if (!regulator)
		return SCMI_INVALID_PARAMETERS;

	if (config == SCMI_VOLTAGE_DOMAIN_CONFIG_ARCH_ON) {
		if (!regulator->enabled) {
			if (regulator_enable(regulator->reg))
				return SCMI_GENERIC_ERROR;
			regulator->enabled = true;
		}
	} else if (config == SCMI_VOLTAGE_DOMAIN_CONFIG_ARCH_OFF) {
		if (regulator->enabled) {
			regulator_disable(regulator->reg);
			regulator->enabled = false;
		}
	}

	return SCMI_NOT_SUPPORTED;
}
