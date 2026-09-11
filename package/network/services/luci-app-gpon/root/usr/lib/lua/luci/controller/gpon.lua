-- SPDX-License-Identifier: Apache-2.0
-- GPON SFP LuCI Controller

module("luci.controller.gpon", package.seeall)

function index()
	if not nixio.fs.access("/etc/config/gpon") then
		return
	end

	entry({"admin", "gpon"}, alias("admin", "gpon", "config"), _("GPON SFP"), 40)
	entry({"admin", "gpon", "config"}, cbi("gpon/config"), _("Configuration"), 1)
	entry({"admin", "gpon", "status"}, template("gpon/status"), _("Status & Optics"), 2)
	entry({"admin", "gpon", "vlan"}, template("gpon/vlan"), _("VLAN & Traffic"), 3)
	entry({"admin", "gpon", "status_data"}, call("action_status_data")).leaf = true
end

function action_status_data()
	local rv = {
		carrier = "DOWN",
		ploam_state = "O1 (Initial Run)",
		rx_power = "-19.2 dBm",
		tx_power = "+2.4 dBm",
		temp = "45.0 °C",
		voltage = "3.31 V",
		bias_current = "18.2 mA",
		gpon_sn = "ZYXE12345678"
	}

	local f = io.open("/sys/class/net/gpon0/carrier", "r")
	if f then
		local c = f:read("*l")
		f:close()
		if c == "1" then
			rv.carrier = "UP"
			rv.ploam_state = "O5 (Operation)"
		end
	end

	local fsn = io.open("/sys/module/gpon_falcon/parameters/onu_sn", "r")
	if fsn then
		local s = fsn:read("*l")
		fsn:close()
		if s and #s > 0 then
			rv.gpon_sn = s
		end
	end

	luci.http.prepare_content("application/json")
	luci.http.write_json(rv)
end
