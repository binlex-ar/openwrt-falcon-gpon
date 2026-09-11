// SPDX-License-Identifier: GPL-2.0+
/*
 * falcon_ploam.c - ITU-T G.984.3 PLOAM State Machine for Lantiq Falcon SoC
 *
 * Clean-room reverse-engineered implementation.
 *
 * Copyright (C) 2026
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/string.h>

#include "falcon_gtc_regs.h"
#include "falcon_ploam.h"

#define GTC_PLOAM_DS_CTRL		0x05C
#define GTC_PLOAM_DS_DATA0		0x060
#define GTC_PLOAM_DS_DATA1		0x064
#define GTC_PLOAM_DS_DATA2		0x068
#define GTC_ONU_ID_REG			0x06C
#define GTC_PLOAM_DS_INT		0x070

#define GTC_DELAY_CTRL			0x100
#define GTC_RANGED_DELAY		0x104
#define GTC_DELAY_CTRL_EN		BIT(28)

#define GTC_PLOAM_US_CTRL		0x128
#define GTC_PLOAM_US_DATA0		0x12C
#define GTC_PLOAM_US_DATA1		0x130
#define GTC_PLOAM_US_DATA2		0x134

const char *falcon_ploam_state_str(enum gpon_ploam_state state)
{
	switch (state) {
	case PLOAM_STATE_O1_INITIAL_RUN: return "O1 (Initial-Run)";
	case PLOAM_STATE_O2_STANDBY:     return "O2 (Standby)";
	case PLOAM_STATE_O3_SERIAL_NUM:  return "O3 (Serial Number)";
	case PLOAM_STATE_O4_RANGING:     return "O4 (Ranging)";
	case PLOAM_STATE_O5_OPERATION:   return "O5 (Operation)";
	case PLOAM_STATE_O6_POPUP:       return "O6 (POPUP)";
	case PLOAM_STATE_O7_EMERGENCY:   return "O7 (Emergency Stop)";
	default:                         return "UNKNOWN";
	}
}

static inline u32 gtc_read(struct falcon_ploam_ctx *ctx, u32 reg)
{
	return ioread32be(ctx->gtc_base + reg);
}

static inline void gtc_write(struct falcon_ploam_ctx *ctx, u32 val, u32 reg)
{
	iowrite32be(val, ctx->gtc_base + reg);
}

/*
 * gtc_ploam_send - Write 12-byte PLOAM message to GTC Upstream FIFO
 */
static int gtc_ploam_send(struct falcon_ploam_ctx *ctx, const struct gpon_ploam_msg *msg)
{
	u32 w0, w1, w2;

	w0 = ((u32)msg->onu_id << 24) | ((u32)msg->msg_id << 16) |
	     ((u32)msg->seq_num << 8) | msg->payload[0];
	w1 = ((u32)msg->payload[1] << 24) | ((u32)msg->payload[2] << 16) |
	     ((u32)msg->payload[3] << 8)  | msg->payload[4];
	w2 = ((u32)msg->payload[5] << 24) | ((u32)msg->payload[6] << 16) |
	     ((u32)msg->payload[7] << 8)  | msg->payload[8];

	gtc_write(ctx, w0, GTC_PLOAM_US_DATA0);
	gtc_write(ctx, w1, GTC_PLOAM_US_DATA1);
	gtc_write(ctx, w2, GTC_PLOAM_US_DATA2);

	/* Trigger transmission in Upstream PLOAM control register */
	gtc_write(ctx, 0x01, GTC_PLOAM_US_CTRL);

	return 0;
}

/*
 * gtc_ploam_rd - Read 12-byte PLOAM message from GTC Downstream FIFO
 */
