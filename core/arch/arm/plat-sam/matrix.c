// SPDX-License-Identifier: BSD-Source-Code
/*
 * Copyright (c) 2013, Atmel Corporation
 * Copyright (c) 2017, Timesys Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <arm32.h>
#include <initcall.h>
#include <io.h>
#include <kernel/interrupt.h>
#include <kernel/panic.h>
#include <kernel/pm.h>
#include <matrix.h>
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>
#include <platform_config.h>
#include <sama5d2.h>
#include <stdint.h>
#include <trace.h>
#include <tz_matrix.h>

#define MATRIX_H64MX	0
#define MATRIX_H32MX	1

#define SECURITY_TYPE_AS	1
#define SECURITY_TYPE_NS	2
#define SECURITY_TYPE_PS	3

#define WORLD_NON_SECURE	0
#define WORLD_SECURE		1

#define MATRIX_SPSELR_COUNT	3
#define MATRIX_SLAVE_COUNT	15

register_phys_mem_pgdir(MEM_AREA_IO_SEC, AT91C_BASE_MATRIX32,
			CORE_MMU_PGDIR_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_SEC, AT91C_BASE_MATRIX64,
			CORE_MMU_PGDIR_SIZE);

static void matrix_register_pm(void);

struct peri_security {
	unsigned int peri_id;
	unsigned int matrix;
	unsigned int security_type;
};

static const struct peri_security peri_security_array[] = {
	{
		.peri_id = AT91C_ID_PMC,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_ARM,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_PIT,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_WDT,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_GMAC,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_XDMAC0,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_XDMAC1,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_ICM,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_AES,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_AESB,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_TDES,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SHA,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_MPDDRC,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_MATRIX1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_AS,
	},
	{
		.peri_id = AT91C_ID_MATRIX0,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_AS,
	},
	{
		.peri_id = AT91C_ID_SECUMOD,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_AS,
	},
	{
		.peri_id = AT91C_ID_HSMC,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_PIOA,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_AS,
	},
	{
		.peri_id = AT91C_ID_FLEXCOM0,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_FLEXCOM1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_FLEXCOM2,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_FLEXCOM3,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_FLEXCOM4,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_UART0,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_UART1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_UART2,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_UART3,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_UART4,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_TWI0,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_TWI1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SDMMC0,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SDMMC1,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SPI0,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SPI1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_TC0,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_TC1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_PWM,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_ADC,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_UHPHS,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_UDPHS,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SSC0,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SSC1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_LCDC,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_ISI,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_TRNG,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_PDMIC,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_IRQ,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_NS,
	},
	{
		.peri_id = AT91C_ID_SFC,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SECURAM,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_AS,
	},
	{
		.peri_id = AT91C_ID_QSPI0,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_QSPI1,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_I2SC0,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_I2SC1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_CAN0_INT0,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_CAN1_INT0,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_CLASSD,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SFR,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SAIC,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_AS,
	},
	{
		.peri_id = AT91C_ID_AIC,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_NS,
	},
	{
		.peri_id = AT91C_ID_L2CC,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_CAN0_INT1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_CAN1_INT1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_GMAC_Q1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_GMAC_Q2,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_PIOB,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_AS,
	},
	{
		.peri_id = AT91C_ID_PIOC,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_AS,
	},
	{
		.peri_id = AT91C_ID_PIOD,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_AS,
	},
	{
		.peri_id = AT91C_ID_SDMMC0_TIMER,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SDMMC1_TIMER,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SYS,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_ACC,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_RXLP,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SFRBU,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_CHIPID,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
};

static vaddr_t matrix32_base(void)
{
	static void *va;

	if (cpu_mmu_enabled()) {
		if (!va)
			va = phys_to_virt(AT91C_BASE_MATRIX32, MEM_AREA_IO_SEC,
					  1);
		return (vaddr_t)va;
	}
	return AT91C_BASE_MATRIX32;
}

static vaddr_t matrix64_base(void)
{
	static void *va;

	if (cpu_mmu_enabled()) {
		if (!va)
			va = phys_to_virt(AT91C_BASE_MATRIX64, MEM_AREA_IO_SEC,
					  1);
		return (vaddr_t)va;
	}
	return AT91C_BASE_MATRIX64;
}

static void matrix_write(unsigned int base,
			 unsigned int offset,
			 const unsigned int value)
{
	io_write32(offset + base, value);
}

static unsigned int matrix_read(int base, unsigned int offset)
{
	return io_read32(offset + base);
}

static void matrix_write_protect_disable(unsigned int matrix_base)
{
	matrix_write(matrix_base, MATRIX_WPMR, MATRIX_WPMR_WPKEY_PASSWD);
}

static void matrix_configure_slave_security(unsigned int matrix_base,
				     unsigned int slave,
				     unsigned int srtop_setting,
				     unsigned int srsplit_setting,
				     unsigned int ssr_setting)
{
	matrix_write(matrix_base, MATRIX_SSR(slave), ssr_setting);
	matrix_write(matrix_base, MATRIX_SRTSR(slave), srtop_setting);
	matrix_write(matrix_base, MATRIX_SASSR(slave), srsplit_setting);
}

static const struct peri_security *get_peri_security(unsigned int peri_id)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(peri_security_array); i++) {
		if (peri_id == peri_security_array[i].peri_id)
			return &peri_security_array[i];
	}

	return NULL;
}

static int matrix_set_periph_world(unsigned int matrix, unsigned int peri_id,
				   unsigned int world)
{
	unsigned int base;
	unsigned int spselr;
	unsigned int idx;
	unsigned int bit;

	idx = peri_id / 32;
	if (idx > 3)
		return -1;

	bit = (0x01 << (peri_id % 32));

	if (matrix == MATRIX_H32MX)
		base = matrix32_base();
	else if (matrix == MATRIX_H64MX)
		base = matrix64_base();
	else
		return -1;

	spselr = matrix_read(base, MATRIX_SPSELR(idx));
	if (world == WORLD_SECURE)
		spselr &= ~bit;
	else
		spselr |= bit;
	matrix_write(base, MATRIX_SPSELR(idx), spselr);

	return 0;
}

int matrix_configure_periph_secure(unsigned int peri_id)
{
	const struct peri_security *psec = NULL;

	psec = get_peri_security(peri_id);
	if (!psec)
		return -1;

	return matrix_set_periph_world(psec->matrix, peri_id, WORLD_SECURE);
}

static int matrix_configure_periph_non_secure(unsigned int *peri_id_array,
					      unsigned int size)
{
	unsigned int i;
	unsigned int *peri_id_p;
	unsigned int matrix;
	unsigned int peri_id;
	const struct peri_security *peripheral_sec;
	int ret;

	if (!peri_id_array || !size)
		return -1;

	peri_id_p = peri_id_array;
	for (i = 0; i < size; i++) {
		peripheral_sec = get_peri_security(*peri_id_p);
		if (!peripheral_sec)
			return -1;

		if (peripheral_sec->security_type != SECURITY_TYPE_PS)
			return -1;

		matrix = peripheral_sec->matrix;
		peri_id = *peri_id_p;
		ret = matrix_set_periph_world(matrix, peri_id,
					      WORLD_NON_SECURE);
		if (ret)
			return -1;

		peri_id_p++;
	}

	return 0;
}

static void matrix_configure_slave_h64mx(void)
{
	unsigned int ddr_port;
	unsigned int ssr_setting;
	unsigned int sasplit_setting;
	unsigned int srtop_setting;

	/*
	 * 0: Bridge from H64MX to AXIMX
	 * (Internal ROM, Crypto Library, PKCC RAM): Always Secured
	 */

	/* 1: H64MX Peripheral Bridge: SDMMC0, SDMMC1 Non-Secure */
	srtop_setting =	MATRIX_SRTOP(1, MATRIX_SRTOP_VALUE_128M)
			| MATRIX_SRTOP(2, MATRIX_SRTOP_VALUE_128M);
	sasplit_setting = MATRIX_SASPLIT(1, MATRIX_SASPLIT_VALUE_128M)
			| MATRIX_SASPLIT(2, MATRIX_SASPLIT_VALUE_128M);
	ssr_setting = (MATRIX_LANSECH_NS(1)
			| MATRIX_LANSECH_NS(2)
			| MATRIX_RDNSECH_NS(1)
			| MATRIX_RDNSECH_NS(2)
			| MATRIX_WRNSECH_NS(1)
			| MATRIX_WRNSECH_NS(2));
	matrix_configure_slave_security(matrix64_base(),
					H64MX_SLAVE_PERI_BRIDGE,
					srtop_setting,
					sasplit_setting,
					ssr_setting);

	/*
	 * Matrix DDR configuration is hardcoded here and is difficult to
	 * generate at runtime. Since this configuration expect the secure
	 * DRAM to be at start of RAM and 8M of size, enforce it here.
	 */
	COMPILE_TIME_ASSERT(CFG_TZDRAM_START == AT91C_BASE_DDRCS);
	COMPILE_TIME_ASSERT(CFG_TZDRAM_SIZE == 0x800000);

	/* 2 ~ 9 DDR2 Port1 ~ 7: Non-Secure, except op-tee tee/ta memory */
	srtop_setting = MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_128M);
	sasplit_setting = (MATRIX_SASPLIT(0, MATRIX_SASPLIT_VALUE_16M)
				| MATRIX_SASPLIT(1, MATRIX_SASPLIT_VALUE_128M)
				| MATRIX_SASPLIT(2, MATRIX_SASPLIT_VALUE_128M)
				| MATRIX_SASPLIT(3, MATRIX_SASPLIT_VALUE_128M));
	ssr_setting = (MATRIX_LANSECH_S(0)
			| MATRIX_LANSECH_NS(1)
			| MATRIX_LANSECH_NS(2)
			| MATRIX_LANSECH_NS(3)
			| MATRIX_RDNSECH_S(0)
			| MATRIX_RDNSECH_NS(1)
			| MATRIX_RDNSECH_NS(2)
			| MATRIX_RDNSECH_NS(3)
			| MATRIX_WRNSECH_S(0)
			| MATRIX_WRNSECH_NS(1)
			| MATRIX_WRNSECH_NS(2)
			| MATRIX_WRNSECH_NS(3));
	/* DDR port 0 not used from NWd */
	for (ddr_port = 1; ddr_port < 8; ddr_port++) {
		matrix_configure_slave_security(matrix64_base(),
					(H64MX_SLAVE_DDR2_PORT_0 + ddr_port),
					srtop_setting,
					sasplit_setting,
					ssr_setting);
	}

	/* 10: Internal SRAM 128K:
	 * - First 64K are reserved for suspend code in Secure World
	 * - Last 64K are for Non-Secure world (used by CAN)
	 */
	srtop_setting = MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_128K);
	sasplit_setting = MATRIX_SASPLIT(0, MATRIX_SRTOP_VALUE_64K);
	ssr_setting = (MATRIX_LANSECH_S(0)
			| MATRIX_RDNSECH_S(0)
			| MATRIX_WRNSECH_S(0));
	matrix_configure_slave_security(matrix64_base(),
					H64MX_SLAVE_INTERNAL_SRAM,
					srtop_setting,
					sasplit_setting,
					ssr_setting);

	/* 11:  Internal SRAM 128K (Cache L2): Default */

	/* 12:  QSPI0: Default */
	/* 13:  QSPI1: Default */
	srtop_setting = MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_128M);
	sasplit_setting = MATRIX_SASPLIT(0, MATRIX_SASPLIT_VALUE_128M);
	ssr_setting = (MATRIX_LANSECH_NS(0)
			| MATRIX_RDNSECH_NS(0)
			| MATRIX_WRNSECH_NS(0));

	matrix_configure_slave_security(matrix64_base(),
				H64MX_SLAVE_QSPI0,
				srtop_setting,
				sasplit_setting,
				ssr_setting);
	matrix_configure_slave_security(matrix64_base(),
				H64MX_SLAVE_QSPI1,
				srtop_setting,
				sasplit_setting,
				ssr_setting);
	/* 14:  AESB: Default */
}

