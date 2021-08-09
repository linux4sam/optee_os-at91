// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2017 Timesys Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <arm32.h>
#include <console.h>
#include <drivers/atmel_uart.h>
#include <drivers/atmel_saic.h>
#include <kernel/boot.h>
#include <kernel/interrupt.h>
#include <kernel/panic.h>
#include <matrix.h>
#include <stdint.h>

static struct atmel_uart_data console_data;

register_phys_mem_pgdir(MEM_AREA_IO_SEC, CONSOLE_UART_BASE,
			CORE_MMU_PGDIR_SIZE);

void console_init(void)
{
#ifdef CONSOLE_UART_BASE
	atmel_uart_init(&console_data, CONSOLE_UART_BASE);
	register_serial_console(&console_data.chip);
#endif
}

void plat_primary_init_early(void)
{
	matrix_init();
}

void itr_core_handler(void)
{
	atmel_saic_it_handle();
}

void main_init_gic(void)
{
	TEE_Result res = TEE_ERROR_GENERIC;

	res = atmel_saic_setup();
	if (res)
		panic("Failed to init interrupts\n");

	matrix_interrupt_init();
}