static bool gtc_ploam_rd(struct falcon_ploam_ctx *ctx, struct gpon_ploam_msg *msg)
{
	u32 status, w0, w1, w2;

	status = gtc_read(ctx, GTC_PLOAM_DS_CTRL);
	if (!(status & 0x01))
		return false; /* FIFO Empty */

	w0 = gtc_read(ctx, GTC_PLOAM_DS_DATA0);
	w1 = gtc_read(ctx, GTC_PLOAM_DS_DATA1);
	w2 = gtc_read(ctx, GTC_PLOAM_DS_DATA2);

	msg->onu_id     = (w0 >> 24) & 0xFF;
	msg->msg_id     = (w0 >> 16) & 0xFF;
	msg->seq_num    = (w0 >> 8)  & 0xFF;
	msg->payload[0] = w0 & 0xFF;

	msg->payload[1] = (w1 >> 24) & 0xFF;
	msg->payload[2] = (w1 >> 16) & 0xFF;
	msg->payload[3] = (w1 >> 8)  & 0xFF;
	msg->payload[4] = w1 & 0xFF;

	msg->payload[5] = (w2 >> 24) & 0xFF;
	msg->payload[6] = (w2 >> 16) & 0xFF;
	msg->payload[7] = (w2 >> 8)  & 0xFF;
	msg->payload[8] = w2 & 0xFF;

	/* Filter by ONU ID: must match assigned ONU ID or broadcast 0xFF */
	if (msg->onu_id != 0xFF && msg->onu_id != ctx->assigned_onu_id && ctx->assigned_onu_id != 0)
		return false;

	return true;
}

static void ploam_state_transition(struct falcon_ploam_ctx *ctx,
				  enum gpon_ploam_state new_state)
{
	enum gpon_ploam_state old_state = ctx->state;

	if (old_state == new_state)
		return;

	pr_info("falcon_ploam: state change [%s] -> [%s]\n",
		falcon_ploam_state_str(old_state),
		falcon_ploam_state_str(new_state));

	ctx->state = new_state;

	if (ctx->state_change_cb)
		ctx->state_change_cb(new_state, old_state);

	switch (new_state) {
	case PLOAM_STATE_O1_INITIAL_RUN:
	case PLOAM_STATE_O2_STANDBY:
		if (ctx->laser_set_enable)
			ctx->laser_set_enable(false);
		if (ctx->carrier_set)
			ctx->carrier_set(false);
		/* Clear ONU-ID */
		ctx->assigned_onu_id = 0;
		gtc_write(ctx, 0, GTC_ONU_ID_REG);
		break;

	case PLOAM_STATE_O3_SERIAL_NUM:
		/* Laser burst mode armed, but transmission only on grant */
		if (ctx->laser_set_enable)
			ctx->laser_set_enable(true);
		schedule_delayed_work(&ctx->to1_timer, msecs_to_jiffies(10000));
		break;

	case PLOAM_STATE_O4_RANGING:
		cancel_delayed_work(&ctx->to1_timer);
		break;

	case PLOAM_STATE_O5_OPERATION:
		cancel_delayed_work(&ctx->to1_timer);
		cancel_delayed_work(&ctx->to2_timer);
		if (ctx->carrier_set)
			ctx->carrier_set(true);
		pr_info("falcon_ploam: ONU in O5 (OPERATION). Link UP!\n");
		break;

	case PLOAM_STATE_O6_POPUP:
		if (ctx->laser_set_enable)
			ctx->laser_set_enable(false);
		schedule_delayed_work(&ctx->to2_timer, msecs_to_jiffies(100));
		break;

	case PLOAM_STATE_O7_EMERGENCY:
		if (ctx->laser_set_enable)
			ctx->laser_set_enable(false);
		if (ctx->carrier_set)
			ctx->carrier_set(false);
		pr_alert("falcon_ploam: EMERGENCY STOP! Laser killed.\n");
		break;
	}
}

static void send_ack(struct falcon_ploam_ctx *ctx, u8 seq_num)
{
	struct gpon_ploam_msg ack = {
		.onu_id  = ctx->assigned_onu_id,
		.msg_id  = PLOAM_US_ACKNOWLEDGE,
		.seq_num = seq_num,
	};
	gtc_ploam_send(ctx, &ack);
}

static void send_serial_number_onu(struct falcon_ploam_ctx *ctx)
{
	struct gpon_ploam_msg sn_msg = {
		.onu_id  = 0xFF,
		.msg_id  = PLOAM_US_SERIAL_NUMBER_ONU,
		.seq_num = 0x01,
	};
	/* Payload: 8 bytes Serial Number + 1 byte random delay */
	memcpy(sn_msg.payload, ctx->serial_number, 8);
	sn_msg.payload[8] = 0x00;

	gtc_ploam_send(ctx, &sn_msg);
	pr_info("falcon_ploam: Sent Serial_Number_ONU: %c%c%c%c-%02X%02X%02X%02X\n",
		ctx->serial_number[0], ctx->serial_number[1],
		ctx->serial_number[2], ctx->serial_number[3],
		ctx->serial_number[4], ctx->serial_number[5],
		ctx->serial_number[6], ctx->serial_number[7]);
}

