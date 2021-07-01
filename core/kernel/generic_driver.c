// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Bootlin
 */

#include <compiler.h>
#include <console.h>
#include <drivers/serial.h>
#include <generic_driver.h>
#include <kernel/dt.h>
#include <kernel/boot.h>
#include <kernel/panic.h>
#include <libfdt.h>
#include <stdlib.h>
#include <string.h>
#include <string_ext.h>

static void driver_generic_probe_node(const void *fdt, int node, int status)
{
	TEE_Result ret;
	const struct dt_driver *drv = NULL;
	const struct generic_driver *gdrv = NULL;

	drv = dt_find_compatible_driver(fdt, node);
	if (!drv || drv->type != DT_DRIVER_GENERIC)
		return;

	gdrv = (const struct generic_driver *)drv->driver;

	ret = gdrv->setup(fdt, node, status);
	if (ret)
		EMSG("Failed to probe driver %s for device %s, err %d",
			drv->name, fdt_get_name(fdt, node, NULL), ret);
}

static void driver_generic_probe_child(const void *fdt, int parent_node)
{
	int child = 0;
	int status = 0;

	/*
	 * Iterate over all nodes recursively and try to find a generic driver
	 * to handle nodes
	 */
	fdt_for_each_subnode(child, fdt, parent_node) {
		status = _fdt_get_status(fdt, child);
		if (!status)
			continue;

		driver_generic_probe_node(fdt, child, status);

		driver_generic_probe_child(fdt, child);
	}
}

static TEE_Result driver_generic_init(void)
{
	const void *fdt = get_embedded_dt();
	if (!fdt)
		panic();

	driver_generic_probe_child(fdt, -1);

	return TEE_SUCCESS;
}
driver_init(driver_generic_init);