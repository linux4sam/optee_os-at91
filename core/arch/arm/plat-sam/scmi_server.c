// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2019, STMicroelectronics
 * Copyright (c) 2021, Microchip
 */
#include <assert.h>
#include <at91_clk.h>
#include <compiler.h>
#include <confine_array_index.h>
#include <drivers/scmi-msg.h>
#include <drivers/scmi.h>
#include <dt-bindings/clock/at91.h>
#include <initcall.h>
#include <mm/core_memprot.h>
#include <mm/core_mmu.h>
#include <platform_config.h>
#include <smc_ids.h>
#include <stdint.h>
#include <speculation_barrier.h>
#include <string.h>
#include <tee_api_defines.h>
#include <util.h>

#define SMT_BUFFER_BASE		CFG_SCMI_SHMEM_START

#if (SMT_BUFFER_BASE + SMT_BUF_SLOT_SIZE > \
	CFG_SCMI_SHMEM_START + CFG_SCMI_SHMEM_SIZE)
#error "SCMI shared memory mismatch"
#endif

register_phys_mem(MEM_AREA_IO_NSEC, CFG_SCMI_SHMEM_START, CFG_SCMI_SHMEM_SIZE);

struct channel_resources {
	struct scmi_msg_channel *channel;
};

static const struct channel_resources scmi_channel[] = {
	[0] = {
		.channel = &(struct scmi_msg_channel){
			.shm_addr = { .pa = SMT_BUFFER_BASE },
			.shm_size = SMT_BUF_SLOT_SIZE,
		},
	},
};

static const struct channel_resources *find_resource(unsigned int channel_id)
{
	assert(channel_id < ARRAY_SIZE(scmi_channel));

	return scmi_channel + channel_id;
}

struct scmi_msg_channel *plat_scmi_get_channel(unsigned int channel_id)
{
	const size_t max_id = ARRAY_SIZE(scmi_channel);
	unsigned int confined_id = confine_array_index(channel_id, max_id);

	if (channel_id >= max_id)
		return NULL;

	return find_resource(confined_id)->channel;
}

static const char vendor[] = "Microchip";
static const char sub_vendor[] = "";

const char *plat_scmi_vendor_name(void)
{
	return vendor;
}

const char *plat_scmi_sub_vendor_name(void)
{
	return sub_vendor;
}

/* Currently supporting only SCMI Base protocol */
static const uint8_t plat_protocol_list[] = {
	SCMI_PROTOCOL_ID_CLOCK,
#ifdef CFG_SCMI_MSG_USE_REGULATOR
	SCMI_PROTOCOL_ID_VOLTAGE_DOMAIN,
#endif
	0 /* Null termination */
};

size_t plat_scmi_protocol_count(void)
{
	const size_t count = ARRAY_SIZE(plat_protocol_list) - 1;

	return count;
}

const uint8_t *plat_scmi_protocol_list(unsigned int channel_id __unused)
{
	return plat_protocol_list;
}

struct sama5d2_pmc_clk {
	unsigned int scmi_id;
	unsigned int pmc_type;
	unsigned int pmc_id;
};

