// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2015-2019, Arm Limited and Contributors. All rights reserved.
 * Copyright (c) 2019-2020, Linaro Limited
 */

#include <drivers/clk.h>
#include <initcall.h>
#include <kernel/boot.h>
#include <drivers/scmi.h>
#include <drivers/scmi-msg.h>
#include <libfdt.h>
#include <stdio.h>

#define SHMEM_NODE_NAME		"shmem"
#define DT_NODE_NAME_LEN	120
#define SCMI_MAX_CHANNEL_COUNT	10

static uint32_t chan_count;
static uint32_t scmi_clk_phandle[SCMI_MAX_CHANNEL_COUNT];
static uint32_t scmi_resetd_phandle[SCMI_MAX_CHANNEL_COUNT];
static uint32_t scmi_voltd_phandle[SCMI_MAX_CHANNEL_COUNT];

struct scmi_cell_props {
	const char *name;
	unsigned int count;
	uint8_t has_cells;
};

static void scmi_set_phandle(unsigned int channel_id, unsigned int protocol_id,
			     unsigned int phandle)
{
	if (channel_id >= SCMI_MAX_CHANNEL_COUNT)
		panic();

	switch (protocol_id) {
	case SCMI_PROTOCOL_ID_CLOCK:
		scmi_clk_phandle[channel_id] = phandle;
		break;
	case SCMI_PROTOCOL_ID_RESET_DOMAIN:
		scmi_resetd_phandle[channel_id] = phandle;
		break;
	case SCMI_PROTOCOL_ID_VOLTAGE_DOMAIN:
		scmi_voltd_phandle[channel_id] = phandle;
		break;
	default:
		break;
	}
}

static int scmi_dt_protocol_to_cells(unsigned int protocol_id,
				     struct scmi_cell_props *props)
{
	switch (protocol_id) {
	case SCMI_PROTOCOL_ID_CLOCK:
		props->count = 1;
		props->name = "clock";
		props->has_cells = 1;
		break;
	case SCMI_PROTOCOL_ID_RESET_DOMAIN:
		props->count = 1;
		props->name = "reset";
		props->has_cells = 1;
		break;
	case SCMI_PROTOCOL_ID_VOLTAGE_DOMAIN:
		props->has_cells = 0;
		props->name = "voltage";
		break;
	default:
		return 1;
	}

	return 0;
}

static int scmi_add_protocols(void *fdt,
			      unsigned int channel_id,
			      int offs)
{
	int ret, prot_id;
	uint32_t phandle;
	const uint8_t *prot_list;
	struct scmi_cell_props props;
	size_t i, prot_count = plat_scmi_protocol_count();
	char tmp_str[40] = "", path_str[DT_NODE_NAME_LEN] = "";

	prot_list = plat_scmi_protocol_list(channel_id);
	for (i = 0; i < prot_count; i++) {
		prot_id = prot_list[i];
		ret = scmi_dt_protocol_to_cells(prot_id, &props);
		if (ret)
			return -1;

		snprintf(tmp_str, sizeof(tmp_str), "scmi%d_%s@%x", channel_id,
			 props.name, prot_id);

		offs = fdt_add_subnode(fdt, offs, tmp_str);
		if (offs < 0)
			return -1;

		ret = fdt_setprop_u32(fdt, offs, "reg", prot_id);
		if (ret < 0)
			return -1;

		if (props.has_cells) {
			snprintf(tmp_str, sizeof(tmp_str), "#%s-cells",
				 props.name);
			ret = fdt_setprop_u32(fdt, offs, tmp_str, props.count);
			if (ret < 0)
				return -1;
		}

		ret = fdt_generate_phandle(fdt, &phandle);
		if (ret < 0)
			return -1;

		ret = fdt_setprop_u32(fdt, offs, "phandle", phandle);
		if (ret < 0)
			return -1;

		scmi_set_phandle(channel_id, prot_id, phandle);

		ret = fdt_get_path(fdt, offs, path_str, sizeof(path_str));
		if (ret < 0)
			return -1;
	}

	return 0;
}

