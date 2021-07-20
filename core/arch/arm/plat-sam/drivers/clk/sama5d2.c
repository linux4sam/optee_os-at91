// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Bootlin
 */
#include <assert.h>
#include <initcall.h>
#include <kernel/boot.h>
#include <libfdt.h>
#include <kernel/dt.h>
#include <kernel/panic.h>
#include <matrix.h>
#include <sama5d2.h>
#include <stdint.h>
#include <util.h>

#include "at91_clk.h"

#include <dt-bindings/clock/at91.h>

struct sam_clk {
	const char *n;
	uint8_t id;
	uint8_t scmi_id;
};

static const struct clk_master_characteristics mck_characteristics = {
	.output = { .min = 124000000, .max = 166000000 },
	.divisors = { 1, 2, 4, 3 },
};

static uint8_t plla_out[] = { 0 };

static uint16_t plla_icpll[] = { 0 };

static const struct clk_range plla_outputs[] = {
	{ .min = 600000000, .max = 1200000000 },
};

static const struct clk_pll_characteristics plla_characteristics = {
	.input = { .min = 12000000, .max = 24000000 },
	.num_output = ARRAY_SIZE(plla_outputs),
	.output = plla_outputs,
	.icpll = plla_icpll,
	.out = plla_out,
};

static const struct clk_pcr_layout sama5d2_pcr_layout = {
	.offset = 0x10c,
	.cmd = BIT(12),
	.gckcss_mask = GENMASK_32(10, 8),
	.pid_mask = GENMASK_32(6, 0),
};

static const struct clk_programmable_layout sama5d2_programmable_layout = {
	.pres_mask = 0xff,
	.pres_shift = 4,
	.css_mask = 0x7,
	.have_slck_mck = 0,
	.is_pres_direct = 1,
};

struct sam_clk sama5d2_systemck[] = {
	{.n = "ddrck", .id = 2, .scmi_id = AT91_SCMI_CLK_SYSTEM_DDRCK },
	{.n = "lcdck", .id = 3, .scmi_id = AT91_SCMI_CLK_SYSTEM_LCDCK },
	{.n = "uhpck", .id = 6, .scmi_id = AT91_SCMI_CLK_SYSTEM_UHPCK },
	{.n = "udpck", .id = 7, .scmi_id = AT91_SCMI_CLK_SYSTEM_UDPCK },
	{.n = "pck0",  .id = 8, .scmi_id = AT91_SCMI_CLK_SYSTEM_PCK0 },
	{.n = "pck1",  .id = 9, .scmi_id = AT91_SCMI_CLK_SYSTEM_PCK1 },
	{.n = "pck2",  .id = 10, .scmi_id = AT91_SCMI_CLK_SYSTEM_PCK2 },
	{.n = "iscck", .id = 18, .scmi_id = AT91_SCMI_CLK_SYSTEM_ISCCK },
};

