// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Bootlin
 */

#ifndef DRIVER_H
#define DRIVER_H

#include <tee_api_types.h>

struct generic_driver {
	TEE_Result (*setup)(const void *fdt, int nodeoffset, int status);
};

#endif