/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021, Bootlin
 */

#ifndef AT91_RSTC_H
#define AT91_RSTC_H

#include <compiler.h>
#include <stdbool.h>

#if defined(CFG_AT91_RSTC)
bool at91_rstc_available(void);

void __noreturn at91_rstc_reset(void);
#else
static inline bool at91_rstc_available(void)
{
	return false;
}

static inline void __noreturn at91_rstc_reset(void) {}
#endif

#endif /* AT91_RSTC_H */