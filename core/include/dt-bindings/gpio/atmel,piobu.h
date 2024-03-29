/* SPDX-License-Identifier: (GPL-2.0 OR BSD-2-Clause) */
/*
 * Copyright (C) 2021 Microchip
 */

#ifndef _DT_BINDINGS_ATMEL_PIOBU_H
#define _DT_BINDINGS_ATMEL_PIOBU_H

#define PIOBU_PIN_AFV_SHIFT		0
#define PIOBU_PIN_AFV_MASK		0xF
#define PIOBU_PIN_AFV(val)		(((val) & PIOBU_PIN_AFV_MASK) >> PIOBU_PIN_AFV_SHIFT)

#define PIOBU_PIN_RFV_SHIFT		4
#define PIOBU_PIN_RFV_MASK		0xF0
#define PIOBU_PIN_RFV(val)		(((val) & PIOBU_PIN_RFV_MASK) >> PIOBU_PIN_RFV_SHIFT)

#define PIOBU_PIN_PULL_MODE_SHIFT	8
#define PIOBU_PIN_PULL_MODE_MASK	(0x3 << PIOBU_PIN_PULL_MODE_SHIFT)
#define PIOBU_PIN_PULL_MODE(val)	(((val) & PIOBU_PIN_PULL_MODE_MASK) >> PIOBU_PIN_PULL_MODE_SHIFT)
#define PIOBU_PIN_PULL_NONE		0
#define PIOBU_PIN_PULL_UP		1
#define PIOBU_PIN_PULL_DOWN		2

#define PIOBU_PIN_DEF_LEVEL_SHIFT	10
#define PIOBU_PIN_DEF_LEVEL_MASK	(1 << PIOBU_PIN_DEF_LEVEL_SHIFT)
#define PIOBU_PIN_DEF_LEVEL(val)	(((val) & PIOBU_PIN_DEF_LEVEL_MASK) >> PIOBU_PIN_DEF_LEVEL_SHIFT)
#define PIOBU_PIN_DEF_LEVEL_LOW		0
#define PIOBU_PIN_DEF_LEVEL_HIGH	1

#define PIOBU_PIN_WAKEUP_SHIFT		11
#define PIOBU_PIN_WAKEUP_MASK		(1 << PIOBU_PIN_WAKEUP_SHIFT)
#define PIOBU_PIN_WAKEUP(val)		(((val) & PIOBU_PIN_WAKEUP_MASK) >> PIOBU_PIN_WAKEUP_SHIFT)
#define PIOBU_PIN_WAKEUP_DISABLE	0
#define PIOBU_PIN_WAKEUP_ENABLE		1

#define PIOBU_PIN_INPUT(afv, rfv, pull_mode, def_level, wakeup) \
	(afv | \
	 (rfv << PIOBU_PIN_RFV_SHIFT) & PIOBU_PIN_RFV_MASK | \
	 (pull_mode << PIOBU_PIN_PULL_MODE_SHIFT) & PIOBU_PIN_PULL_MODE_MASK | \
	 (def_level << PIOBU_PIN_DEF_LEVEL_SHIFT) & PIOBU_PIN_DEF_LEVEL_MASK)

#endif /* _DT_BINDINGS_ATMEL_PIOBU_H */