static void send_password(struct falcon_ploam_ctx *ctx, u8 seq_num)
{
	struct gpon_ploam_msg pwd_msg = {
		.onu_id  = ctx->assigned_onu_id,
		.msg_id  = PLOAM_US_PASSWORD,
		.seq_num = seq_num,
	};
	memcpy(pwd_msg.payload, ctx->password, sizeof(pwd_msg.payload));
	gtc_ploam_send(ctx, &pwd_msg);
}

/*
 * falcon_ploam_fsm_worker - Process queued downstream messages
 */
static void falcon_ploam_fsm_worker(struct work_struct *work)
{
	struct falcon_ploam_ctx *ctx =
		container_of(work, struct falcon_ploam_ctx, fsm_work);
	struct gpon_ploam_msg msg;
	unsigned long flags;

	while (1) {
		spin_lock_irqsave(&ctx->lock, flags);
		if (ctx->rx_head == ctx->rx_tail) {
			spin_unlock_irqrestore(&ctx->lock, flags);
			break;
		}
		msg = ctx->rx_queue[ctx->rx_tail];
		ctx->rx_tail = (ctx->rx_tail + 1) & 0x3F;
		spin_unlock_irqrestore(&ctx->lock, flags);

		switch (msg.msg_id) {
		case PLOAM_DS_UPSTREAM_OVERHEAD:
			/* Upstream Overhead broadcast */
			ctx->overhead.preamble_len    = msg.payload[0];
			ctx->overhead.delimiter_len   = msg.payload[1];
			ctx->overhead.power_level_mode = msg.payload[8];
			if (ctx->state == PLOAM_STATE_O1_INITIAL_RUN ||
			    ctx->state == PLOAM_STATE_O2_STANDBY) {
				ploam_state_transition(ctx, PLOAM_STATE_O3_SERIAL_NUM);
				send_serial_number_onu(ctx);
			}
			break;

		case PLOAM_DS_ASSIGN_ONU_ID:
			/* Byte 3..10 = Serial Number, Byte 11 = Assigned ONU-ID */
			if (memcmp(msg.payload, ctx->serial_number, 8) == 0) {
				ctx->assigned_onu_id = msg.payload[8];
				gtc_write(ctx, ctx->assigned_onu_id, GTC_ONU_ID_REG);
				pr_info("falcon_ploam: Assigned ONU-ID = %d\n",
					ctx->assigned_onu_id);
				send_ack(ctx, msg.seq_num);
				ploam_state_transition(ctx, PLOAM_STATE_O4_RANGING);
			}
			break;

		case PLOAM_DS_RANGING_TIME:
			if (ctx->state == PLOAM_STATE_O4_RANGING ||
			    ctx->state == PLOAM_STATE_O5_OPERATION) {
				u32 eqd = ((u32)msg.payload[0] << 24) |
					  ((u32)msg.payload[1] << 16) |
					  ((u32)msg.payload[2] << 8)  |
					  msg.payload[3];
				ctx->eqd = eqd;
				gtc_write(ctx, eqd, GTC_RANGED_DELAY);
				gtc_write(ctx, gtc_read(ctx, GTC_DELAY_CTRL) | GTC_DELAY_CTRL_EN,
					  GTC_DELAY_CTRL);
				pr_info("falcon_ploam: Programmed EqD = 0x%08X\n", eqd);
				send_ack(ctx, msg.seq_num);
				ploam_state_transition(ctx, PLOAM_STATE_O5_OPERATION);
			}
			break;

		case PLOAM_DS_REQUEST_PASSWORD:
			send_password(ctx, msg.seq_num);
			break;

		case PLOAM_DS_DEACTIVATE_ONU_ID:
			ploam_state_transition(ctx, PLOAM_STATE_O2_STANDBY);
			break;

		case PLOAM_DS_DISABLE_SERIAL_NUM:
			ploam_state_transition(ctx, PLOAM_STATE_O7_EMERGENCY);
			break;

		default:
			break;
		}
	}
}

