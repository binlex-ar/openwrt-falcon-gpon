/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * falcon_ploam.h - ITU-T G.984.3 PLOAM Protocol Engine for Lantiq Falcon
 *
 * Clean-room reverse-engineered implementation.
 * Copyright (C) 2026
 */

#ifndef __FALCON_PLOAM_H__
#define __FALCON_PLOAM_H__

#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>

/* ITU-T G.984.3 PLOAM States */
enum gpon_ploam_state {
	PLOAM_STATE_O1_INITIAL_RUN = 1,
	PLOAM_STATE_O2_STANDBY     = 2,
	PLOAM_STATE_O3_SERIAL_NUM  = 3,
	PLOAM_STATE_O4_RANGING     = 4,
	PLOAM_STATE_O5_OPERATION   = 5,
	PLOAM_STATE_O6_POPUP       = 6,
	PLOAM_STATE_O7_EMERGENCY   = 7,
};

/* Downstream PLOAM Message IDs (ITU-T G.984.3 Table 9-1) */
#define PLOAM_DS_UPSTREAM_OVERHEAD	0x01
#define PLOAM_DS_SERIAL_NUMBER_MASK	0x02
#define PLOAM_DS_ASSIGN_ONU_ID		0x03
#define PLOAM_DS_RANGING_TIME		0x04
#define PLOAM_DS_DEACTIVATE_ONU_ID	0x05
#define PLOAM_DS_DISABLE_SERIAL_NUM	0x06
#define PLOAM_DS_CONFIGURE_VP_VC	0x07
#define PLOAM_DS_ENCRYPTED_PORT_ID	0x08
#define PLOAM_DS_REQUEST_PASSWORD	0x09
#define PLOAM_DS_ASSIGNED_ALLOC_ID	0x0A
#define PLOAM_DS_NOMINAL_BIT_RATE	0x0B
#define PLOAM_DS_SLEEP_MODE		0x0C
#define PLOAM_DS_BER_INTERVAL		0x11
#define PLOAM_DS_KEY_SWITCHING_TIME	0x12
#define PLOAM_DS_EXT_BURST_LENGTH	0x15

/* Upstream PLOAM Message IDs (ITU-T G.984.3 Table 9-2) */
#define PLOAM_US_SERIAL_NUMBER_ONU	0x01
#define PLOAM_US_PASSWORD		0x02
#define PLOAM_US_DYING_GASP		0x03
#define PLOAM_US_ENCRYPTION_KEY		0x05
#define PLOAM_US_PEK			0x06
#define PLOAM_US_ACKNOWLEDGE		0x09
#define PLOAM_US_SLEEP_REQUEST		0x10

/* Raw PLOAM Message Structure: 1 + 1 + 10 = 12 bytes */
struct gpon_ploam_msg {
	u8 onu_id;			/* Byte 0: ONU-ID (0xFF = broadcast) */
	u8 msg_id;			/* Byte 1: Message Type ID */
	u8 seq_num;			/* Byte 2: Sequence Number */
	u8 payload[9];			/* Bytes 3..11: Payload data */
} __packed;

/* Upstream Overhead parameters parsed from PLOAM 0x01 */
struct gpon_us_overhead {
	u8 preamble_len;
	u8 delimiter_len;
	u8 preamble_pat[3];
	u8 delimiter_pat[3];
	u8 power_level_mode;
};

/* Full PLOAM context state */
struct falcon_ploam_ctx {
	void __iomem		*gtc_base;
	enum gpon_ploam_state	state;
	u8			assigned_onu_id;
	u8			serial_number[8];	/* 4-byte Vendor ID + 4-byte VSN */
	u8			password[10];		/* PLOAM Registration password */
	u32			eqd;			/* Equalization Delay in bits */
	struct gpon_us_overhead	overhead;
	
	/* Ring buffer for received downstream PLOAMs */
	struct gpon_ploam_msg	rx_queue[64];
	u8			rx_head;
	u8			rx_tail;
	spinlock_t		lock;

	/* Workqueue for asynchronous FSM handling */
	struct work_struct	fsm_work;
	struct delayed_work	to1_timer;		/* Serial number timeout (10s) */
	struct delayed_work	to2_timer;		/* POPUP timeout (100ms) */

	/* Callbacks into BOSA optics & netdev */
	void (*laser_set_enable)(bool enable);
	void (*carrier_set)(bool link_up);
};

/* Public API */
int falcon_ploam_init(struct falcon_ploam_ctx *ctx, void __iomem *gtc_base,
		      const u8 *sn, const u8 *pwd);
void falcon_ploam_destroy(struct falcon_ploam_ctx *ctx);
void falcon_ploam_rx_irq_handler(struct falcon_ploam_ctx *ctx);
void falcon_ploam_event_los(struct falcon_ploam_ctx *ctx, bool los);
void falcon_ploam_event_lof(struct falcon_ploam_ctx *ctx, bool lof);
const char *falcon_ploam_state_str(enum gpon_ploam_state state);

#endif /* __FALCON_PLOAM_H__ */
