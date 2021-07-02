PLATFORM_FLAVOR ?= sama5d27_som1_ek

flavor_dts_file-sama5d2xult = at91-sama5d2_xplained.dts
flavor_dts_file-sama5d27_som1_ek = at91-sama5d27_som1_ek.dts

ifeq ($(flavor_dts_file-$(PLATFORM_FLAVOR)),)
$(error Invalid platform flavor $(PLATFORM_FLAVOR))
endif
CFG_EMBED_DTB_SOURCE_FILE ?= $(flavor_dts_file-$(PLATFORM_FLAVOR))

include core/arch/arm/cpu/cortex-a5.mk

$(call force,CFG_TEE_CORE_NB_CORE,1)
$(call force,CFG_ATMEL_UART,y)
$(call force,CFG_NO_SMP,y)
$(call force,CFG_PL310,y)
$(call force,CFG_PL310_LOCKED,y)
$(call force,CFG_AT91_MATRIX,y)
$(call force,CFG_ATMEL_TCB,y)
$(call force,CFG_SM_PLATFORM_HANDLER,y)
$(call force,CFG_DRIVERS_CLK,y)
$(call force,CFG_PSCI_ARM32,y)

# These values are forced because of matrix configuration for secure area.
# When modifying these, always update matrix settings in
# matrix_configure_slave_h64mx().
$(call force,CFG_TZDRAM_START,0x20000000)
$(call force,CFG_TZDRAM_SIZE,0x800000)

CFG_SHMEM_START  ?= 0x21000000
CFG_SHMEM_SIZE   ?= 0x400000

CFG_SCMI_SHMEM_START  ?= 0x21400000
CFG_SCMI_SHMEM_SIZE   ?= 0x1000

CFG_TEE_RAM_VA_SIZE ?= 0x100000

CFG_DRAM_SIZE    ?= 0x20000000

# Device tree related configuration
CFG_DT_ADDR ?= 0x21500000
CFG_GENERATE_DTB_OVERLAY ?= y

# SCMI related configuration
CFG_SCMI_PTA ?= y

CFG_SCMI_MSG_DRIVERS ?= y
ifeq ($(CFG_SCMI_MSG_DRIVERS),y)
$(call force,CFG_SCMI_MSG_SMT,y)
$(call force,CFG_SCMI_MSG_CLOCK,y)
$(call force,CFG_SCMI_MSG_CLOCK_GENERIC,y)
$(call force,CFG_SCMI_MSG_SMT_FASTCALL_ENTRY,y)
endif
