-- SPDX-License-Identifier: Apache-2.0
-- GPON Configuration CBI Form

local m, s, o

m = Map("gpon", translate("GPON SFP Configuration"),
	translate("Configure GPON Serial Number, PLOAM credentials, and OMCI parameters for Lantiq Falcon SFP stick."))

s = m:section(NamedSection, "config", "config", translate("General Settings"))
s.anonymous = true
s:tab("identity", translate("Identity & Registration"))
s:tab("omci", translate("OMCI & MIB"))
s:tab("hardware", translate("Optics & Driver"))

-- Identity Tab
o = s:taboption("identity", Value, "gpon_sn", translate("GPON Serial Number"),
	translate("12-character GPON Serial Number (e.g. HWTC12345678, ALCL12345678, or 16-character HEX)."))
o.rmempty = false

o = s:taboption("identity", Value, "ploam_pwd", translate("PLOAM Password / Registration ID"),
	translate("Authentication password required by some ISPs (e.g. Rostelecom). Leave empty if not required."))
o.password = true
o.rmempty = true

o = s:taboption("identity", Value, "vendor_id", translate("Vendor ID"),
	translate("4-character Vendor ID spoofed in OMCI ME 256 (ONT-G). e.g. HWTC, ALCL, ZTEG."))
o.default = "ALCL"

o = s:taboption("identity", Value, "equipment_id", translate("Equipment ID"),
	translate("ONT Equipment ID spoofed in OMCI ME 257 (ONT2-G). e.g. BVL3A5HNAAG010SP."))
o.default = "BVL3A5HNAAG010SP"

o = s:taboption("identity", Value, "hw_ver", translate("Hardware Version"),
	translate("Hardware version string reported to OLT. e.g. 3FE56641AAAA01."))
o.default = "3FE56641AAAA01"

-- OMCI Tab
o = s:taboption("omci", ListValue, "uni_type", translate("UNI Interface Type"),
	translate("Select UNI data plane model."))
o:value("veip", translate("VEIP (Virtual Ethernet Interface Point - Recommended)"))
o:value("pptp", translate("PPTP (Physical Path Termination Point)"))
o.default = "veip"

o = s:taboption("omci", ListValue, "mib_file", translate("OMCI MIB Template"),
	translate("Select baseline MIB template loaded by omcid."))
o:value("data_1v_8q.ini", "data_1v_8q.ini (VEIP 1V 8Q)")
o:value("data_1g_8q_us1280_ds512.ini", "data_1g_8q_us1280_ds512.ini (PPTP 1G 8Q)")
o.default = "data_1v_8q.ini"

o = s:taboption("omci", Value, "omcc_ver", translate("OMCC Version"),
	translate("Baseline message set version. Default 160 (0xA0 = ITU-T G.988)."))
o.default = "160"

o = s:taboption("omci", Value, "iop_mask", translate("Interoperability (IOP) Mask"),
	translate("OLT vendor quirk workaround bitmask. Default 0 (Standard)."))
o.default = "0"

-- Hardware Tab
o = s:taboption("hardware", DummyValue, "_driver", translate("GPON Driver"),
	translate("Unified Open-Source Kernel Driver (gpon_falcon.ko) for Linux 6.12"))

function m.on_after_commit(self)
	local uci = require("luci.model.uci").cursor()
	local sn = uci:get("gpon", "config", "gpon_sn") or ""
	local pwd = uci:get("gpon", "config", "ploam_pwd") or ""

	-- Sync with sysfs parameter
	if #sn > 0 then
		luci.sys.call("echo '" .. sn .. "' > /sys/module/gpon_falcon/parameters/onu_sn 2>/dev/null")
		luci.sys.call("echo 'gpon_falcon onu_sn=\"" .. sn .. "\" onu_pwd=\"" .. pwd .. "\"' > /etc/modules.d/50-gpon-falcon 2>/dev/null")
	end

	-- Restart omcid
	luci.sys.call("/etc/init.d/omcid restart 2>/dev/null")
end

return m
