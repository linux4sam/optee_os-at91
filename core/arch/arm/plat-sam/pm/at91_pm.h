/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021, Bootlin
 */

#ifndef AT91_PM_H
#define AT91_PM_H

#define	AT91_PM_STANDBY		0x00
#define AT91_PM_ULP0		0x01
#define AT91_PM_ULP0_FAST	0x02
#define AT91_PM_ULP1		0x03
#define	AT91_PM_BACKUP		0x04

#ifndef __ASSEMBLER__

#include <kernel/thread.h>
#include <sm/sm.h>
#include <tee_api_types.h>
#include <types_ext.h>

struct at91_pm_data {
	vaddr_t shdwc;
	vaddr_t securam;
	vaddr_t secumod;
	vaddr_t sfrbu;
	vaddr_t pmc;
	vaddr_t ramc;
	unsigned int mode;
	const void *fdt;
};

#if defined(CFG_AT91_PM)


extern void at91_pm_suspend_in_sram(struct at91_pm_data *pm_data);
extern void at91_pm_cpu_resume(void);

void at91_pm_resume(struct at91_pm_data *pm_data);

static inline bool at91_pm_suspend_available(void)
{
	return true;
}

TEE_Result at91_pm_suspend(uintptr_t entry, struct sm_nsec_ctx *nsec);

void at91_pm_cpu_idle(void);

TEE_Result sama5d2_pm_init(const void *fdt, vaddr_t shdwc);

void at91_pm_set_suspend_mode(struct thread_smc_args *args);

void at91_pm_get_suspend_mode(struct thread_smc_args *args);

#else

static inline void at91_pm_cpu_idle(void) {}

static inline bool at91_pm_suspend_available(void)
{
	return false;
}

static inline TEE_Result at91_pm_suspend(uintptr_t entry __unused,
					 struct sm_nsec_ctx *nsec __unused)
{
	return TEE_ERROR_NOT_SUPPORTED;
}

static inline TEE_Result sama5d2_pm_init(const void *fdt, vaddr_t shdwc)
{
	return TEE_SUCCESS;
}

#endif

#endif /* __ASSEMBLER__ */

#endif /* AT91_PM_H */