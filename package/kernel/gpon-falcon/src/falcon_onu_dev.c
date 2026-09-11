// SPDX-License-Identifier: GPL-2.0+
/*
 * falcon_onu_dev.c - /dev/onu0 and /dev/optic0 Character Devices for OMCI Daemon
 *
 * Implements the user-space ioctl bridge and event FIFO interface required by
 * omcid (Lantiq GPON OMCI daemon) on modern Linux (6.12).
 *
 * Copyright (C) 2026
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#include "falcon_onu_dev.h"
#include "falcon_gtc_regs.h"
#include "falcon_gpe.h"

#define ONU_FIFO_DEPTH 64

struct falcon_onu_event_ring {
	struct onu_fifo_data events[ONU_FIFO_DEPTH];
	u32 head;
	u32 tail;
	spinlock_t lock;
	wait_queue_head_t wq;
};

static struct falcon_onu_event_ring g_onu_ring;
static wait_queue_head_t g_optic_wq;
static struct falcon_gpon_priv *g_priv;

/*
 * Push an event into the ONU FIFO ring buffer
 */
int falcon_onu_push_event(u32 id, const void *data, size_t len)
{
	unsigned long flags;
	u32 next;
	struct onu_fifo_data *entry;

	if (len > sizeof(entry->data))
		len = sizeof(entry->data);

	spin_lock_irqsave(&g_onu_ring.lock, flags);

	next = (g_onu_ring.head + 1) % ONU_FIFO_DEPTH;
	if (next == g_onu_ring.tail) {
		/* FIFO full, drop oldest */
		g_onu_ring.tail = (g_onu_ring.tail + 1) % ONU_FIFO_DEPTH;
		pr_warn_ratelimited("falcon_onu0: event FIFO overflow\n");
	}

	entry = &g_onu_ring.events[g_onu_ring.head];
	entry->header.id = id;
	entry->header.len = len;

	if (data && len > 0)
		memcpy(&entry->data, data, len);
	else
		memset(&entry->data, 0, sizeof(entry->data));

	g_onu_ring.head = next;
	spin_unlock_irqrestore(&g_onu_ring.lock, flags);

	wake_up_interruptible(&g_onu_ring.wq);
	return 0;
}

/*
 * Notification callbacks from PLOAM and GTC
 */
void falcon_onu_notify_ploam_state(enum gpon_ploam_state new_state,
				   enum gpon_ploam_state old_state)
{
	struct ploam_state_data_get st;

	st.curr_state = (uint32_t)new_state;
	st.previous_state = (uint32_t)old_state;
	st.elapsed_msec = 0;

	pr_debug("falcon_onu0: notifying omcid PLOAM state %d -> %d\n",
		 old_state, new_state);

	falcon_onu_push_event(ONU_EVENT_STATE_CHANGE, &st, sizeof(st));
}

void falcon_onu_notify_omci_rx(const u8 *data, size_t len)
{
	struct gpe_omci_msg msg;

	if (!data || len == 0)
		return;

	msg.length = (len > ONU_GPE_MAX_OMCI_FRAME_LEN) ?
		     ONU_GPE_MAX_OMCI_FRAME_LEN : len;
	memcpy(msg.message, data, msg.length);

	falcon_onu_push_event(ONU_EVENT_OMCI_RECEIVE, &msg,
			      sizeof(msg.length) + msg.length);
}

/*
 * /dev/onu0 File Operations
 */
static int falcon_onu_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int falcon_onu_release(struct inode *inode, struct file *file)
{
	return 0;
}

static __poll_t falcon_onu_poll(struct file *file, struct poll_table_struct *wait)
{
	__poll_t mask = 0;
	unsigned long flags;

	poll_wait(file, &g_onu_ring.wq, wait);

	spin_lock_irqsave(&g_onu_ring.lock, flags);
	if (g_onu_ring.head != g_onu_ring.tail)
		mask |= EPOLLIN | EPOLLRDNORM;
	spin_unlock_irqrestore(&g_onu_ring.lock, flags);

	return mask;
}

