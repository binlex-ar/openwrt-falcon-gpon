# SPDX-License-Identifier: GPL-2.0+

obj-m += gpon_falcon.o

gpon_falcon-y := \
	falcon_gpon_core.o \
	falcon_ploam.o \
	falcon_dma.o \
	falcon_bosa.o \
	falcon_onu_dev.o

ccflags-y += -I$(src) -Wno-error
