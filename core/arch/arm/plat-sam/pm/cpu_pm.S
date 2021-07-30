/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021, Bootlin
 */

#include <arm.h>
#include <arm32_macros.S>
#include <asm.S>

FUNC cpu_do_idle , :
	dsb					@ WFI may enter a low-power mode
	wfi
	bx	lr
END_FUNC cpu_do_idle
