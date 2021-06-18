// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Bootlin
 */

#include <drivers/clk.h>
#include <drivers/scmi.h>
#include <drivers/scmi-msg.h>

size_t plat_scmi_clock_count(unsigned int channel_id)
{
	return clk_scmi_get_count(channel_id);
}

const char *plat_scmi_clock_get_name(unsigned int channel_id,
				     unsigned int scmi_id)
{
	struct clk *clk;

	clk = clk_scmi_get_by_id(channel_id, scmi_id);
	if (!clk)
		return NULL;

	return clk_get_name(clk);
}

int32_t plat_scmi_clock_rates_array(unsigned int channel_id,
				    unsigned int scmi_id,
				    size_t start_index,
				    unsigned long *rates,
				    size_t *nb_elts)
{
	struct clk *clk;

	clk = clk_scmi_get_by_id(channel_id, scmi_id);
	if (!clk)
		return SCMI_NOT_FOUND;

	return clk_scmi_get_rates_array(clk, start_index, rates, nb_elts);
}

unsigned long plat_scmi_clock_get_rate(unsigned int channel_id,
				       unsigned int scmi_id)
{
	struct clk *clk;

	clk = clk_scmi_get_by_id(channel_id, scmi_id);
	if (!clk)
		return SCMI_NOT_FOUND;

	return clk_get_rate(clk);
}

int32_t plat_scmi_clock_set_rate(unsigned int channel_id,
				 unsigned int scmi_id,
				 unsigned long rate)
{
	struct clk *clk;

	clk = clk_scmi_get_by_id(channel_id, scmi_id);
	if (!clk)
		return SCMI_NOT_FOUND;

	return clk_set_rate(clk, rate);
}

int32_t plat_scmi_clock_get_state(unsigned int channel_id,
				  unsigned int scmi_id)
{
	struct clk *clk;

	clk = clk_scmi_get_by_id(channel_id, scmi_id);
	if (!clk)
		return SCMI_NOT_FOUND;

	return clk_is_enabled(clk);
}

int32_t plat_scmi_clock_set_state(unsigned int channel_id,
				  unsigned int scmi_id,
				  bool enable_not_disable)
{
	struct clk *clk;

	clk = clk_scmi_get_by_id(channel_id, scmi_id);
	if (!clk)
		return SCMI_NOT_FOUND;

	if (enable_not_disable) {
		if (clk_enable(clk))
			return SCMI_GENERIC_ERROR;
	} else {
		clk_disable(clk);
	}

	return SCMI_SUCCESS;
}