static void matrix_configure_slave_h32mx(void)
{
	unsigned int ssr_setting;
	unsigned int sasplit_setting;
	unsigned int srtop_setting;

	/* 0: Bridge from H32MX to H64MX: Not Secured */
	/* 1: H32MX Peripheral Bridge 0: Not Secured */
	/* 2: H32MX Peripheral Bridge 1: Not Secured */

	/*
	 * 3: External Bus Interface
	 * EBI CS0 Memory(256M) ----> Slave Region 0, 1
	 * EBI CS1 Memory(256M) ----> Slave Region 2, 3
	 * EBI CS2 Memory(256M) ----> Slave Region 4, 5
	 * EBI CS3 Memory(128M) ----> Slave Region 6
	 * NFC Command Registers(128M) -->Slave Region 7
	 * NANDFlash(EBI CS3) --> Slave Region 6: Non-Secure
	 */
	srtop_setting =	MATRIX_SRTOP(6, MATRIX_SRTOP_VALUE_128M);
	srtop_setting |= MATRIX_SRTOP(7, MATRIX_SRTOP_VALUE_128M);
	sasplit_setting = MATRIX_SASPLIT(6, MATRIX_SASPLIT_VALUE_128M);
	sasplit_setting |= MATRIX_SASPLIT(7, MATRIX_SASPLIT_VALUE_128M);
	ssr_setting = (MATRIX_LANSECH_NS(6)
			| MATRIX_RDNSECH_NS(6)
			| MATRIX_WRNSECH_NS(6));
	ssr_setting |= (MATRIX_LANSECH_NS(7)
			| MATRIX_RDNSECH_NS(7)
			| MATRIX_WRNSECH_NS(7));
	matrix_configure_slave_security(matrix32_base(),
					H32MX_EXTERNAL_EBI,
					srtop_setting,
					sasplit_setting,
					ssr_setting);

	/* 4: NFC SRAM (4K): Non-Secure */
	srtop_setting = MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_8K);
	sasplit_setting = MATRIX_SASPLIT(0, MATRIX_SASPLIT_VALUE_8K);
	ssr_setting = (MATRIX_LANSECH_NS(0)
			| MATRIX_RDNSECH_NS(0)
			| MATRIX_WRNSECH_NS(0));
	matrix_configure_slave_security(matrix32_base(),
					H32MX_NFC_SRAM,
					srtop_setting,
					sasplit_setting,
					ssr_setting);

	/* 5:
	 * USB Device High Speed Dual Port RAM (DPR): 1M
	 * USB Host OHCI registers: 1M
	 * USB Host EHCI registers: 1M
	 */
	srtop_setting = (MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_1M)
			| MATRIX_SRTOP(1, MATRIX_SRTOP_VALUE_1M)
			| MATRIX_SRTOP(2, MATRIX_SRTOP_VALUE_1M));
	sasplit_setting = (MATRIX_SASPLIT(0, MATRIX_SASPLIT_VALUE_1M)
			| MATRIX_SASPLIT(1, MATRIX_SASPLIT_VALUE_1M)
			| MATRIX_SASPLIT(2, MATRIX_SASPLIT_VALUE_1M));
	ssr_setting = (MATRIX_LANSECH_NS(0)
			| MATRIX_LANSECH_NS(1)
			| MATRIX_LANSECH_NS(2)
			| MATRIX_RDNSECH_NS(0)
			| MATRIX_RDNSECH_NS(1)
			| MATRIX_RDNSECH_NS(2)
			| MATRIX_WRNSECH_NS(0)
			| MATRIX_WRNSECH_NS(1)
			| MATRIX_WRNSECH_NS(2));
	matrix_configure_slave_security(matrix32_base(),
					H32MX_USB,
					srtop_setting,
					sasplit_setting,
					ssr_setting);
}

