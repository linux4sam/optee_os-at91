// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Bootlin
 */

#include "at91_securam.h"
#include <io.h>
#include <kernel/dt.h>
#include <libfdt.h>
#include <types_ext.h>

#define AT91_SECUMOD_RAMRDY	0x14
#define AT91_SECUMOD_RAMRDY_READY	BIT(0)

static struct securam_data {
	vaddr_t ram_base;
	size_t ram_size;
	size_t ram_alloced;
} securam;

TEE_Result at91_securam_alloc(size_t len, vaddr_t *addr)
{
	if (securam.ram_alloced + len > securam.ram_size)
		return TEE_ERROR_OUT_OF_MEMORY;

	*addr = (securam.ram_base + securam.ram_alloced);
	securam.ram_alloced += len;

	return TEE_SUCCESS;
}

TEE_Result at91_securam_init(const void *fdt)
{
	size_t size = 0;
	vaddr_t secumod_base = 0;
	int node = -1;
	uint32_t val = 0;

	node = fdt_node_offset_by_compatible(fdt, -1, "atmel,sama5d2-securam");
	if (node < 0)
		return TEE_ERROR_ITEM_NOT_FOUND;

	if (dt_map_dev(fdt, node, &securam.ram_base, &securam.ram_size) < 0)
		return TEE_ERROR_GENERIC;

	/* CLOCKS */

	node = fdt_node_offset_by_compatible(fdt, -1, "atmel,sama5d2-secumod");
	if (node < 0)
		return TEE_ERROR_ITEM_NOT_FOUND;

	if (dt_map_dev(fdt, node, &secumod_base, &size) < 0)
		return TEE_ERROR_GENERIC;

	while(1) {
		val = io_read32(secumod_base + AT91_SECUMOD_RAMRDY);
		if (val & AT91_SECUMOD_RAMRDY_READY)
			break;
	}

	dt_unmap_dev(fdt, node, secumod_base, size);

	return TEE_SUCCESS;
}