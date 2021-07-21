// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Bootlin
 */

#include <at91_clk.h>
#include "at91_pm.h"
#include "at91_securam.h"
#include "at91_shdwc.h"
#include <io.h>
#include <kernel/dt.h>
#include <libfdt.h>
#include <mm/core_memprot.h>
#include <stdbool.h>
#include <tee_api_types.h>

#define AT91_MEMCTRL_MC		0
#define AT91_MEMCTRL_SDRAMC	1
#define AT91_MEMCTRL_DDRSDR	2

#define	AT91_PM_STANDBY		0x00
#define AT91_PM_ULP0		0x01
#define AT91_PM_ULP0_FAST	0x02
#define AT91_PM_ULP1		0x03
#define	AT91_PM_BACKUP		0x04

#define AT91_DDRSDRC_LPR	0x1C	/* Low Power Register */
#define		AT91_DDRSDRC_LPCB	(3 << 0)		/* Low-power Configurations */
#define			AT91_DDRSDRC_LPCB_POWER_DOWN		2

struct at91_pm {
	vaddr_t shdwc;
	vaddr_t securam;
	size_t securam_size;
	vaddr_t sfrbu;
	vaddr_t pmc;
	vaddr_t ramc;
	const void *fdt;
};

static struct at91_pm_bu {
	int suspended;
	unsigned long reserved;
	paddr_t canary;
	paddr_t resume;
} *pm_bu;

static uint32_t canary = 0xA5A5A5A5;

static struct at91_pm soc_pm = {0};

static void sama5d3_ddr_standby(void)
{
	uint32_t lpr0 = 0;
	uint32_t saved_lpr0 = 0;

	saved_lpr0 = io_read32(soc_pm.ramc + AT91_DDRSDRC_LPR);
	lpr0 = saved_lpr0 & ~AT91_DDRSDRC_LPCB;
	lpr0 |= AT91_DDRSDRC_LPCB_POWER_DOWN;

	io_write32(soc_pm.ramc + AT91_DDRSDRC_LPR, lpr0);

	cpu_do_idle();

	io_write32(soc_pm.ramc + AT91_DDRSDRC_LPR, saved_lpr0);
}

static void at91_sama5d2_config_shdwc_ws(vaddr_t shdwc, uint32_t *mode,
					uint32_t *polarity)
{
	uint32_t val = 0;

	/* SHDWC.WUIR */
	val = io_read32(shdwc + AT91_SHDW_WUIR);
	*mode |= (val & 0x3ff);
	*polarity |= ((val >> 16) & 0x3ff);
}

static int at91_sama5d2_config_pmc_ws(vaddr_t pmc, uint32_t mode,
				      uint32_t polarity)
{
	io_write32(pmc + AT91_PMC_FSMR, mode);
	io_write32(pmc + AT91_PMC_FSPR, polarity);

	return 0;
}

struct wakeup_source_info {
	unsigned int pmc_fsmr_bit;
	unsigned int shdwc_mr_bit;
	bool set_polarity;
};

static const struct wakeup_source_info ws_info[] = {
	{ .pmc_fsmr_bit = AT91_PMC_FSTT(10),	.set_polarity = true },
	{ .pmc_fsmr_bit = AT91_PMC_RTCAL,	.shdwc_mr_bit = BIT(17) },
	{ .pmc_fsmr_bit = AT91_PMC_USBAL },
	{ .pmc_fsmr_bit = AT91_PMC_SDMMC_CD },
};

struct wakeup_src {
	const char *compatible;
	const struct wakeup_source_info *info;
};

static const struct wakeup_src sama5d2_ws_ids[] = {
	{ .compatible = "atmel,sama5d2-gem",		.info = &ws_info[0] },
	{ .compatible = "atmel,at91rm9200-rtc",		.info = &ws_info[1] },
	{ .compatible = "atmel,sama5d3-udc",		.info = &ws_info[2] },
	{ .compatible = "atmel,at91rm9200-ohci",	.info = &ws_info[2] },
	{ .compatible = "usb-ohci",			.info = &ws_info[2] },
	{ .compatible = "atmel,at91sam9g45-ehci",	.info = &ws_info[2] },
	{ .compatible = "usb-ehci",			.info = &ws_info[2] },
	{ .compatible = "atmel,sama5d2-sdhci",		.info = &ws_info[3] },
	{ /* sentinel */ }
};