static const struct sama5d2_pmc_clk pmc_clks[] = {
	{AT91_SCMI_CLK_CORE_MCK, PMC_TYPE_CORE, PMC_MCK},
	{AT91_SCMI_CLK_CORE_UTMI, PMC_TYPE_CORE, PMC_UTMI},
	{AT91_SCMI_CLK_CORE_MAIN, PMC_TYPE_CORE, PMC_MAIN},
	{AT91_SCMI_CLK_CORE_MCK2, PMC_TYPE_CORE, PMC_MCK2},
	{AT91_SCMI_CLK_CORE_I2S0_MUX, PMC_TYPE_CORE, PMC_I2S0_MUX},
	{AT91_SCMI_CLK_CORE_I2S1_MUX, PMC_TYPE_CORE, PMC_I2S1_MUX},
	{AT91_SCMI_CLK_CORE_PLLACK, PMC_TYPE_CORE, PMC_PLLACK},
	{AT91_SCMI_CLK_CORE_AUDIOPLLCK, PMC_TYPE_CORE, PMC_AUDIOPLLCK},
	{AT91_SCMI_CLK_CORE_MCK_PRES, PMC_TYPE_CORE, PMC_MCK_PRES},
	{AT91_SCMI_CLK_SYSTEM_DDRCK, PMC_TYPE_SYSTEM, 2},
	{AT91_SCMI_CLK_SYSTEM_LCDCK, PMC_TYPE_SYSTEM, 3},
	{AT91_SCMI_CLK_SYSTEM_UHPCK, PMC_TYPE_SYSTEM, 6},
	{AT91_SCMI_CLK_SYSTEM_UDPCK, PMC_TYPE_SYSTEM, 7},
	{AT91_SCMI_CLK_SYSTEM_PCK0, PMC_TYPE_SYSTEM, 8},
	{AT91_SCMI_CLK_SYSTEM_PCK1, PMC_TYPE_SYSTEM, 9},
	{AT91_SCMI_CLK_SYSTEM_PCK2, PMC_TYPE_SYSTEM, 10},
	{AT91_SCMI_CLK_SYSTEM_ISCCK, PMC_TYPE_SYSTEM, 18},
	{AT91_SCMI_CLK_PERIPH_MACB0_CLK, PMC_TYPE_PERIPHERAL, 5},
	{AT91_SCMI_CLK_PERIPH_TDES_CLK, PMC_TYPE_PERIPHERAL, 11},
	{AT91_SCMI_CLK_PERIPH_MATRIX1_CLK, PMC_TYPE_PERIPHERAL, 14},
	{AT91_SCMI_CLK_PERIPH_HSMC_CLK, PMC_TYPE_PERIPHERAL, 17},
	{AT91_SCMI_CLK_PERIPH_PIOA_CLK, PMC_TYPE_PERIPHERAL, 18},
	{AT91_SCMI_CLK_PERIPH_FLX0_CLK, PMC_TYPE_PERIPHERAL, 19},
	{AT91_SCMI_CLK_PERIPH_FLX1_CLK, PMC_TYPE_PERIPHERAL, 20},
	{AT91_SCMI_CLK_PERIPH_FLX2_CLK, PMC_TYPE_PERIPHERAL, 21},
	{AT91_SCMI_CLK_PERIPH_FLX3_CLK, PMC_TYPE_PERIPHERAL, 22},
	{AT91_SCMI_CLK_PERIPH_FLX4_CLK, PMC_TYPE_PERIPHERAL, 23},
	{AT91_SCMI_CLK_PERIPH_UART0_CLK, PMC_TYPE_PERIPHERAL, 24},
	{AT91_SCMI_CLK_PERIPH_UART1_CLK, PMC_TYPE_PERIPHERAL, 25},
	{AT91_SCMI_CLK_PERIPH_UART2_CLK, PMC_TYPE_PERIPHERAL, 26},
	{AT91_SCMI_CLK_PERIPH_UART3_CLK, PMC_TYPE_PERIPHERAL, 27},
	{AT91_SCMI_CLK_PERIPH_UART4_CLK, PMC_TYPE_PERIPHERAL, 28},
	{AT91_SCMI_CLK_PERIPH_TWI0_CLK, PMC_TYPE_PERIPHERAL, 29},
	{AT91_SCMI_CLK_PERIPH_TWI1_CLK, PMC_TYPE_PERIPHERAL, 30},
	{AT91_SCMI_CLK_PERIPH_SPI0_CLK, PMC_TYPE_PERIPHERAL, 33},
	{AT91_SCMI_CLK_PERIPH_SPI1_CLK, PMC_TYPE_PERIPHERAL, 34},
	{AT91_SCMI_CLK_PERIPH_TCB0_CLK, PMC_TYPE_PERIPHERAL, 35},
	{AT91_SCMI_CLK_PERIPH_TCB1_CLK, PMC_TYPE_PERIPHERAL, 36},
	{AT91_SCMI_CLK_PERIPH_PWM_CLK, PMC_TYPE_PERIPHERAL, 38},
	{AT91_SCMI_CLK_PERIPH_ADC_CLK, PMC_TYPE_PERIPHERAL, 40},
	{AT91_SCMI_CLK_PERIPH_UHPHS_CLK, PMC_TYPE_PERIPHERAL, 41},
	{AT91_SCMI_CLK_PERIPH_UDPHS_CLK, PMC_TYPE_PERIPHERAL, 42},
	{AT91_SCMI_CLK_PERIPH_SSC0_CLK, PMC_TYPE_PERIPHERAL, 43},
	{AT91_SCMI_CLK_PERIPH_SSC1_CLK, PMC_TYPE_PERIPHERAL, 44},
	{AT91_SCMI_CLK_PERIPH_TRNG_CLK, PMC_TYPE_PERIPHERAL, 47},
	{AT91_SCMI_CLK_PERIPH_PDMIC_CLK, PMC_TYPE_PERIPHERAL, 48},
	{AT91_SCMI_CLK_PERIPH_SECURAM_CLK, PMC_TYPE_PERIPHERAL, 51},
	{AT91_SCMI_CLK_PERIPH_I2S0_CLK, PMC_TYPE_PERIPHERAL, 54},
	{AT91_SCMI_CLK_PERIPH_I2S1_CLK, PMC_TYPE_PERIPHERAL, 55},
	{AT91_SCMI_CLK_PERIPH_CAN0_CLK, PMC_TYPE_PERIPHERAL, 56},
	{AT91_SCMI_CLK_PERIPH_CAN1_CLK, PMC_TYPE_PERIPHERAL, 57},
	{AT91_SCMI_CLK_PERIPH_PTC_CLK, PMC_TYPE_PERIPHERAL, 58},
	{AT91_SCMI_CLK_PERIPH_CLASSD_CLK, PMC_TYPE_PERIPHERAL, 59},
	{AT91_SCMI_CLK_PERIPH_DMA0_CLK, PMC_TYPE_PERIPHERAL, 6},
	{AT91_SCMI_CLK_PERIPH_DMA1_CLK, PMC_TYPE_PERIPHERAL, 7},
	{AT91_SCMI_CLK_PERIPH_AES_CLK, PMC_TYPE_PERIPHERAL, 9},
	{AT91_SCMI_CLK_PERIPH_AESB_CLK, PMC_TYPE_PERIPHERAL, 10},
	{AT91_SCMI_CLK_PERIPH_SHA_CLK, PMC_TYPE_PERIPHERAL, 12},
	{AT91_SCMI_CLK_PERIPH_MPDDR_CLK, PMC_TYPE_PERIPHERAL, 13},
	{AT91_SCMI_CLK_PERIPH_MATRIX0_CLK, PMC_TYPE_PERIPHERAL, 15},
	{AT91_SCMI_CLK_PERIPH_SDMMC0_HCLK, PMC_TYPE_PERIPHERAL, 31},
	{AT91_SCMI_CLK_PERIPH_SDMMC1_HCLK, PMC_TYPE_PERIPHERAL, 32},
	{AT91_SCMI_CLK_PERIPH_LCDC_CLK, PMC_TYPE_PERIPHERAL, 45},
	{AT91_SCMI_CLK_PERIPH_ISC_CLK, PMC_TYPE_PERIPHERAL, 46},
	{AT91_SCMI_CLK_PERIPH_QSPI0_CLK, PMC_TYPE_PERIPHERAL, 52},
	{AT91_SCMI_CLK_PERIPH_QSPI1_CLK, PMC_TYPE_PERIPHERAL, 53},
	{AT91_SCMI_CLK_GCK_SDMMC0_GCLK, PMC_TYPE_GCK, 31},
	{AT91_SCMI_CLK_GCK_SDMMC1_GCLK, PMC_TYPE_GCK, 32},
	{AT91_SCMI_CLK_GCK_TCB0_GCLK, PMC_TYPE_GCK, 35},
	{AT91_SCMI_CLK_GCK_TCB1_GCLK, PMC_TYPE_GCK, 36},
	{AT91_SCMI_CLK_GCK_PWM_GCLK, PMC_TYPE_GCK, 38},
	{AT91_SCMI_CLK_GCK_ISC_GCLK, PMC_TYPE_GCK, 46},
	{AT91_SCMI_CLK_GCK_PDMIC_GCLK, PMC_TYPE_GCK, 48},
	{AT91_SCMI_CLK_GCK_I2S0_GCLK, PMC_TYPE_GCK, 54},
	{AT91_SCMI_CLK_GCK_I2S1_GCLK, PMC_TYPE_GCK, 55},
	{AT91_SCMI_CLK_GCK_CAN0_GCLK, PMC_TYPE_GCK, 56},
	{AT91_SCMI_CLK_GCK_CAN1_GCLK, PMC_TYPE_GCK, 57},
	{AT91_SCMI_CLK_GCK_CLASSD_GCLK, PMC_TYPE_GCK, 59},
	{AT91_SCMI_CLK_PROG_PROG0, PMC_TYPE_PROGRAMMABLE, 0},
	{AT91_SCMI_CLK_PROG_PROG1, PMC_TYPE_PROGRAMMABLE, 1},
	{AT91_SCMI_CLK_PROG_PROG2, PMC_TYPE_PROGRAMMABLE, 2},
};

