global-incdirs-y += .
srcs-y += main.c sam_sfr.c freq.c
srcs-$(CFG_AT91_MATRIX) += matrix.c
srcs-$(CFG_PL310) += sam_pl310.c
srcs-$(CFG_SCMI_MSG_DRIVERS) += scmi_server.c

subdirs-y += pm
subdirs-y += drivers
subdirs-y += nsec-service
