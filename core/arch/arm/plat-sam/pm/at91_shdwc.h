/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021, Bootlin
 */

#ifndef AT91_SHDWC_H
#define AT91_SHDWC_H

#include <compiler.h>
#include <stdbool.h>

#define AT91_SHDW_CR	0x00		/* Shut Down Control Register */
#define AT91_SHDW_SHDW		BIT(0)			/* Shut Down command */
#define AT91_SHDW_KEY		(0xa5UL << 24)		/* KEY Password */

#define AT91_SHDW_MR	0x04		/* Shut Down Mode Register */
#define AT91_SHDW_WKUPDBC_SHIFT	24
#define AT91_SHDW_WKUPDBC_MASK	GENMASK_32(26, 24)
#define AT91_SHDW_WKUPDBC(x)	(((x) << AT91_SHDW_WKUPDBC_SHIFT) \
						& AT91_SHDW_WKUPDBC_MASK)
#define AT91_SHDW_RTCWKEN	BIT32(17)

#define AT91_SHDW_SR	0x08		/* Shut Down Status Register */
#define AT91_SHDW_WKUPIS_SHIFT	16
#define AT91_SHDW_WKUPIS_MASK	GENMASK_32(31, 16)
#define AT91_SHDW_WKUPIS(x)	((1 << (x)) << AT91_SHDW_WKUPIS_SHIFT \
						& AT91_SHDW_WKUPIS_MASK)

#define AT91_SHDW_WUIR	0x0c		/* Shutdown Wake-up Inputs Register */
#define AT91_SHDW_WKUPEN_MASK	GENMASK_32(15, 0)
#define AT91_SHDW_WKUPEN(x)	((1 << (x)) & AT91_SHDW_WKUPEN_MASK)
#define AT91_SHDW_WKUPT_SHIFT	16
#define AT91_SHDW_WKUPT_MASK	GENMASK_32(31, 16)
#define AT91_SHDW_WKUPT(x)	((1 << (x)) << AT91_SHDW_WKUPT_SHIFT \
						& AT91_SHDW_WKUPT_MASK)

#if defined(CFG_AT91_SHDWC)
bool at91_shdwc_available(void);

void __noreturn at91_shdwc_shutdown(void);
#else
static inline bool at91_shdwc_available(void)
{
	return false;
}

static inline void __noreturn at91_shdwc_shutdown(void) {}
#endif

#endif /* AT91_SHDWC_H */