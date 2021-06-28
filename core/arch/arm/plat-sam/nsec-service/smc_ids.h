/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021, Bootlin
 */

#ifndef SMC_IDS_H
#define SMC_IDS_H
#include <optee_msg.h>
#include <sm/optee_smc.h>

#define SAMA5_SMC_SIP_L2X0_WRITE_REG	0x100

#define SAMA5_SMC_SIP_SCMI_CALL_ID	0x200

#define SAMA5_SMC_SIP_SFR_REG_CALL_ID	0x300

/* SAMA5 SMC return codes */
#define SAMA5_SMC_SIP_RETURN_SUCCESS	0x0
#define SAMA5_SMC_SIP_RETURN_EPERM	0x1

#endif /* SMC_IDS_H */
