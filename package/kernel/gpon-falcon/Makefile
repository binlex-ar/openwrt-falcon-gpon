# SPDX-License-Identifier: GPL-2.0-only
#
# OpenWrt package Makefile for Lantiq Falcon Open-Source GPON Driver
# Target: package/kernel/gpon-falcon/Makefile
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=gpon-falcon
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0+

include $(INCLUDE_DIR)/package.mk

define KernelPackage/gpon-falcon
  SUBMENU:=Network Devices
  TITLE:=Lantiq Falcon GPON SoC Open Driver
  DEPENDS:=@TARGET_lantiq_falcon
  FILES:=$(PKG_BUILD_DIR)/gpon_falcon.ko
  AUTOLOAD:=$(call AutoLoad,50,gpon_falcon)
  KCONFIG:=
endef

define KernelPackage/gpon-falcon/description
  Clean-room open-source kernel driver for Lantiq Falcon (PEF98036) GPON SoC,
  supporting hardware GTC, GPE microcode engine, BOSA optics calibration,
  and ITU-T G.984.3 PLOAM state machine.
endef

define Build/Compile
	$(KERNEL_MAKE) M="$(PKG_BUILD_DIR)" modules
endef

define KernelPackage/gpon-falcon/install
	$(INSTALL_DIR) $(1)/lib/firmware/sfu
	$(CP) ./files/lib/firmware/sfu/* $(1)/lib/firmware/sfu/
endef

$(eval $(call KernelPackage,gpon-falcon))
