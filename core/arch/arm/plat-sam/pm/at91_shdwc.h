// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Bootlin
 */

#ifndef AT91_SHDWC_H
#define AT91_SHDWC_H

#include <compiler.h>
#include <stdbool.h>

bool at91_shdwc_available(void);

void __noreturn at91_shdwc_shutdown(void);

#endif /* AT91_SHDWC_H */