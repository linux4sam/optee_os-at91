/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2019, STMicroelectronics
 * Copyright (c) 2021, Bootlin
 */
#ifndef SAM_PL310_H
#define SAM_PL310_H

#include <sm/sm.h>

enum sm_handler_ret sam_pl310_write_reg(struct thread_smc_args *args);

#endif /* #ifndef SAM_PL310_H */
