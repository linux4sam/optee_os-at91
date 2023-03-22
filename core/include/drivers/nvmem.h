/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2023, Microchip
 */

#ifndef __DRIVERS_NVMEM_H
#define __DRIVERS_NVMEM_H

#include <kernel/dt_driver.h>
#include <tee_api_types.h>
#include <types_ext.h>

struct nvmem_cell;

struct nvmem_ops {
	TEE_Result (*cell_read)(struct nvmem_cell *cell, void **data,
				size_t *len);
	void (*cell_free)(struct nvmem_cell *cell);
};

struct nvmem_cell {
	paddr_t offset;
	size_t len;
	const struct nvmem_ops *ops;
	void *drv_data;
};

typedef struct nvmem_cell *(*nvmem_dt_func)(struct dt_driver_phandle_args *a,
					    void *data, TEE_Result *res);

#ifdef CFG_DRIVERS_NVMEM
/**
 * nvmem_register_provider - Register a nvmem controller
 *
 * @fdt: Device tree to work on
 * @nodeoffset: Node offset of the nvmem controller
 * @get_dt_pinctrl: Callback to match the nvmem controller with a struct
 		    nvmem_cell
 * @data: Data which will be passed to the get_dt_nvmem callback
 * Return a TEE_Result compliant value
 */
static inline
TEE_Result nvmem_register_provider(const void *fdt, int nodeoffset,
				   nvmem_dt_func get_dt_nvmem, void *data)
{
	return dt_driver_register_provider(fdt, nodeoffset,
					   (get_of_device_func)get_dt_nvmem,
					   data, DT_DRIVER_NVMEM);
}

/**
 * nvmem_get_cell_by_name - Obtain a nvmem cell by name
 *
 * @fdt: Device tree to work on
 * @nodeoffset: Node offset of the pin controller
 * @name: name of the nvmem cell to obtain from device-tree
 * @cell: Pointer filled with the retrieved cell, must be freed after use
   using nvmem_cell_free()
 * Return a TEE_Result compliant value
 */
TEE_Result nvmem_get_cell_by_name(const void *fdt, int nodeoffset,
				  const char *name, struct nvmem_cell **cell);

/**
 * nvmem_get_cell_by_index - Obtain a nvmem cell by index
 *
 * @fdt: Device tree to work on
 * @nodeoffset: Node offset of the pin controller
 * @nvmem_index: Index of the nvmem cell to obtain from device-tree
 * @cell: Pointer filled with the retrieved cell, must be freed after use
   using nvmem_cell_free()
 * Return a TEE_Result compliant value
 */
 static inline
TEE_Result nvmem_get_cell_by_index(const void *fdt, int nodeoffset,
				   unsigned int nvmem_index,
				   struct nvmem_cell **cell)
{
	TEE_Result res = TEE_ERROR_GENERIC;

	*cell = dt_driver_device_from_node_idx_prop("nvmem-cells", fdt,
						    nodeoffset, nvmem_index,
						    DT_DRIVER_NVMEM, &res);
	return res;
}

/**
 * nvmem_cell_parse_dt - Parse device-tree information to fill a nvmem cell
 *
 * @fdt: Device tree to work on
 * @nodeoffset: Node offset of the nvmem cell controller
 * @cell: Pointer to cell that will be filled
 * Return a TEE_Result compliant value
 */
TEE_Result nvmem_cell_parse_dt(const void *fdt, int nodeoffset,
			       struct nvmem_cell *cell);

/**
 * nvmem_cell_free - Free a nvmem cell that was previously obtained
 *
 * @cell: Cell to be freed
 */
static inline void nvmem_cell_free(struct nvmem_cell *cell)
{
	if (!cell->ops->cell_free)
		return;

	cell->ops->cell_free(cell);
}

/**
 * nvmem_cell_read - Read data from a nvmem cell
 * 
 * @cell: Cell to read from nvmem
 * @data: Data allocated and read from nvmem. Must be free after usage using
 *	  free().
 * @len: Length of the nvmem cell that was read
 * Return a TEE_Result compliant value
 */
static inline
TEE_Result nvmem_cell_read(struct nvmem_cell *cell, void **data, size_t *len)
{
	if (!cell->ops->cell_read)
		return TEE_ERROR_NOT_SUPPORTED;

	return cell->ops->cell_read(cell, data, len);
}

#else /* CFG_DRIVERS_NVMEM */
static inline
TEE_Result nvmem_register_provider(const void *fdt __unused,
				   int nodeoffset __unused,
				   nvmem_dt_func get_dt_nvmem __unused,
				   void *data __unused)
{
	return TEE_ERROR_NOT_SUPPORTED;
}

static inline
TEE_Result nvmem_get_cell_by_name(const void *fdt __unused,
				  int nodeoffset __unused,
				  const char *name __unused,
				  struct nvmem_cell **cell __unused)
{
	return TEE_ERROR_NOT_SUPPORTED;
}

static inline
TEE_Result nvmem_get_cell_by_index(const void *fdt __unused,
				   int nodeoffset __unused,
				   unsigned int nvmem_index __unused,
				   struct nvmem_cell **cell __unused)
{
	return TEE_ERROR_NOT_SUPPORTED;
}

static inline
TEE_Result nvmem_cell_parse_dt(const void *fdt __unused,
			       int nodeoffset __unused,
			       struct nvmem_cell *cell __unused)
{
	return TEE_ERROR_NOT_SUPPORTED;
}

static inline void nvmem_cell_free(struct nvmem_cell *cell __unused)
{
}
#endif /* CFG_DRIVERS_NVMEM */
#endif /* __DRIVERS_NVMEM_H */