static const struct {
	struct sam_clk clk;
	struct clk_range r;
} sama5d2_periph32ck[] = {
	{ {.n = "macb0_clk",   .id = 5, .scmi_id = AT91_SCMI_CLK_PERIPH32_MACB0_CLK },  .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "tdes_clk",    .id = 11, .scmi_id = AT91_SCMI_CLK_PERIPH32_TDES_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "matrix1_clk", .id = 14, .scmi_id = AT91_SCMI_CLK_PERIPH32_MATRIX1_CLK }, },
	{ {.n = "hsmc_clk",    .id = 17, .scmi_id = AT91_SCMI_CLK_PERIPH32_HSMC_CLK }, },
	{ {.n = "pioA_clk",    .id = 18, .scmi_id = AT91_SCMI_CLK_PERIPH32_PIOA_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "flx0_clk",    .id = 19, .scmi_id = AT91_SCMI_CLK_PERIPH32_FLX0_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "flx1_clk",    .id = 20, .scmi_id = AT91_SCMI_CLK_PERIPH32_FLX1_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "flx2_clk",    .id = 21, .scmi_id = AT91_SCMI_CLK_PERIPH32_FLX2_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "flx3_clk",    .id = 22, .scmi_id = AT91_SCMI_CLK_PERIPH32_FLX3_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "flx4_clk",    .id = 23, .scmi_id = AT91_SCMI_CLK_PERIPH32_FLX4_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "uart0_clk",   .id = 24, .scmi_id = AT91_SCMI_CLK_PERIPH32_UART0_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "uart1_clk",   .id = 25, .scmi_id = AT91_SCMI_CLK_PERIPH32_UART1_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "uart2_clk",   .id = 26, .scmi_id = AT91_SCMI_CLK_PERIPH32_UART2_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "uart3_clk",   .id = 27, .scmi_id = AT91_SCMI_CLK_PERIPH32_UART3_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "uart4_clk",   .id = 28, .scmi_id = AT91_SCMI_CLK_PERIPH32_UART4_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "twi0_clk",    .id = 29, .scmi_id = AT91_SCMI_CLK_PERIPH32_TWI0_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "twi1_clk",    .id = 30, .scmi_id = AT91_SCMI_CLK_PERIPH32_TWI1_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "spi0_clk",    .id = 33, .scmi_id = AT91_SCMI_CLK_PERIPH32_SPI0_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "spi1_clk",    .id = 34, .scmi_id = AT91_SCMI_CLK_PERIPH32_SPI1_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "tcb0_clk",    .id = 35, .scmi_id = AT91_SCMI_CLK_PERIPH32_TCB0_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "tcb1_clk",    .id = 36, .scmi_id = AT91_SCMI_CLK_PERIPH32_TCB1_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "pwm_clk",     .id = 38, .scmi_id = AT91_SCMI_CLK_PERIPH32_PWM_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "adc_clk",     .id = 40, .scmi_id = AT91_SCMI_CLK_PERIPH32_ADC_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "uhphs_clk",   .id = 41, .scmi_id = AT91_SCMI_CLK_PERIPH32_UHPHS_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "udphs_clk",   .id = 42, .scmi_id = AT91_SCMI_CLK_PERIPH32_UDPHS_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "ssc0_clk",    .id = 43, .scmi_id = AT91_SCMI_CLK_PERIPH32_SSC0_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "ssc1_clk",    .id = 44, .scmi_id = AT91_SCMI_CLK_PERIPH32_SSC1_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "trng_clk",    .id = 47, .scmi_id = AT91_SCMI_CLK_PERIPH32_TRNG_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "pdmic_clk",   .id = 48, .scmi_id = AT91_SCMI_CLK_PERIPH32_PDMIC_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "securam_clk", .id = 51, .scmi_id = AT91_SCMI_CLK_PERIPH32_SECURAM_CLK }, },
	{ {.n = "i2s0_clk",    .id = 54, .scmi_id = AT91_SCMI_CLK_PERIPH32_I2S0_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "i2s1_clk",    .id = 55, .scmi_id = AT91_SCMI_CLK_PERIPH32_I2S1_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "can0_clk",    .id = 56, .scmi_id = AT91_SCMI_CLK_PERIPH32_CAN0_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "can1_clk",    .id = 57, .scmi_id = AT91_SCMI_CLK_PERIPH32_CAN1_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "ptc_clk",     .id = 58, .scmi_id = AT91_SCMI_CLK_PERIPH32_PTC_CLK }, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "classd_clk",  .id = 59, .scmi_id = AT91_SCMI_CLK_PERIPH32_CLASSD_CLK }, .r = { .min = 0, .max = 83000000 }, },
};

static struct sam_clk sama5d2_periphck[] = {
	{.n = "dma0_clk",    .id = 6, .scmi_id = AT91_SCMI_CLK_PERIPH_DMA0_CLK },
	{.n = "dma1_clk",    .id = 7, .scmi_id = AT91_SCMI_CLK_PERIPH_DMA1_CLK },
	{.n = "aes_clk",     .id = 9, .scmi_id = AT91_SCMI_CLK_PERIPH_AES_CLK },
	{.n = "aesb_clk",    .id = 10, .scmi_id = AT91_SCMI_CLK_PERIPH_AESB_CLK },
	{.n = "sha_clk",     .id = 12, .scmi_id = AT91_SCMI_CLK_PERIPH_SHA_CLK },
	{.n = "mpddr_clk",   .id = 13, .scmi_id = AT91_SCMI_CLK_PERIPH_MPDDR_CLK },
	{.n = "matrix0_clk", .id = 15, .scmi_id = AT91_SCMI_CLK_PERIPH_MATRIX0_CLK },
	{.n = "sdmmc0_hclk", .id = 31, .scmi_id = AT91_SCMI_CLK_PERIPH_SDMMC0_HCLK },
	{.n = "sdmmc1_hclk", .id = 32, .scmi_id = AT91_SCMI_CLK_PERIPH_SDMMC1_HCLK },
	{.n = "lcdc_clk",    .id = 45, .scmi_id = AT91_SCMI_CLK_PERIPH_LCDC_CLK },
	{.n = "isc_clk",     .id = 46, .scmi_id = AT91_SCMI_CLK_PERIPH_ISC_CLK },
	{.n = "qspi0_clk",   .id = 52, .scmi_id = AT91_SCMI_CLK_PERIPH_QSPI0_CLK },
	{.n = "qspi1_clk",   .id = 53, .scmi_id = AT91_SCMI_CLK_PERIPH_QSPI1_CLK },
};

