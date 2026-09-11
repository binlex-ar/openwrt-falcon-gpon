include $(TOPDIR)/rules.mk

PKG_NAME:=omcid
PKG_VERSION:=4.5.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/omcid
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=Routing and Redirection
  TITLE:=Lantiq GPON OMCI Daemon
  DEPENDS:=+libuci +libpthread +librt
endef

define Package/omcid/description
  OMCI daemon for Lantiq Falcon GPON SoC implementing ITU-T G.984 and G.988 protocols.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	# 1. Build lib_ifxos
	( cd $(PKG_BUILD_DIR)/lib_ifxos && \
	  ./configure \
	    --host=$(GNU_TARGET_NAME) \
	    --build=$(GNU_HOST_NAME) \
	    --prefix=$(PKG_BUILD_DIR)/install_ifxos \
	    --without-kernel-module \
	    --enable-linux-26 \
	    CC="$(TARGET_CC)" \
	    CFLAGS="$(TARGET_CFLAGS) -fcommon -Wno-error" \
	    LDFLAGS="$(TARGET_LDFLAGS)" && \
	  $(MAKE) && \
	  $(MAKE) install \
	)

	# 2. Build gpon_omci_api
	( cd $(PKG_BUILD_DIR)/gpon_omci_api-4.5.0 && \
	  ./configure \
	    --host=$(GNU_TARGET_NAME) \
	    --build=$(GNU_HOST_NAME) \
	    --prefix=$(PKG_BUILD_DIR)/install_omci_api \
	    --enable-ifxos-include="-I$(PKG_BUILD_DIR)/lib_ifxos/src/include" \
	    --enable-driver-include="-I$(PKG_BUILD_DIR)/include/onu -I$(PKG_BUILD_DIR)/include/optic" \
	    --enable-device=PSB98030 \
	    --enable-falcon-sw-image-support \
	    --enable-mcc \
	    --disable-voip \
	    --disable-remote-onu \
	    --enable-debug-prints \
	    CC="$(TARGET_CC)" \
	    CFLAGS="$(TARGET_CFLAGS) -fcommon -Wno-error" \
	    LDFLAGS="$(TARGET_LDFLAGS) -L$(PKG_BUILD_DIR)/install_ifxos/lib" && \
	  $(MAKE) \
	)

	# 3. Build gpon_omci_onu (omcid binary)
	( cd $(PKG_BUILD_DIR)/gpon_omci_onu-4.5.0 && \
	  CPPFLAGS="-I$(PKG_BUILD_DIR)/include/onu" \
	  ./configure \
	    --host=$(GNU_TARGET_NAME) \
	    --build=$(GNU_HOST_NAME) \
	    --prefix=$(PKG_BUILD_DIR)/install_omcid \
	    --enable-ifxos-include="-I$(PKG_BUILD_DIR)/lib_ifxos/src/include -I$(PKG_BUILD_DIR)/lib_ifxos/src/include/ifxos" \
	    --enable-ifxos-library="-L$(PKG_BUILD_DIR)/install_ifxos/lib" \
	    --enable-omci-api-include="-I$(PKG_BUILD_DIR)/gpon_omci_api-4.5.0/include -I$(PKG_BUILD_DIR)/gpon_omci_api-4.5.0/include/me -I$(PKG_BUILD_DIR)/gpon_omci_api-4.5.0/include/mcc" \
	    --enable-omci-api-library="-L$(PKG_BUILD_DIR)/gpon_omci_api-4.5.0/src" \
	    --disable-cli \
	    --disable-voip \
	    --enable-pm \
	    --disable-remote-onu \
	    --enable-omci-uci \
	    --disable-libucimap \
	    --enable-formatted-omci-dump \
	    --enable-debug-prints \
	    CC="$(TARGET_CC)" \
	    CFLAGS="$(TARGET_CFLAGS) -fcommon -Wno-error" \
	    LDFLAGS="$(TARGET_LDFLAGS) -L$(PKG_BUILD_DIR)/install_ifxos/lib -L$(PKG_BUILD_DIR)/gpon_omci_api-4.5.0/src" && \
	  $(MAKE) \
	)
endef

define Package/omcid/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/gpon_omci_onu-4.5.0/src/omcid $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/omcid.init $(1)/etc/init.d/omcid
	$(INSTALL_DIR) $(1)/etc/mibs
	$(INSTALL_DATA) ./files/mibs/*.ini $(1)/etc/mibs/
endef

$(eval $(call BuildPackage,omcid))
