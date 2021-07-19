// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Bootlin
 */

#include <at91_clk.h>
#include <at91_pm.h>
#include <at91_shdwc.h>
#include <compiler.h>
#include <generic_driver.h>
#include <io.h>
#include <kernel/dt.h>
#include <libfdt.h>
#include <stdbool.h>
#include <tee_api_defines.h>
#include <tee_api_types.h>
#include <trace.h>
#include <types_ext.h>
#include <util.h>

#define	AT91_DDRSDRC_MDR	0x20	/* Memory Device Register */
#define		AT91_DDRSDRC_MD		(7 << 0)	/* Memory Device Type */
#define			AT91_DDRSDRC_MD_SDR		0
#define			AT91_DDRSDRC_MD_LOW_POWER_SDR	1
#define			AT91_DDRSDRC_MD_LOW_POWER_DDR	3
#define			AT91_DDRSDRC_MD_LPDDR3		5
#define			AT91_DDRSDRC_MD_DDR2		6	/* [SAM9 Only] */
#define			AT91_DDRSDRC_MD_LPDDR2		7

#define AT91_DDRSDRC_LPR	0x1C	/* Low Power Register */
#define		AT91_DDRSDRC_LPDDR2_PWOFF	(1 << 3)	/* LPDDR Power Off */

/* SHDWC */
#define SLOW_CLOCK_FREQ	32768ULL

#define SHDW_WK_PIN(reg, cfg)	((reg) & AT91_SHDW_WKUPIS((cfg)->wkup_pin_input))
#define SHDW_RTCWK(reg, cfg)	(((reg) >> ((cfg)->sr_rtcwk_shift)) & 0x1)
#define SHDW_RTTWK(reg, cfg)	(((reg) >> ((cfg)->sr_rttwk_shift)) & 0x1)
#define SHDW_RTCWKEN(cfg)	(1 << ((cfg)->mr_rtcwk_shift))
#define SHDW_RTTWKEN(cfg)	(1 << ((cfg)->mr_rttwk_shift))

#define DBC_PERIOD_US(x)	DIV_ROUND_UP((1000000ULL * (x)), \
							SLOW_CLOCK_FREQ)


static vaddr_t shdwc_base = 0;
static vaddr_t mpddrc_base = 0;

bool at91_shdwc_available(void)
{
	return shdwc_base != 0;
}

void __noreturn at91_shdwc_shutdown(void)
{
	vaddr_t pmc_base = at91_pmc_get_base();

	asm volatile(
		/* Align to cache lines */
		".balign 32\n\t"

		/* Ensure AT91_SHDW_CR is in the TLB by reading it */
		"	ldr	r6, [%2, #" TO_STR(AT91_SHDW_CR) "]\n\t"

		/* Power down SDRAM0 */
		"	tst	%0, #0\n\t"
		"	beq	1f\n\t"
		"	str	%1, [%0, #" TO_STR(AT91_DDRSDRC_LPR) "]\n\t"

		/* Switch the master clock source to slow clock. */
		"1:	ldr	r6, [%4, %5]\n\t"
		"	bic	r6, r6,  #" TO_STR(AT91_PMC_CSS) "\n\t"
		"	str	r6, [%4, %5]\n\t"
		/* Wait for clock switch. */
		"2:	ldr	r6, [%4, #" TO_STR(AT91_PMC_SR) "]\n\t"
		"	tst	r6, #"	    TO_STR(AT91_PMC_MCKRDY) "\n\t"
		"	beq	2b\n\t"

		/* Shutdown CPU */
		"	str	%3, [%2, #" TO_STR(AT91_SHDW_CR) "]\n\t"

		"	b	.\n\t"
		:
		: "r" (mpddrc_base),
		  "r" (AT91_DDRSDRC_LPDDR2_PWOFF),
		  "r" (shdwc_base),
		  "r" (AT91_SHDW_KEY | AT91_SHDW_SHDW),
		  "r" (pmc_base),
		  "r" (AT91_PMC_MCKR)
		: "r6");

	while(1);
}

static const unsigned long long sdwc_dbc_period[] = {
	0, 3, 32, 512, 4096, 32768,
};