static const struct {
	struct sam_clk clk;
	struct clk_range r;
	int chg_pid;
} sama5d2_gck[] = {
	{ {.n = "sdmmc0_gclk", .id = 31, .scmi_id = AT91_SCMI_CLK_GCK_SDMMC0_GCLK }, .chg_pid = INT_MIN, },
	{ {.n = "sdmmc1_gclk", .id = 32, .scmi_id = AT91_SCMI_CLK_GCK_SDMMC1_GCLK }, .chg_pid = INT_MIN, },
	{ {.n = "tcb0_gclk",   .id = 35, .scmi_id = AT91_SCMI_CLK_GCK_TCB0_GCLK }, .chg_pid = INT_MIN, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "tcb1_gclk",   .id = 36, .scmi_id = AT91_SCMI_CLK_GCK_TCB1_GCLK }, .chg_pid = INT_MIN, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "pwm_gclk",    .id = 38, .scmi_id = AT91_SCMI_CLK_GCK_PWM_GCLK }, .chg_pid = INT_MIN, .r = { .min = 0, .max = 83000000 }, },
	{ {.n = "isc_gclk",    .id = 46, .scmi_id = AT91_SCMI_CLK_GCK_ISC_GCLK }, .chg_pid = INT_MIN, },
	{ {.n = "pdmic_gclk",  .id = 48, .scmi_id = AT91_SCMI_CLK_GCK_PDMIC_GCLK }, .chg_pid = INT_MIN, },
	{ {.n = "i2s0_gclk",   .id = 54, .scmi_id = AT91_SCMI_CLK_GCK_I2S0_GCLK }, .chg_pid = 5, },
	{ {.n = "i2s1_gclk",   .id = 55, .scmi_id = AT91_SCMI_CLK_GCK_I2S1_GCLK }, .chg_pid = 5, },
	{ {.n = "can0_gclk",   .id = 56, .scmi_id = AT91_SCMI_CLK_GCK_CAN0_GCLK }, .chg_pid = INT_MIN, .r = { .min = 0, .max = 80000000 }, },
	{ {.n = "can1_gclk",   .id = 57, .scmi_id = AT91_SCMI_CLK_GCK_CAN1_GCLK }, .chg_pid = INT_MIN, .r = { .min = 0, .max = 80000000 }, },
	{ {.n = "classd_gclk", .id = 59, .scmi_id = AT91_SCMI_CLK_GCK_CLASSD_GCLK }, .chg_pid = 5, .r = { .min = 0, .max = 100000000 }, },
};

struct sam_clk sama5d2_progck[] = {
	{.n = "prog0", .id = 0, .scmi_id = AT91_SCMI_CLK_PROG_PROG0 },
	{.n = "prog1", .id = 1, .scmi_id = AT91_SCMI_CLK_PROG_PROG1 },
	{.n = "prog2", .id = 2, .scmi_id = AT91_SCMI_CLK_PROG_PROG2 },
};

#define PARENT_SIZE	MAX(ARRAY_SIZE(sama5d2_systemck), 6U)

static struct pmc_data *pmc;

vaddr_t at91_pmc_get_base(void)
{
	assert(pmc);

	return pmc->base;
}