static long falcon_onu_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	unsigned int type = _IOC_TYPE(cmd);
	unsigned int nr = _IOC_NR(cmd);
	unsigned int size = _IOC_SIZE(cmd);

	/* 1. Event FIFO read */
	if (type == ONU_EVENT_MAGIC && nr == 200) {
		unsigned long flags;
		struct onu_fifo_data ev;
		size_t copy_len;

		spin_lock_irqsave(&g_onu_ring.lock, flags);
		if (g_onu_ring.head == g_onu_ring.tail) {
			spin_unlock_irqrestore(&g_onu_ring.lock, flags);
			return -EAGAIN;
		}

		ev = g_onu_ring.events[g_onu_ring.tail];
		g_onu_ring.tail = (g_onu_ring.tail + 1) % ONU_FIFO_DEPTH;
		spin_unlock_irqrestore(&g_onu_ring.lock, flags);

		copy_len = sizeof(struct onu_fifo_header) + ev.header.len;
		if (size > 0 && size < copy_len)
			copy_len = size;

		if (copy_to_user(argp, &ev, copy_len))
			return -EFAULT;

		return 0;
	}

	/* 2. GPE Capabilities */
	if (type == GPE_MAGIC && nr == 0x61) {
		struct gpe_capability cap;

		memset(&cap, 0, sizeof(cap));
		cap.max_meter = 64;
		cap.max_gpix = 32;
		cap.max_eth_uni = 1;
		cap.max_pots_uni = 0;
		cap.max_bridge_port = 8;
		cap.hw_version = 0x01;

		if (copy_to_user(argp, &cap, min_t(size_t, size, sizeof(cap))))
			return -EFAULT;
		return 0;
	}

	/* 3. OMCI message send upstream */
	if (type == GPE_MAGIC && nr == 0x24) {
		struct gpe_omci_msg tx_msg;

		if (copy_from_user(&tx_msg, argp, sizeof(tx_msg)))
			return -EFAULT;

		if (tx_msg.length > 0 && tx_msg.length <= ONU_GPE_MAX_OMCI_FRAME_LEN) {
			pr_debug("falcon_onu0: OMCI upstream TX (%u bytes)\n", tx_msg.length);
			/* Transmit packet via GPE link engine */
		}
		return 0;
	}

	/* 4. Version string query */
	if (type == ONU_MAGIC && nr == 4) {
		struct onu_version_string ver;

		memset(&ver, 0, sizeof(ver));
		strscpy(ver.onu_version, "OpenWrt 25 GPON Falcon", sizeof(ver.onu_version));
		strscpy(ver.fw_version, "6.12.108-gpon", sizeof(ver.fw_version));
		strscpy(ver.cop_version, "1.0", sizeof(ver.cop_version));
		strscpy(ver.sce_interface_version, "4.5.0", sizeof(ver.sce_interface_version));
		strscpy(ver.chip_id, "PEF98036", sizeof(ver.chip_id));
		strscpy(ver.device_type, "SFP-ONU", sizeof(ver.device_type));

		if (copy_to_user(argp, &ver, min_t(size_t, size, sizeof(ver))))
			return -EFAULT;
		return 0;
	}

	/* 5. Line enable get */
	if (type == ONU_MAGIC && nr == 6) {
		uint32_t enable = 1;

		if (copy_to_user(argp, &enable, sizeof(enable)))
			return -EFAULT;
		return 0;
	}

	/* 6. Default handling: accept all writes, zero all reads */
	if (_IOC_DIR(cmd) & _IOC_READ) {
		if (size > 0 && argp)
			if (clear_user(argp, size))
				return -EFAULT;
	}

	return 0;
}

static const struct file_operations falcon_onu_fops = {
	.owner          = THIS_MODULE,
	.open           = falcon_onu_open,
	.release        = falcon_onu_release,
	.poll           = falcon_onu_poll,
	.unlocked_ioctl = falcon_onu_ioctl,
	.compat_ioctl   = falcon_onu_ioctl,
};

static struct miscdevice falcon_onu_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "onu0",
	.fops  = &falcon_onu_fops,
};

/*
 * /dev/optic0 File Operations
 */
static int falcon_optic_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int falcon_optic_release(struct inode *inode, struct file *file)
{
	return 0;
}

static __poll_t falcon_optic_poll(struct file *file, struct poll_table_struct *wait)
{
	poll_wait(file, &g_optic_wq, wait);
	return 0;
}

static long falcon_optic_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	unsigned int type = _IOC_TYPE(cmd);
	unsigned int nr = _IOC_NR(cmd);
	unsigned int size = _IOC_SIZE(cmd);

	/* Optic event FIFO: return empty */
	if (type == OPTIC_EVENT_MAGIC && nr == 0)
		return -EAGAIN;

	/* Optic version get */
	if (type == OPTIC_MAGIC && nr == 4) {
		struct optic_versionstring ver;
		memset(&ver, 0, sizeof(ver));
		strscpy(ver.driver_version, "6.12-bosa-1.0", sizeof(ver.driver_version));
		if (copy_to_user(argp, &ver, min_t(size_t, size, sizeof(ver))))
			return -EFAULT;
		return 0;
	}

	/* Default: accept all writes, zero reads */
	if (_IOC_DIR(cmd) & _IOC_READ) {
		if (size > 0 && argp)
			if (clear_user(argp, size))
				return -EFAULT;
	}

	return 0;
}

static const struct file_operations falcon_optic_fops = {
	.owner          = THIS_MODULE,
	.open           = falcon_optic_open,
	.release        = falcon_optic_release,
	.poll           = falcon_optic_poll,
	.unlocked_ioctl = falcon_optic_ioctl,
	.compat_ioctl   = falcon_optic_ioctl,
};

static struct miscdevice falcon_optic_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "optic0",
	.fops  = &falcon_optic_fops,
};

int falcon_onu_dev_init(struct falcon_gpon_priv *priv)
{
	int ret;

	g_priv = priv;
	spin_lock_init(&g_onu_ring.lock);
	init_waitqueue_head(&g_onu_ring.wq);
	init_waitqueue_head(&g_optic_wq);
	g_onu_ring.head = 0;
	g_onu_ring.tail = 0;

	ret = misc_register(&falcon_onu_miscdev);
	if (ret) {
		pr_err("falcon_onu0: failed to register /dev/onu0 (%d)\n", ret);
		return ret;
	}

	ret = misc_register(&falcon_optic_miscdev);
	if (ret) {
		pr_err("falcon_onu0: failed to register /dev/optic0 (%d)\n", ret);
		misc_deregister(&falcon_onu_miscdev);
		return ret;
	}

	pr_info("falcon_onu0: character devices /dev/onu0 and /dev/optic0 registered\n");
	return 0;
}

void falcon_onu_dev_exit(struct falcon_gpon_priv *priv)
{
	misc_deregister(&falcon_optic_miscdev);
	misc_deregister(&falcon_onu_miscdev);
	pr_info("falcon_onu0: character devices deregistered\n");
}