static bool dev_is_wakeup_source(const void *fdt, int node)
{
	return fdt_get_property(fdt, node, "wakeup-source", NULL) != NULL;
}

static int at91_pm_config_ws(unsigned int pm_mode, bool set)
{
	const struct wakeup_source_info *wsi;
	const struct wakeup_src *wsrc;
	unsigned int mode = 0;
	unsigned int polarity = 0;
	unsigned int val = 0;
	unsigned int src = 0;
	int node;

	if (pm_mode != AT91_PM_ULP1)
		return 0;

	if (!set) {
		io_write32(soc_pm.pmc + AT91_PMC_FSMR, mode);
		return 0;
	}

	at91_sama5d2_config_shdwc_ws(soc_pm.shdwc, &mode, &polarity);

	val = io_read32(soc_pm.shdwc + AT91_SHDW_MR);

	/* Loop through defined wakeup sources. */
	for (src = 0; src < ARRAY_SIZE(sama5d2_ws_ids); src++) {
		wsrc = &sama5d2_ws_ids[src];
		wsi = wsrc->info;
		node = -1;
		/* FIXMEEEEEEEEEEEEEEEE */
		do {
			if (dev_is_wakeup_source(soc_pm.fdt, node)) {

				/* Check if enabled on SHDWC. */
				if (wsi->shdwc_mr_bit && !(val & wsi->shdwc_mr_bit))
					continue;

				mode |= wsi->pmc_fsmr_bit;
				if (wsi->set_polarity)
					polarity |= wsi->pmc_fsmr_bit;
			}
		} while(0);
	}

	if (mode)
		at91_sama5d2_config_pmc_ws(soc_pm.pmc, mode, polarity);
	else
		EMSG("AT91: PM: no ULP1 wakeup sources found!");

	return mode ? 0 : TEE_ERROR_BAD_STATE;
}


/*
 * Called after processes are frozen, but before we shutdown devices.
 */
// static int at91_pm_begin(suspend_state_t state)
// {
	// switch (state) {
	// case PM_SUSPEND_MEM:
	// 	soc_pm.data.mode = soc_pm.data.suspend_mode;
	// 	break;

	// case PM_SUSPEND_STANDBY:
	// 	soc_pm.data.mode = soc_pm.data.standby_mode;
	// 	break;

	// default:
	// 	soc_pm.data.mode = -1;
	// }

// 	return at91_pm_config_ws(soc_pm.data.mode, true);
// }

/*
 * Verify that all the clocks are correct before entering
 * slow-clock mode.
 */
static int at91_pm_verify_clocks(void)
{
	uint32_t scsr = 0;
	int i = 0;

	scsr = io_read32(soc_pm.pmc + AT91_PMC_SCSR);

	/* USB must not be using PLLB */
	if ((scsr & (AT91SAM926x_PMC_UHP | AT91SAM926x_PMC_UDP)) != 0) {
		EMSG("AT91: PM - Suspend-to-RAM with USB still active");
		return 0;
	}

	/* PCK0..PCK3 must be disabled, or configured to use clk32k */
	for (i = 0; i < 4; i++) {
		uint32_t css;

		if ((scsr & (AT91_PMC_PCK0 << i)) == 0)
			continue;
		css = io_read32(soc_pm.pmc + AT91_PMC_PCKR(i)) & AT91_PMC_CSS;
		if (css != AT91_PMC_CSS_SLOW) {
			EMSG("AT91: PM - Suspend-to-RAM with PCK%d src %d", i, css);
			return 0;
		}
	}

	return 1;
}

// static int at91_pm_enter(suspend_state_t state)
// {

// 	switch (state) {
// 	case PM_SUSPEND_MEM:
// 	case PM_SUSPEND_STANDBY:
// 		/*
// 		 * Ensure that clocks are in a valid state.
// 		 */
// 		if (soc_pm.data.mode >= AT91_PM_ULP0 &&
// 		    !at91_pm_verify_clocks())
// 			goto error;