static unsigned int security_ps_peri_id[] = {
	AT91C_ID_PMC,
	AT91C_ID_ARM,
	AT91C_ID_PIT,
	AT91C_ID_WDT,
	AT91C_ID_GMAC,
	AT91C_ID_XDMAC0,
	AT91C_ID_XDMAC1,
	AT91C_ID_ICM,
	AT91C_ID_AES,
	AT91C_ID_AESB,
	AT91C_ID_TDES,
	AT91C_ID_SHA,
	AT91C_ID_MPDDRC,
	AT91C_ID_HSMC,
	AT91C_ID_FLEXCOM0,
	AT91C_ID_FLEXCOM1,
	AT91C_ID_FLEXCOM2,
	AT91C_ID_FLEXCOM3,
	AT91C_ID_FLEXCOM4,
	AT91C_ID_UART0,
	AT91C_ID_UART1,
	AT91C_ID_UART2,
	AT91C_ID_UART3,
	AT91C_ID_UART4,
	AT91C_ID_TWI0,
	AT91C_ID_TWI1,
	AT91C_ID_SDMMC0,
	AT91C_ID_SDMMC1,
	AT91C_ID_SPI0,
	AT91C_ID_SPI1,
	AT91C_ID_TC0,
	AT91C_ID_TC1,
	AT91C_ID_PWM,
	AT91C_ID_ADC,
	AT91C_ID_UHPHS,
	AT91C_ID_UDPHS,
	AT91C_ID_SSC0,
	AT91C_ID_SSC1,
	AT91C_ID_LCDC,
	AT91C_ID_ISI,
	AT91C_ID_TRNG,
	AT91C_ID_PDMIC,
	AT91C_ID_SFC,
	AT91C_ID_QSPI0,
	AT91C_ID_QSPI1,
	AT91C_ID_I2SC0,
	AT91C_ID_I2SC1,
	AT91C_ID_CAN0_INT0,
	AT91C_ID_CAN1_INT0,
	AT91C_ID_CLASSD,
	AT91C_ID_SFR,
	AT91C_ID_L2CC,
	AT91C_ID_CAN0_INT1,
	AT91C_ID_CAN1_INT1,
	AT91C_ID_GMAC_Q1,
	AT91C_ID_GMAC_Q2,
	AT91C_ID_SDMMC0_TIMER,
	AT91C_ID_SDMMC1_TIMER,
	AT91C_ID_SYS,
	AT91C_ID_ACC,
	AT91C_ID_RXLP,
	AT91C_ID_SFRBU,
	AT91C_ID_CHIPID,
};