static int add_scmi_mem(void *fdt, struct scmi_msg_channel *chan,
			uint32_t phandle)
{
	char path_str[DT_NODE_NAME_LEN];
	int ret, offs;

	offs = dt_add_reserved_memory(fdt, "scmi_"SHMEM_NODE_NAME,
				      chan->shm_addr.pa, chan->shm_size);
	if (offs < 0)
		return -1;

	ret = fdt_setprop_u32(fdt, offs, "phandle", phandle);
	if (ret < 0)
		return -1;

	ret = fdt_get_path(fdt, offs, path_str, sizeof(path_str));
	if (ret < 0)
		return -1;

	return 0;
}

static int scmi_add_channel(void *fdt,
			    struct scmi_msg_channel *chan,
			    unsigned int channel_id)
{
	uint32_t phandle;
	int offs, ret;
	char tmp_str[DT_NODE_NAME_LEN];

	ret = fdt_generate_phandle(fdt, &phandle);
	if (ret < 0)
		return -1;

	/* And then finally add scmi memories */
	ret = add_scmi_mem(fdt, chan, phandle);
	if (ret < 0) {
		EMSG("Failed to add scmi memory node for channel %d",
		     channel_id);
		return 1;
	}

	snprintf(tmp_str, sizeof(tmp_str), "scmi%d", channel_id);

	offs = dt_add_overlay_fragment(fdt, "/");
	if (offs < 0)
		return -1;

	offs = fdt_add_subnode(fdt, offs, "firmware");
	if (offs < 0)
		return -1;

	offs = fdt_add_subnode(fdt, offs, tmp_str);
	if (offs < 0)
		return -1;

#ifdef CFG_SCMI_MSG_SMT_FASTCALL_ENTRY
	ret = fdt_setprop_string(fdt, offs, "compatible",
				 "arm,scmi-smc");
	if (ret < 0)
		return -1;

	ret = fdt_setprop_u32(fdt, offs, "arm,smc-id", chan->smc_id);
	if (ret < 0)
		return -1;
#else
	ret = fdt_setprop_string(fdt, offs, "compatible",
				 "arm,scmi");
	if (ret < 0)
		return -1;
#endif

	ret = fdt_setprop_u32(fdt, offs, "#address-cells", 1);
	if (ret < 0)
		return -1;

	ret = fdt_setprop_u32(fdt, offs, "#size-cells", 0);
	if (ret < 0)
		return -1;

	ret = fdt_setprop_u32(fdt, offs, SHMEM_NODE_NAME, phandle);
	if (ret < 0)
		return -1;

	ret = fdt_get_path(fdt, offs, tmp_str, sizeof(tmp_str));
	if (ret < 0)
		return -1;

	ret = scmi_add_protocols(fdt, channel_id, offs);
	if (ret < 0)
		return -1;

	/* Add local_fixup for shmem phandle */
	offs = dt_add_fixup_node(fdt, tmp_str);
	if (offs < 0)
		return -1;

	ret = fdt_setprop_u32(fdt, offs, SHMEM_NODE_NAME, 0);
	if (ret < 0)
		return -1;

	return 0;
}

static int scmi_base_update_dt(void)
{
	int ret;
	void *fdt = get_external_dt();
	struct scmi_msg_channel *chan = NULL;

	chan_count = 0;

	while (1) {
		chan = plat_scmi_get_channel(chan_count);
		if (!chan)
			break;

		ret = scmi_add_channel(fdt, chan, chan_count);
		if (ret < 0) {
			EMSG("Failed to add scmi node for channel %d",
			     chan_count);
			return 1;
		}
		chan_count++;
	}

	return 0;
}

static TEE_Result scmi_update_dt(void)
{
	scmi_base_update_dt();
	clk_scmi_update_dt(scmi_clk_phandle, chan_count);

	return TEE_SUCCESS;
}
driver_init_late(scmi_update_dt);