// 		at91_pm_suspend(state);

// 		break;

// 	case PM_SUSPEND_ON:
// 		cpu_do_idle();
// 		break;

// 	default:
// 		pr_debug("AT91: PM - bogus suspend state %d\n", state);
// 		goto error;
// 	}

// 	return 0;
// }

static TEE_Result at91_pm_dt_dram_init(const void *fdt)
{
	size_t size = 0;
	int node = -1;

	node = fdt_node_offset_by_compatible(fdt, -1, "atmel,sama5d3-ddramc");
	if (node < 0)
		return TEE_ERROR_ITEM_NOT_FOUND;

	if (dt_map_dev(fdt, node, &soc_pm.ramc, &size) < 0)
		return TEE_ERROR_GENERIC;

	return TEE_SUCCESS;
}


static TEE_Result at91_pm_backup_init(const void *fdt)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	size_t size = 0;
	int node = -1;
	vaddr_t alloc;

	node = fdt_node_offset_by_compatible(fdt, -1, "atmel,sama5d2-sfrbu");
	if (node < 0)
		return TEE_ERROR_ITEM_NOT_FOUND;

	if (dt_map_dev(fdt, node, &soc_pm.sfrbu, &size) < 0)
		return TEE_ERROR_GENERIC;

	res = at91_securam_alloc(sizeof(struct at91_pm_bu), &alloc);
	if (res)
		return res;

	pm_bu = (void *) alloc;
	pm_bu->suspended = 0;
	pm_bu->canary = virt_to_phys(&canary);
	// pm_bu->resume = __pa_symbol(cpu_resume);

	return TEE_SUCCESS;
}

	// {
	// 	.uhp_udp_mask = AT91SAM926x_PMC_UHP | AT91SAM926x_PMC_UDP,
	// 	.mckr = 0x30,
	// 	.version = AT91_PMC_V1,
	// },

#define at91_pm_suspend_in_sram_sz 10

static TEE_Result at91_pm_sram_init(void)
{
	// struct gen_pool *sram_pool;
	// paddr_t sram_pbase;
	vaddr_t sram_base;
	TEE_Result res = TEE_ERROR_GENERIC;

	res = at91_securam_alloc(at91_pm_suspend_in_sram_sz, &sram_base);
	if (res) {
		EMSG("%s: unable to alloc sram!\n", __func__);
		return res;
	}

	// sram_pbase = virt_to_phys((void *) sram_base);
	// at91_suspend_sram_fn = __arm_ioremap_exec(sram_pbase,
	// 				at91_pm_suspend_in_sram_sz, false);
	// if (!at91_suspend_sram_fn) {
	// 	pr_warn("SRAM: Could not map\n");
	// 	goto out_put_device;
	// }

	// /* Copy the pm suspend handler to SRAM */
	// at91_suspend_sram_fn = fncpy(at91_suspend_sram_fn,
	// 		&at91_pm_suspend_in_sram, at91_pm_suspend_in_sram_sz);
	// return;

	return TEE_SUCCESS;
}

static TEE_Result at91_pm_init(void)
{
	soc_pm.pmc = at91_pmc_get_base();

	return at91_pm_sram_init();
}

TEE_Result sama5d2_pm_init(const void *fdt, vaddr_t shdwc)
{
	TEE_Result res = TEE_ERROR_GENERIC;

	soc_pm.fdt = fdt;
	soc_pm.shdwc = shdwc;

	res = at91_securam_init(fdt);
	if (res)
		return res;


	res = at91_pm_dt_dram_init(fdt);
	if (res)
		return res;

	res = at91_pm_backup_init(fdt);
	if (res)
		return res;

	res = at91_pm_init();
	if (res)
		return res;

	// soc_pm.ws_ids = sama5d2_ws_ids;
	// soc_pm.config_shdwc_ws = at91_sama5d2_config_shdwc_ws;
	// soc_pm.config_pmc_ws = at91_sama5d2_config_pmc_ws;

	return TEE_SUCCESS;
}