static uint32_t at91_shdwc_debouncer_value(uint32_t in_period_us)
{
	int i = 0;
	int max_idx = ARRAY_SIZE(sdwc_dbc_period) - 1;
	unsigned long long period_us = 0;
	unsigned long long max_period_us = DBC_PERIOD_US(sdwc_dbc_period[max_idx]);

	if (in_period_us > max_period_us) {
		DMSG("debouncer period %u too big, reduced to %llu us\n",
		     in_period_us, max_period_us);
		return max_idx;
	}

	for (i = max_idx - 1; i > 0; i--) {
		period_us = DBC_PERIOD_US(sdwc_dbc_period[i]);
		DMSG("%s: ref[%d] = %llu\n", __func__, i, period_us);
		if (in_period_us > period_us)
			break;
	}

	return i + 1;
}

static uint32_t at91_shdwc_get_wakeup_input(const void *fdt, int np)
{
	const uint32_t *prop = NULL;
	uint32_t wk_input_mask = 0;
	uint32_t wuir = 0;
	uint32_t wk_input = 0;
	int child = 0;
	int len = 0;

	fdt_for_each_subnode(child, fdt, np) {
		prop = fdt_getprop(fdt, child, "reg", &len);
		if (!prop || len != sizeof(uint32_t)) {
			DMSG("reg property is missing for node %s\n",
			     fdt_get_name(fdt, child, NULL));
			continue;
		}
		wk_input = fdt32_to_cpu(*prop);
		wk_input_mask = BIT32(wk_input);
		if (!(wk_input_mask & AT91_SHDW_WKUPEN_MASK)) {
			DMSG("wake-up input %d out of bounds ignore\n",
			     wk_input);
			continue;
		}
		wuir |= wk_input_mask;

		if (fdt_getprop(fdt, child, "atmel,wakeup-active-high", NULL))
			wuir |= AT91_SHDW_WKUPT(wk_input);

		DMSG("%s: (child %d) wuir = %#x\n", __func__, wk_input, wuir);
	}

	return wuir;
}

static int at91_shdwc_dt_configure(const void *fdt, int np)
{
	const uint32_t *prop = NULL;
	uint32_t mode = 0;
	uint32_t tmp = 0;
	uint32_t input = 0;
	int len = 0;

	prop = fdt_getprop(fdt, np, "debounce-delay-us", &len);
	if (prop && len == sizeof(uint32_t)) {
		tmp = fdt32_to_cpu(*prop);
		mode |= AT91_SHDW_WKUPDBC(at91_shdwc_debouncer_value(tmp));
	}

	if (fdt_getprop(fdt, np, "atmel,wakeup-rtc-timer", &len))
		mode |= AT91_SHDW_RTCWKEN;

	DMSG("%s: mode = %#x\n", __func__, mode);
	io_write32(shdwc_base + AT91_SHDW_MR, mode);

	input = at91_shdwc_get_wakeup_input(fdt, np);
	io_write32(shdwc_base + AT91_SHDW_WUIR, input);

	return TEE_SUCCESS;
}

static TEE_Result shdwc_setup(const void *fdt, int nodeoffset,
				 int status __unused)
{
	int ddr_node = 0;
	size_t size = 0;
	uint32_t ddr = AT91_DDRSDRC_MD_LPDDR2;

	if (dt_map_dev(fdt, nodeoffset, &shdwc_base, &size) < 0)
		return TEE_ERROR_GENERIC;

	ddr_node = fdt_node_offset_by_compatible(fdt, -1,
						 "atmel,sama5d3-ddramc");
	if (ddr_node < 0)
		return TEE_ERROR_GENERIC;

	if (dt_map_dev(fdt, ddr_node, &mpddrc_base, &size) < 0)
		return TEE_ERROR_GENERIC;

	ddr = io_read32(mpddrc_base + AT91_DDRSDRC_MDR) & AT91_DDRSDRC_MD;
	if (ddr != AT91_DDRSDRC_MD_LPDDR2 && ddr != AT91_DDRSDRC_MD_LPDDR3)
		mpddrc_base = 0;

	at91_shdwc_dt_configure(fdt, nodeoffset);

	sama5d2_pm_init(fdt, shdwc_base);

	return TEE_SUCCESS;
}

static const struct generic_driver shdwc_driver = {
	.setup = shdwc_setup,
};

static const struct dt_device_match shdwc_match_table[] = {
	{ .compatible = "atmel,sama5d2-shdwc" },
	{ 0 }
};

const struct dt_driver shdwc_dt_driver __dt_driver = {
	.name = "shdwc",
	.type = DT_DRIVER_GENERIC,
	.match_table = shdwc_match_table,
	.driver = &shdwc_driver,
};
