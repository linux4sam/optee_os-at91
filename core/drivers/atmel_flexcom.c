// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright 2021 Microchip
 */

#include <assert.h>
#include <drivers/i2c.h>
#include <drivers/pinctrl.h>
#include <dt-bindings/mfd/atmel-flexcom.h>
#include <io.h>
#include <kernel/dt_driver.h>
#include <libfdt.h>
#include <matrix.h>
#include <string.h>

static TEE_Result atmel_flexcom_mode(const void *fdt, int node, uint32_t *mode)
{
	const uint32_t *cuint = NULL;
	
	cuint = fdt_getprop(fdt, node, "atmel,flexcom-mode", NULL);
	if (!cuint)
		return TEE_ERROR_BAD_PARAMETERS;

	*mode = fdt32_to_cpu(*cuint);

	if (*mode < ATMEL_FLEXCOM_MODE_USART || *mode > ATMEL_FLEXCOM_MODE_TWI)
		return TEE_ERROR_BAD_PARAMETERS;

	return TEE_SUCCESS;
}

static TEE_Result atmel_flexcom_node_probe(const void *fdt, int node,
				       const void *compat_data __unused,
				       const struct dt_driver *dt_drv __unused)
{
	int status = _fdt_get_status(fdt, node);
	unsigned int matrix_id = 0;
	vaddr_t base = 0;
	size_t size = 0;
	uint32_t mode = 0;
	int subnode = -1;
	TEE_Result res = TEE_ERROR_GENERIC;

	if (status != DT_STATUS_OK_SEC)
		return TEE_SUCCESS;

	res = matrix_dt_get_id(fdt, node, &matrix_id);
	if (res)
		return res;
	
	res = atmel_flexcom_mode(fdt, node, &mode);

	if (dt_map_dev(fdt, node, &base, &size, DT_MAP_AUTO) < 0)
		return TEE_ERROR_GENERIC;

	matrix_configure_periph_secure(matrix_id);

	io_write32(base, mode);

	fdt_for_each_subnode(subnode, fdt, node)
		dt_driver_maybe_add_probe_node(fdt, subnode);

	return TEE_SUCCESS;
}

static const struct dt_device_match atmel_flexcom_match_table[] = {
	{ .compatible = "atmel,sama5d2-flexcom" },
	{ }
};

DEFINE_DT_DRIVER(atmel_flexcom_dt_driver) = {
	.name = "atmel_flexcom",
	.type = DT_DRIVER_NOTYPE,
	.match_table = atmel_flexcom_match_table,
	.probe = atmel_flexcom_node_probe,
};