static TEE_Result pmc_setup(const void *fdt, int nodeoffset)
{
	size_t size;
	vaddr_t base;
	unsigned int i;
	int bypass = 0;
	const uint32_t *fdt_prop;
	struct pmc_clk *pmc_clk;
	const struct sam_clk *sam_clk;
	struct clk_range range = CLK_RANGE(0, 0);
	struct clk *h32mxck, *mckdivck, *plladivck, *usbck, *audiopll_pmcck;
	struct clk *parents[PARENT_SIZE], *main_clk, *utmi_clk, *slow_clk, *clk;
	struct clk *main_rc_osc, *main_osc, *main_xtal_clk, *audiopll_fracck;

	if (dt_map_dev(fdt, nodeoffset, &base, &size) < 0)
		panic();

	// if (_fdt_get_status(fdt, nodeoffset) == DT_STATUS_OK_SEC)
	// 	matrix_configure_periph_secure(AT91C_ID_PMC);

	slow_clk = clk_dt_get_by_name(fdt, nodeoffset, "slow_clk");
	if (!slow_clk)
		panic();

	main_xtal_clk = clk_dt_get_by_name(fdt, nodeoffset, "main_xtal");
	if (!main_xtal_clk)
		panic();

	pmc = pmc_data_allocate(PMC_AUDIOPLLCK + 1,
				ARRAY_SIZE(sama5d2_systemck),
				ARRAY_SIZE(sama5d2_periphck) +
				ARRAY_SIZE(sama5d2_periph32ck),
				ARRAY_SIZE(sama5d2_gck),
				ARRAY_SIZE(sama5d2_progck));
	pmc->base = base;
	if (!pmc)
		panic();

	main_rc_osc = pmc_register_main_rc_osc(pmc, "main_rc_osc", 12000000);
	if (!main_rc_osc)
		panic();

	fdt_prop = fdt_getprop(fdt, nodeoffset, "atmel,osc-bypass", NULL);
	if (fdt_prop)
		bypass = fdt32_to_cpu(*fdt_prop);

	main_osc = pmc_register_main_osc(pmc, "main_osc", main_xtal_clk,
					 bypass);
	if (!main_osc)
		panic();

	parents[0] = main_rc_osc;
	parents[1] = main_osc;
	main_clk = at91_clk_register_sam9x5_main(pmc, "mainck", parents, 2);
	if (!main_clk)
		panic();

	pmc_clk = &pmc->chws[PMC_MAIN];
	pmc_clk->clk = main_clk;
	pmc_clk->id = PMC_MAIN;

	clk = at91_clk_register_pll(pmc, "pllack", main_clk, 0,
				    &sama5d3_pll_layout, &plla_characteristics);
	if (!clk)
		panic();
	plladivck = at91_clk_register_plldiv(pmc, "plladivck", clk);
	if (!plladivck)
		panic();

	pmc_clk = &pmc->chws[PMC_PLLACK];
	pmc_clk->clk = plladivck;
	pmc_clk->id = PMC_PLLACK;

	audiopll_fracck = at91_clk_register_audio_pll_frac(pmc, "audiopll_fracck",
							   main_clk);
	if (!audiopll_fracck)
		panic();

	clk = at91_clk_register_audio_pll_pad(pmc, "audiopll_padck",
					      audiopll_fracck);
	if (!clk)
		panic();

	audiopll_pmcck = at91_clk_register_audio_pll_pmc(pmc, "audiopll_pmcck",
							 audiopll_fracck);
	if (!audiopll_pmcck)
		panic();

	pmc_clk = &pmc->chws[PMC_AUDIOPLLCK];
	pmc_clk->clk = audiopll_pmcck;
	pmc_clk->id = PMC_AUDIOPLLCK;

	utmi_clk = at91_clk_register_utmi(pmc, "utmick", main_clk);
	if (!utmi_clk)
		panic();

	pmc_clk = &pmc->chws[PMC_UTMI];
	pmc_clk->clk = utmi_clk;
	pmc_clk->id = PMC_UTMI;

	parents[0] = slow_clk;
	parents[1] = main_clk;
	parents[2] = plladivck;
	parents[3] = utmi_clk;

	clk = at91_clk_register_master_pres(pmc, "masterck_pres", 4,
					    parents,
					    &at91sam9x5_master_layout,
					    &mck_characteristics, INT_MIN);
	if (!clk)
		panic();

	mckdivck = at91_clk_register_master_div(pmc, "masterck_div",
						clk,
						&at91sam9x5_master_layout,
						&mck_characteristics);
	if (!mckdivck)
		panic();

	pmc_clk = &pmc->chws[PMC_MCK];
	pmc_clk->clk = mckdivck;
	pmc_clk->id = PMC_MCK;

	h32mxck = at91_clk_register_h32mx(pmc, "h32mxck", mckdivck);
	if (!h32mxck)
		panic();

	pmc_clk = &pmc->chws[PMC_MCK2];
	pmc_clk->clk = h32mxck;
	pmc_clk->id = PMC_MCK2;

	parents[0] = plladivck;
	parents[1] = utmi_clk;
	usbck = at91sam9x5_clk_register_usb(pmc, "usbck", parents, 2);
	if (!usbck)
		panic();

	if (clk_set_parent(usbck, 1) != TEE_SUCCESS)
		panic();

	clk_set_rate(usbck, 48000000);

	parents[0] = slow_clk;
	parents[1] = main_clk;
	parents[2] = plladivck;
	parents[3] = utmi_clk;
	parents[4] = mckdivck;
	parents[5] = audiopll_pmcck;
	for (i = 0; i < ARRAY_SIZE(sama5d2_progck); i++) {
		sam_clk = &sama5d2_progck[i];
		clk = at91_clk_register_programmable(pmc, sam_clk->n,
						     parents, 6, i,
						     &sama5d2_programmable_layout);
		if (!clk)
			panic();

		pmc_clk = &pmc->pchws[i];
		pmc_clk->clk = clk;
		pmc_clk->id = sam_clk->id;
	}

	/* This array order must match the one in sama5d2_systemck */
	parents[0] = mckdivck;
	parents[1] = mckdivck;
	parents[2] = usbck;
	parents[3] = usbck;
	parents[4] = pmc->pchws[0].clk;
	parents[5] = pmc->pchws[1].clk;
	parents[6] = pmc->pchws[2].clk;
	parents[7] = mckdivck;
	for (i = 0; i < ARRAY_SIZE(sama5d2_systemck); i++) {
		sam_clk = &sama5d2_systemck[i];
		clk = at91_clk_register_system(pmc, sam_clk->n,
					       parents[i],
					       sam_clk->id);
		if (!clk)
			panic();

		pmc_clk = &pmc->shws[i];
		pmc_clk->clk = clk;
		pmc_clk->id = sam_clk->id;
	}

	for (i = 0; i < ARRAY_SIZE(sama5d2_periphck); i++) {
		sam_clk = &sama5d2_periphck[i];
		clk = at91_clk_register_sam9x5_peripheral(pmc,
							  &sama5d2_pcr_layout,
							  sam_clk->n,
							  mckdivck,
							  sam_clk->id,
							  &range);
		if (!clk)
			panic();

		pmc_clk = &pmc->phws[i];
		pmc_clk->clk = clk;
		pmc_clk->id = sam_clk->id;
	}

	for (i = 0; i < ARRAY_SIZE(sama5d2_periph32ck); i++) {
		sam_clk = &sama5d2_periph32ck[i].clk;
		clk = at91_clk_register_sam9x5_peripheral(pmc,
							  &sama5d2_pcr_layout,
							  sam_clk->n,
							  h32mxck,
							  sam_clk->id,
							  &sama5d2_periph32ck[i].r);
		if (!clk)
			panic();

		pmc_clk = &pmc->phws[ARRAY_SIZE(sama5d2_periphck) + i];
		pmc_clk->clk = clk;
		pmc_clk->id = sam_clk->id;
	}

	parents[0] = slow_clk;
	parents[1] = main_clk;
	parents[2] = plladivck;
	parents[3] = utmi_clk;
	parents[4] = mckdivck;
	parents[5] = audiopll_pmcck;
	for (i = 0; i < ARRAY_SIZE(sama5d2_gck); i++) {
		sam_clk = &sama5d2_gck[i].clk;
		clk = at91_clk_register_generated(pmc,
						  &sama5d2_pcr_layout,
						  sam_clk->n,
						  parents, 6,
						  sam_clk->id,
						  &sama5d2_gck[i].r,
						  sama5d2_gck[i].chg_pid);
		if (!clk)
			panic();

		pmc_clk = &pmc->ghws[i];
		pmc_clk->clk = clk;
		pmc_clk->id = sam_clk->id;
	}

	parents[0] = pmc_clk_get_by_name(pmc->phws, pmc->nperiph, "i2s0_clk");
	parents[1] = pmc_clk_get_by_name(pmc->ghws, pmc->ngck, "i2s0_gclk");
	clk = at91_clk_i2s_mux_register("i2s0_muxclk", parents, 2, 0);
	if (!clk)
		panic();

	pmc->chws[PMC_I2S0_MUX].clk = clk;
	pmc->chws[PMC_I2S0_MUX].id = PMC_I2S0_MUX;

	parents[0] = pmc_clk_get_by_name(pmc->phws, pmc->nperiph, "i2s1_clk");
	parents[1] = pmc_clk_get_by_name(pmc->ghws, pmc->ngck, "i2s1_gclk");
	clk = at91_clk_i2s_mux_register("i2s1_muxclk", parents, 2, 1);
	if (!clk)
		panic();

	pmc->chws[PMC_I2S1_MUX].clk = clk;
	pmc->chws[PMC_I2S1_MUX].id = PMC_I2S1_MUX;

	clk_dt_register_clk_provider(fdt, nodeoffset, clk_dt_pmc_get, pmc);

	return TEE_SUCCESS;
}

CLK_DT_DECLARE(sama5d2_clk, "atmel,sama5d2-pmc", pmc_setup);
