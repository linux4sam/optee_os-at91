// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Microchip
 */

#include <drivers/nvmem.h>
#include <io.h>
#include <kernel/dt.h>
#include <malloc.h>
#include <matrix.h>
#include <sama5d2.h>
#include <string.h>
#include <tee_api_defines.h>
#include <tee_api_types.h>
#include <types_ext.h>

#define ATMEL_SFC_KR		0x0
#define ATMEL_SFC_SR		0x1C
#define ATMEL_SFC_SR_PGMC	BIT(0)
#define ATMEL_SFC_SR_PGMF	BIT(1)
#define ATMEL_SFC_DR		0x20

#define ATMEL_SFC_CELLS_32	17
#define ATMEL_SFC_CELLS_8	(ATMEL_SFC_CELLS_32 * sizeof(uint32_t))

struct atmel_sfc {
	paddr_t base;
	uint8_t fuses[ATMEL_SFC_CELLS_8];
};

static TEE_Result atmel_sfc_cell_read(struct nvmem_cell *cell, void **data,
				      size_t *len)
{
	uint8_t *data_ptr;
	struct atmel_sfc *atmel_sfc = cell->drv_data;

	data_ptr = malloc(cell->len);
	if (!data_ptr)
		return TEE_ERROR_OUT_OF_MEMORY;

	memcpy(data_ptr, &atmel_sfc->fuses[cell->offset], cell->len);

	*data = data_ptr;
	*len = cell->len;

	return TEE_SUCCESS;
}

static void atmel_sfc_cell_free(struct nvmem_cell *cell)
{
	free(cell);
}

static const struct nvmem_ops atmel_sfc_nvmem_ops = {
	.cell_read = atmel_sfc_cell_read,
	.cell_free = atmel_sfc_cell_free,
};

static struct nvmem_cell *atmel_sfc_dt_get(struct dt_driver_phandle_args *a,
					void *data, TEE_Result *res)
{
	struct nvmem_cell *cell = NULL;
	
	cell = calloc(1, sizeof(*cell));
	if (!cell) {
		*res = TEE_ERROR_OUT_OF_MEMORY;
		return NULL;
	}

	*res = nvmem_cell_parse_dt(a->fdt, a->phandle_node, cell);
	if (*res)
		goto out_free;

	if (cell->offset + cell->len > ATMEL_SFC_CELLS_8) {
		*res = TEE_ERROR_GENERIC;
		goto out_free;
	}

	cell->ops = &atmel_sfc_nvmem_ops;
	cell->drv_data = data;
	*res = TEE_SUCCESS;

	return cell;

out_free:
	free(cell);

	return NULL;
}

static void atmel_sfc_read_fuse(struct atmel_sfc *atmel_sfc)
{
	size_t i = 0;
	uint32_t val = 0;

	for (i = 0; i < ATMEL_SFC_CELLS_32; i++) {
		val = io_read32(atmel_sfc->base + ATMEL_SFC_DR + i * 4);
		memcpy(&atmel_sfc->fuses[i * 4], &val, sizeof(val));
	}
}

static TEE_Result atmel_sfc_probe(const void *fdt, int node,
				  const void *compat_data __unused)
{
	paddr_t base = 0;
	size_t size = 0;
	struct atmel_sfc *atmel_sfc = NULL;

	if (_fdt_get_status(fdt, node) != DT_STATUS_OK_SEC)
		return TEE_ERROR_NODE_DISABLED;

	matrix_configure_periph_secure(AT91C_ID_SFC);

	if (dt_map_dev(fdt, node, &base, &size, DT_MAP_AUTO) < 0)
		return TEE_ERROR_GENERIC;

	atmel_sfc = calloc(1, sizeof(*atmel_sfc));
	if (!atmel_sfc)
		return TEE_ERROR_OUT_OF_MEMORY;

	atmel_sfc->base = base;

	atmel_sfc_read_fuse(atmel_sfc);

	return nvmem_register_provider(fdt, node, atmel_sfc_dt_get, atmel_sfc);
}

static const struct dt_device_match atmel_sfc_match_table[] = {
	{ .compatible = "atmel,sama5d2-sfc" },
	{ }
};

DEFINE_DT_DRIVER(atmel_sfc_dt_driver) = {
	.name = "atmel_sfc",
	.type = DT_DRIVER_NOTYPE,
	.match_table = atmel_sfc_match_table,
	.probe = atmel_sfc_probe,
};
