// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Microchip
 */

#include <drivers/pm/sam/atmel_pm.h>
#include <console.h>
#include <drivers/scmi-msg.h>
#include <io.h>
#include <kernel/tee_misc.h>
#include <mm/core_memprot.h>
#include <sm/optee_smc.h>
#include <sm/sm.h>
#include <smc_ids.h>

#include <sam_pl310.h>

static enum sm_handler_ret sam_sip_handler(struct thread_smc_args *args)
{
	switch (OPTEE_SMC_FUNC_NUM(args->a0)) {
#if defined(CFG_PL310)
	case SAMA5_SMC_SIP_L2X0_WRITE_CTRL:
		return sam_pl310_write_ctrl(args);
#endif
	case SAMA5_SMC_SIP_SCMI_CALL_ID:
		scmi_smt_fastcall_smc_entry(0);
		args->a0 = SAMA5_SMC_SIP_RETURN_SUCCESS;
		break;
#if defined(CFG_ATMEL_PM)
	case SAMA5_SMC_SIP_SET_SUSPEND_MODE:
		at91_pm_set_suspend_mode(args);
		break;
	case SAMA5_SMC_SIP_GET_SUSPEND_MODE:
		at91_pm_get_suspend_mode(args);
		break;
#endif
	default:
		return SM_HANDLER_PENDING_SMC;
	}

	return SM_HANDLER_SMC_HANDLED;
}

enum sm_handler_ret sm_platform_handler(struct sm_ctx *ctx)
{
	uint32_t *nsec_r0 = (uint32_t *)(&ctx->nsec.r0);
	uint16_t smc_owner = OPTEE_SMC_OWNER_NUM(*nsec_r0);

	switch (smc_owner) {
	case OPTEE_SMC_OWNER_SIP:
		return sam_sip_handler((struct thread_smc_args *)nsec_r0);
	default:
		return SM_HANDLER_PENDING_SMC;
	}
}

