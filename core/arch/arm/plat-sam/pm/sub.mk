srcs-$(CFG_PSCI_ARM32) += psci.c

srcs-$(CFG_AT91_RSTC) += at91_rstc.c
srcs-$(CFG_AT91_SHDWC) += at91_shdwc.c

srcs-$(CFG_AT91_SECURAM) += at91_securam.c
srcs-$(CFG_AT91_PM) += at91_pm.c cpu_pm.S