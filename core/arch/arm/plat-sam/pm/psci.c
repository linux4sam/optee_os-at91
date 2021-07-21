// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Bootlin
 */

#include "at91_rstc.h"
#include "at91_shdwc.h"
#include <console.h>
#include <kernel/panic.h>
#include <sm/psci.h>
#include <sm/std_smc.h>
#include <stdint.h>
#include <trace.h>

int psci_system_suspend(uintptr_t entry __unused,
			uint32_t context_id __unused,
			struct sm_nsec_ctx *nsec)
{
	DMSG("system suspend");

	/* Prepare suspend */

	/* Enter suspend */

	/* Exit suspend */

	nsec->mon_lr = entry;

	return PSCI_RET_SUCCESS;
}

void __noreturn psci_system_off(void)
{
	if (!at91_shdwc_available())
		panic();

	if (TRACE_LEVEL >= TRACE_DEBUG)
		console_flush();

	at91_shdwc_shutdown();
}

void __noreturn psci_system_reset(void)
{
	if (!at91_rstc_available())
		panic();

	at91_rstc_reset();
}

int psci_features(uint32_t psci_fid)
{
	switch (psci_fid) {
	case ARM_SMCCC_VERSION:
	case PSCI_PSCI_FEATURES:
	case PSCI_VERSION:
		return PSCI_RET_SUCCESS;
	case PSCI_SYSTEM_RESET:
		if (at91_rstc_available())
			return PSCI_RET_SUCCESS;
		return PSCI_RET_NOT_SUPPORTED;
	case PSCI_SYSTEM_OFF:
		if (at91_shdwc_available())
			return PSCI_RET_SUCCESS;
		return PSCI_RET_NOT_SUPPORTED;
	default:
		return PSCI_RET_NOT_SUPPORTED;
	}
}

uint32_t psci_version(void)
{
	return PSCI_VERSION_1_0;
}
