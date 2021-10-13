/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021, Bootlin
 */

#ifndef SAM_SFR_H
#define SAM_SFR_H

#include <sm/sm.h>
#include <util.h>

/* OHCI INT Configuration Register */
#define AT91_SFR_OHCIICR	0x10
/* UTMI Clock Trimming Register */
#define AT91_SFR_UTMICKTRIM	0x30
/* Serial number 0 Register */
#define AT91_SFR_SN0		0x4c
/* Serial number 1 Register */
#define AT91_SFR_SN1		0x50
/* L2 cache RAM used as an internal SRAM */
#define AT91_SFR_L2CC_HRAMC     0x58
/* I2SC Register */
#define AT91_SFR_I2SCLKSEL	0x90

/* Field definitions */
#define AT91_UTMICKTRIM_FREQ			GENMASK_32(1, 0)

vaddr_t sam_sfr_base(void);

enum sm_handler_ret sam_sfr_access_reg(struct thread_smc_args *args);

#endif
