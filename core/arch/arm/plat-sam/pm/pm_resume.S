#include <arm.h>
#include <arm32_macros.S>
#include <asm.S>

FUNC at91_pm_cpu_resume, :
UNWIND(	.cantunwind)

	cps	#CPSR_MODE_MON
	/*
	 * No stack, scratch r0-r3
	 * TODO: Need to use specific configure, but not plat_xxx.
	 * Because plat_xx maybe changed in future, we can not rely on it.
	 * Need handle sp carefully.
	 */
	blx	plat_cpu_reset_early

	b	sm_pm_cpu_resume
END_FUNC at91_pm_cpu_resume
