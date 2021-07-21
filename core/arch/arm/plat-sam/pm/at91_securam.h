/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021, Bootlin
 */

#ifndef AT91_SECURAM_H
#define AT91_SECURAM_H

#include <tee_api_types.h>
#include <types_ext.h>

#if defined(CFG_AT91_SECURAM)

TEE_Result at91_securam_alloc(size_t len, vaddr_t *addr);

TEE_Result at91_securam_init(const void *fdt);

#else

static inline TEE_Result at91_securam_alloc(size_t len, vaddr_t *addr)
{
	return TEE_ERROR_NOT_SUPPORTED;
}

static inline TEE_Result at91_securam_init(const void *fdt)
{
	return TEE_ERROR_NOT_SUPPORTED;
}

#endif

#endif /* AT91_SECURAM_H */