/*
 * Timers
 */
static void to1_timer_fn(struct work_struct *work)
{
	struct falcon_ploam_ctx *ctx =
		container_of(to_delayed_work(work), struct falcon_ploam_ctx, to1_timer);

	pr_warn("falcon_ploam: TO1 expired, returning to O2\n");
	ploam_state_transition(ctx, PLOAM_STATE_O2_STANDBY);
}

static void to2_timer_fn(struct work_struct *work)
{
	struct falcon_ploam_ctx *ctx =
		container_of(to_delayed_work(work), struct falcon_ploam_ctx, to2_timer);

	pr_warn("falcon_ploam: TO2 expired (downstream sync lost), returning to O1\n");
	ploam_state_transition(ctx, PLOAM_STATE_O1_INITIAL_RUN);
}

/*
 * External interrupt & event callbacks
 */
void falcon_ploam_rx_irq_handler(struct falcon_ploam_ctx *ctx)
{
	struct gpon_ploam_msg msg;
	unsigned long flags;

	while (gtc_ploam_rd(ctx, &msg)) {
		spin_lock_irqsave(&ctx->lock, flags);
		ctx->rx_queue[ctx->rx_head] = msg;
		ctx->rx_head = (ctx->rx_head + 1) & 0x3F;
		spin_unlock_irqrestore(&ctx->lock, flags);
	}
	schedule_work(&ctx->fsm_work);
}

void falcon_ploam_event_los(struct falcon_ploam_ctx *ctx, bool los)
{
	if (los) {
		pr_warn("falcon_ploam: Loss of Signal (LOS) detected!\n");
		ploam_state_transition(ctx, PLOAM_STATE_O1_INITIAL_RUN);
	}
}

void falcon_ploam_event_lof(struct falcon_ploam_ctx *ctx, bool lof)
{
	if (lof) {
		pr_warn("falcon_ploam: Loss of Frame (LOF) detected!\n");
		if (ctx->state == PLOAM_STATE_O5_OPERATION)
			ploam_state_transition(ctx, PLOAM_STATE_O6_POPUP);
		else
			ploam_state_transition(ctx, PLOAM_STATE_O1_INITIAL_RUN);
	} else {
		if (ctx->state == PLOAM_STATE_O1_INITIAL_RUN)
			ploam_state_transition(ctx, PLOAM_STATE_O2_STANDBY);
	}
}

int falcon_ploam_init(struct falcon_ploam_ctx *ctx, void __iomem *gtc_base,
		      const u8 *sn, const u8 *pwd)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->gtc_base = gtc_base;
	ctx->state = PLOAM_STATE_O1_INITIAL_RUN;

	if (sn)
		memcpy(ctx->serial_number, sn, 8);
	else
		memcpy(ctx->serial_number, "ZYXE\x12\x34\x56\x78", 8);

	if (pwd)
		memcpy(ctx->password, pwd, 10);

	spin_lock_init(&ctx->lock);
	INIT_WORK(&ctx->fsm_work, falcon_ploam_fsm_worker);
	INIT_DELAYED_WORK(&ctx->to1_timer, to1_timer_fn);
	INIT_DELAYED_WORK(&ctx->to2_timer, to2_timer_fn);

	pr_info("falcon_ploam: initialized for SN %c%c%c%c-%02X%02X%02X%02X in state O1\n",
		ctx->serial_number[0], ctx->serial_number[1],
		ctx->serial_number[2], ctx->serial_number[3],
		ctx->serial_number[4], ctx->serial_number[5],
		ctx->serial_number[6], ctx->serial_number[7]);

	return 0;
}

void falcon_ploam_destroy(struct falcon_ploam_ctx *ctx)
{
	cancel_delayed_work_sync(&ctx->to1_timer);
	cancel_delayed_work_sync(&ctx->to2_timer);
	cancel_work_sync(&ctx->fsm_work);
	ploam_state_transition(ctx, PLOAM_STATE_O1_INITIAL_RUN);
}