static TEE_Result sam_init_scmi_clk(void)
{
	unsigned int i = 0;
	struct clk *clk = NULL;
	TEE_Result res = TEE_ERROR_GENERIC;
	const struct sama5d2_pmc_clk *pmc_clk = NULL;

	for (i = 0; i < ARRAY_SIZE(pmc_clks); i++) {
		pmc_clk = &pmc_clks[i];
		res = at91_pmc_clk_get(pmc_clk->pmc_type, pmc_clk->pmc_id, &clk);
		if (res) {
			EMSG("Failed to get PMC clock type %d, id %d",
			     pmc_clk->pmc_type, pmc_clk->pmc_id);
			return res;
		}
		res = scmi_clk_add(clk, 0, pmc_clk->scmi_id);
		if (res){
			EMSG("Failed to add PMC scmi clock id %d",
			     pmc_clk->scmi_id);
			return res;
		}
	}

	clk = at91_sckc_clk_get();
	if (!clk)
		return TEE_ERROR_GENERIC;

	res = scmi_clk_add(clk, 0, AT91_SCMI_CLK_SCKC_SLOWCK_32K);
	if (res){
		EMSG("Failed to add slow clock to scmi clocks");
		return res;
	}

	return TEE_SUCCESS;
}

/*
 * Initialize platform SCMI resources
 */
static TEE_Result sam_init_scmi_server(void)
{
	size_t i = 0;

	for (i = 0; i < ARRAY_SIZE(scmi_channel); i++) {
		const struct channel_resources *res = scmi_channel + i;
		struct scmi_msg_channel *chan = res->channel;

		/* Enforce non-secure shm mapped as device memory */
		chan->shm_addr.va = (vaddr_t)phys_to_virt(chan->shm_addr.pa,
							  MEM_AREA_IO_NSEC, 1);
		assert(chan->shm_addr.va);

		scmi_smt_init_agent_channel(chan);
	}

	sam_init_scmi_clk();

	return TEE_SUCCESS;
}

driver_init_late(sam_init_scmi_server);
