# SPDX-License-Identifier: GPL-2.0+
#
# Makefile for Open-Source Lantiq Falcon GPON Kernel Driver
#

obj-m += gpon_falcon.o

gpon_falcon-objs := \
	falcon_gpon_core.o \
	falcon_ploam.o \
	falcon_dma.o \
	falcon_bosa.o

ccflags-y += -I$(src) -Wall

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