void matrix_init(void)
{
	int ret = -1;

	matrix_write_protect_disable(matrix64_base());
	matrix_write_protect_disable(matrix32_base());

	matrix_configure_slave_h64mx();
	matrix_configure_slave_h32mx();

	ret = matrix_configure_periph_non_secure(security_ps_peri_id,
					         ARRAY_SIZE(security_ps_peri_id));
	if (ret)
		panic("Failed to configure matrix");

}

static TEE_Result matrix_pm_init(void)
{
	/*
	 * We can't call matrix_register_pm in matrix_init since allocator is
	 * not ready yet so we just call it later in this driver init callback.
	 */
	matrix_register_pm();

	return TEE_SUCCESS;
}
driver_init(matrix_pm_init);

#ifdef CFG_PM_ARM32
struct matrix_state {
	uint32_t spselr[MATRIX_SPSELR_COUNT];
	uint32_t ssr[MATRIX_SLAVE_COUNT];
	uint32_t srtsr[MATRIX_SLAVE_COUNT];
	uint32_t sassr[MATRIX_SLAVE_COUNT];
};

static struct matrix_state matrix32_state;
static struct matrix_state matrix64_state;

static void matrix_save_regs(vaddr_t base, struct matrix_state *state)
{
	int idx = 0;

	for (idx = 0; idx < MATRIX_SPSELR_COUNT; idx++)
		state->spselr[idx] = matrix_read(base, MATRIX_SPSELR(idx));

	for (idx = 0; idx < MATRIX_SLAVE_COUNT; idx++) {
		state->ssr[idx] = matrix_read(base, MATRIX_SSR(idx));
		state->srtsr[idx] = matrix_read(base, MATRIX_SRTSR(idx));
		state->sassr[idx] = matrix_read(base, MATRIX_SASSR(idx));
	}
}

