include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-gpon
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/luci-app-gpon
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=3. Applications
  TITLE:=LuCI Support for GPON SFP Configuration & OMCI
  DEPENDS:=+luci-compat +libuci-lua
  PKGARCH:=all
endef

define Package/luci-app-gpon/description
  Web-based GPON configuration interface for Lantiq Falcon SFP sticks.
  Allows configuring Serial Number, PLOAM password, Vendor/Equipment ID spoofing,
  viewing optical signal power (DDM), and OMCI status.
endef

define Build/Compile
endef

define Package/luci-app-gpon/install
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/controller
	$(INSTALL_BIN) ./root/usr/lib/lua/luci/controller/gpon.lua $(1)/usr/lib/lua/luci/controller/
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/model/cbi/gpon
	$(INSTALL_DATA) ./root/usr/lib/lua/luci/model/cbi/gpon/*.lua $(1)/usr/lib/lua/luci/model/cbi/gpon/
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/view/gpon
	$(INSTALL_DATA) ./root/usr/lib/lua/luci/view/gpon/*.htm $(1)/usr/lib/lua/luci/view/gpon/
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DATA) ./root/etc/config/gpon $(1)/etc/config/gpon
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./root/etc/uci-defaults/99-luci-gpon $(1)/etc/uci-defaults/
endef

$(eval $(call BuildPackage,luci-app-gpon))
