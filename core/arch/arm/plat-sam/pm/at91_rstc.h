// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Bootlin
 */

#ifndef AT91_RSTC_H
#define AT91_RSTC_H

#include <compiler.h>
#include <stdbool.h>

bool at91_rstc_available(void);

void __noreturn at91_rstc_reset(void);

#endif /* AT91_RSTC_H */