static void matrix_suspend(void)
{
	matrix_save_regs(matrix32_base(), &matrix32_state);
	matrix_save_regs(matrix64_base(), &matrix64_state);
}

static void matrix_restore_regs(vaddr_t base, struct matrix_state *state)
{
	int idx = 0;

	matrix_write_protect_disable(base);

	for (idx = 0; idx < MATRIX_SPSELR_COUNT; idx++)
		matrix_write(base, MATRIX_SPSELR(idx), state->spselr[idx]);

	for (idx = 0; idx < MATRIX_SLAVE_COUNT; idx++) {
		matrix_write(base, MATRIX_SSR(idx), state->ssr[idx]);
		matrix_write(base, MATRIX_SRTSR(idx), state->srtsr[idx]);
		matrix_write(base, MATRIX_SASSR(idx), state->sassr[idx]);
	}
}

static void matrix_resume(void)
{
	matrix_restore_regs(matrix32_base(), &matrix32_state);
	matrix_restore_regs(matrix64_base(), &matrix64_state);
}

static TEE_Result matrix_pm(enum pm_op op, uint32_t pm_hint __unused,
			   const struct pm_callback_handle *hdl __unused)
{
	switch(op) {
	case PM_OP_RESUME:
		matrix_resume();
		break;
	case PM_OP_SUSPEND:
		matrix_suspend();
		break;
	default:
		panic("Invalid PM operation");
	}

	return TEE_SUCCESS;
}

static void matrix_register_pm(void)
{
	register_pm_driver_cb(matrix_pm, NULL);
}

#else
static void matrix_register_pm(void)
{
}
#endif
