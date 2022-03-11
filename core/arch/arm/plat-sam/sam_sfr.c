// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2017 Timesys Corporation.
 * Copyright (C) 2021 Bootlin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <io.h>
#include <kernel/boot.h>
#include <kernel/dt.h>
#include <kernel/tz_ssvce_pl310.h>
#include <libfdt.h>
#include <matrix.h>
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>
#include <sm/sm.h>
#include <smc_ids.h>
#include <types_ext.h>
#include <sama5d2.h>

#include <sam_pl310.h>
#include <sam_sfr.h>

register_phys_mem_pgdir(MEM_AREA_IO_SEC, SFR_BASE, CORE_MMU_PGDIR_SIZE);

vaddr_t sam_sfr_base(void)
{
	static void *va;

	if (cpu_mmu_enabled()) {
		if (!va)
			va = phys_to_virt(SFR_BASE, MEM_AREA_IO_SEC, 1);
		return (vaddr_t)va;
	}
	return SFR_BASE;
}

uint32_t atmel_sfr_read_sn0(void)
{
	return io_read32(sam_sfr_base() + AT91_SFR_SN0);
}

uint32_t atmel_sfr_read_sn1(void)
{
	return io_read32(sam_sfr_base() + AT91_SFR_SN1);
}

void atmel_sfr_set_usb_suspend(bool set)
{
	uint32_t regval = io_read32(sam_sfr_base() + AT91_SFR_OHCIICR);

	if (set)
		regval |= AT91_OHCIICR_USB_SUSPEND;
	else
		regval &= ~AT91_OHCIICR_USB_SUSPEND;

	io_write32(sam_sfr_base() + AT91_SFR_OHCIICR, regval);
}

static TEE_Result atmel_sfr_probe(const void *fdt, int node,
				  const void *compat_data __unused)
{
	if (_fdt_get_status(fdt, node) != DT_STATUS_OK_SEC)
		return TEE_ERROR_GENERIC;
	
	matrix_configure_periph_secure(AT91C_ID_SFR);

	return TEE_SUCCESS;
}

static const struct dt_device_match atmel_sfr_match_table[] = {
	{ .compatible = "atmel,sama5d2-sfr" },
	{ }
};

DEFINE_DT_DRIVER(atmel_sfr_dt_driver) = {
	.name = "atmel_sfr",
	.type = DT_DRIVER_NOTYPE,
	.match_table = atmel_sfr_match_table,
	.probe = atmel_sfr_probe,
};
