/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * falcon_onu_dev.h - Character device (/dev/onu0, /dev/optic0) and ioctl bridge
 * for OMCI daemon (omcid) on Lantiq Falcon SoC.
 *
 * Copyright (C) 2026
 */

#ifndef __FALCON_ONU_DEV_H__
#define __FALCON_ONU_DEV_H__

#include <linux/types.h>
#include <linux/ioctl.h>
#include "falcon_ploam.h"

/* Magic Numbers for MIPS ioctl encoding */
#define ONU_MAGIC           1
#define ONU_EVENT_MAGIC     2
#define PLOAM_MAGIC         3
#define GPE_MAGIC           4
#define GPE_TABLE_MAGIC     5
#define GTC_MAGIC           6
#define LAN_MAGIC           7
#define OPTIC_MAGIC         9
#define OPTIC_EVENT_MAGIC   10

/* Event Notification IDs */
#define ONU_EVENT_HARDWARE          0
#define ONU_EVENT_PLOAM_DS          1
#define ONU_EVENT_PLOAM_US          2
#define ONU_EVENT_STATE_CHANGE      3
#define ONU_EVENT_OMCI_RECEIVE      4
#define ONU_EVENT_GTC_TCA           5
#define ONU_EVENT_GPE_TCA           6
#define ONU_EVENT_LAN_TCA           7
#define ONU_EVENT_GTC_STATUS_CHANGE 8
#define ONU_EVENT_OMCI_SENT         11
#define ONU_EVENT_LINK_STATE_CHANGE 13

#define ONU_GPE_MAX_OMCI_FRAME_LEN  1980

/* Event FIFO header */
struct onu_fifo_header {
	uint32_t id;
	uint32_t len;
} __packed;

/* PLOAM FSM state retrieval */
struct ploam_state_data_get {
	uint32_t curr_state;
	uint32_t previous_state;
	uint32_t elapsed_msec;
} __packed;

/* OMCI message payload */
struct gpe_omci_msg {
	uint32_t length;
	uint8_t  message[ONU_GPE_MAX_OMCI_FRAME_LEN];
} __packed;

/* Link State Event */
struct onu_link_state {
	uint32_t port;
	uint32_t old_state;
	uint32_t new_state;
} __packed;

/* Combined event FIFO data */
struct onu_fifo_data {
	struct onu_fifo_header header;
	union {
		uint32_t val32;
		struct ploam_state_data_get state;
		struct gpe_omci_msg omci_message;
		struct onu_link_state link_state;
		uint8_t raw[2048];
	} data;
} __packed;

/* GPE Hardware Capabilities */
struct gpe_capability {
	uint32_t max_meter;
	uint32_t max_gpix;
	uint32_t max_eth_uni;
	uint32_t max_pots_uni;
	uint32_t max_bridge_port;
	uint32_t hw_version;
	uint32_t _v751_reserved;
} __packed;

/* ONU Version String */
struct onu_version_string {
	char onu_version[80];
	char fw_version[80];
	char cop_version[80];
	char sce_interface_version[80];
	char chip_id[80];
	char device_type[80];
} __packed;

/* Optic Version String */
struct optic_versionstring {
	char driver_version[80];
};

/* Core Ioctls */
#define FIO_ONU_VERSION_GET       _IOR(ONU_MAGIC, 4, struct onu_version_string)
#define FIO_ONU_LINE_ENABLE_SET   _IOW(ONU_MAGIC, 5, uint32_t)
#define FIO_ONU_LINE_ENABLE_GET   _IOR(ONU_MAGIC, 6, uint32_t)

#define FIO_ONU_EVENT_FIFO        _IOR(ONU_EVENT_MAGIC, 200, struct onu_fifo_header)
#define FIO_ONU_EVENT_ENABLE_SET  _IOW(ONU_EVENT_MAGIC, 201, uint32_t)
#define FIO_ONU_EVENT_ENABLE_GET  _IOR(ONU_EVENT_MAGIC, 202, uint32_t)

#define FIO_GPE_CAPABILITY_GET    _IOR(GPE_MAGIC, 0x61, struct gpe_capability)
#define FIO_GPE_OMCI_SEND         _IOW(GPE_MAGIC, 0x24, struct gpe_omci_msg)

#define FIO_OPTIC_EVENT_FIFO      _IOR(OPTIC_EVENT_MAGIC, 0, struct onu_fifo_header)
#define FIO_OPTIC_EVENT_SET       _IOW(OPTIC_EVENT_MAGIC, 1, uint32_t)
#define FIO_OPTIC_VERSION_GET     _IOR(OPTIC_MAGIC, 4, struct optic_versionstring)

/* Forward declaration */
struct falcon_gpon_priv;

int falcon_onu_dev_init(struct falcon_gpon_priv *priv);
void falcon_onu_dev_exit(struct falcon_gpon_priv *priv);
void falcon_onu_notify_ploam_state(enum gpon_ploam_state new_state,
				   enum gpon_ploam_state old_state);
void falcon_onu_notify_omci_rx(const u8 *data, size_t len);

#endif /* __FALCON_ONU_DEV_H__ */
