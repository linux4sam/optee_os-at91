/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021, Bootlin
 */

#ifndef CLK_SCMI_H
#define CLK_SCMI_H

#include <drivers/clk.h>
#include <tee_api_types.h>

TEE_Result clk_scmi_match(struct clk *clk, unsigned int channel_id,
			  unsigned int scmi_id);

#endif /* CLK_SCMI_H */
