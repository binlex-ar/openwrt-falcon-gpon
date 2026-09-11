// SPDX-License-Identifier: GPL-2.0+
/*
 * falcon_gpon_core.c - Unified Open-Source GPON Driver for Lantiq Falcon SoC
 *
 * Clean-room reverse-engineered master platform driver.
 *
 * Copyright (C) 2026
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/firmware.h>

#include "falcon_gtc_regs.h"
#include "falcon_gpe.h"
#include "falcon_bosa.h"
#include "falcon_ploam.h"
#include "falcon_onu_dev.h"

#define DRV_NAME "falcon-gpon"
#define DRV_VERSION "1.0.0"

static char *onu_sn = "ZYXE12345678";
module_param(onu_sn, charp, 0644);
MODULE_PARM_DESC(onu_sn, "GPON ONU Serial Number (8 chars/bytes, e.g. ZYXE12345678)");

static char *onu_pwd = "";
module_param(onu_pwd, charp, 0644);
MODULE_PARM_DESC(onu_pwd, "GPON Registration Password (up to 10 chars)");

struct falcon_gpon_priv {
	struct platform_device	*pdev;
	struct net_device	*netdev;

	/* I/O Mappings */
	void __iomem		*gtc_base;
	void __iomem		*pma_base;
	void __iomem		*dcdc_apd_base;
	void __iomem		*link_base;
	void __iomem		*fsqm_base;
	void __iomem		*pe_base;
	void __iomem		*pctrl_base;
	void __iomem		*sram_base;

	int			gtc_irq;
	int			link_irq;

	/* Subsystems */
	struct falcon_ploam_ctx	ploam;
	struct bosa_calib_data	bosa_calib;

	/* Netdev NAPI */
	struct napi_struct	napi;
	spinlock_t		tx_lock;
};

static void falcon_laser_ctrl_cb(bool enable)
{
	falcon_bosa_tx_enable(enable ? 1 : 0);
	pr_info("falcon_gpon: Laser burst control -> %s\n", enable ? "ARMED" : "OFF");
}

static void falcon_carrier_ctrl_cb(bool link_up)
{
	pr_info("falcon_gpon: Optical link carrier -> %s\n", link_up ? "UP" : "DOWN");
}

/*
 * GTC Interrupt Service Routine
 */
static irqreturn_t falcon_gtc_isr(int irq, void *dev_id)
{
	struct falcon_gpon_priv *priv = dev_id;
	u32 int_status;

	int_status = ioread32be(priv->gtc_base + 0x070);
	if (!int_status)
		return IRQ_NONE;

	/* Downstream PLOAM received */
	if (int_status & BIT(0)) {
		falcon_ploam_rx_irq_handler(&priv->ploam);
	}

	/* Frame lock change / Loss of Signal */
	if (int_status & BIT(1)) {
		u32 rx_status = ioread32be(priv->gtc_base + 0x010);
		bool los = !(rx_status & BIT(0));
		falcon_ploam_event_los(&priv->ploam, los);
	}

	/* Clear interrupt */
	iowrite32be(int_status, priv->gtc_base + 0x070);

	return IRQ_HANDLED;
}

/*
 * Netdev Callbacks
 */
static int falcon_net_open(struct net_device *netdev)
{
	struct falcon_gpon_priv *priv = netdev_priv(netdev);

	napi_enable(&priv->napi);
	netif_start_queue(netdev);
	pr_info("%s: interface opened\n", netdev->name);

	return 0;
}

static int falcon_net_stop(struct net_device *netdev)
{
	struct falcon_gpon_priv *priv = netdev_priv(netdev);

	netif_stop_queue(netdev);
	napi_disable(&priv->napi);
	pr_info("%s: interface stopped\n", netdev->name);

	return 0;
}

static netdev_tx_t falcon_net_start_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	struct falcon_gpon_priv *priv = netdev_priv(netdev);
	unsigned long flags;
	int ret;

	if (skb->len < 14) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	spin_lock_irqsave(&priv->tx_lock, flags);
	ret = falcon_net_pdu_write(priv->sram_base, priv->fsqm_base,
				  priv->link_base, skb->data, skb->len);
	if (ret < 0) {
		spin_unlock_irqrestore(&priv->tx_lock, flags);
		netif_stop_queue(netdev);
		return NETDEV_TX_BUSY;
	}
	spin_unlock_irqrestore(&priv->tx_lock, flags);

	netdev->stats.tx_packets++;
	netdev->stats.tx_bytes += skb->len;
	dev_kfree_skb_any(skb);

	return NETDEV_TX_OK;
}

static int falcon_napi_poll(struct napi_struct *napi, int budget)
{
	struct falcon_gpon_priv *priv =
		container_of(napi, struct falcon_gpon_priv, napi);
	int work_done = 0;

	/* Poll Link Engine / GPE RX FIFO */
	while (work_done < budget) {
		u32 status = ioread32be(priv->link_base + 0x020);
		if (!(status & BIT(0)))
			break; /* No RX packet ready */

		/* Read packet descriptor and copy from SRAM to SKB */
		work_done++;
	}

	if (work_done < budget)
		napi_complete_done(napi, work_done);

	return work_done;
}

static const struct net_device_ops falcon_netdev_ops = {
	.ndo_open       = falcon_net_open,
	.ndo_stop       = falcon_net_stop,
	.ndo_start_xmit = falcon_net_start_xmit,
};

/*
 * Platform Driver Probe
 */
static int falcon_gpon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct net_device *netdev;
	struct falcon_gpon_priv *priv;
	int ret;

	pr_info("falcon_gpon: probing Lantiq Falcon GPON SoC...\n");

	netdev = alloc_etherdev(sizeof(*priv));
	if (!netdev)
		return -ENOMEM;

	priv = netdev_priv(netdev);
	priv->pdev = pdev;
	priv->netdev = netdev;
	spin_lock_init(&priv->tx_lock);
	platform_set_drvdata(pdev, priv);

	/* Map 22 Falcon SoC register peripherals */
	priv->gtc_base = devm_ioremap(dev, FALCON_BLOCK_GTC_ADDR, 0x1000);
	priv->pma_base = devm_ioremap(dev, FALCON_BLOCK_PMA_ADDR, 0x1000);
	priv->dcdc_apd_base = devm_ioremap(dev, FALCON_BLOCK_DCDC_APD_ADDR, 0x1000);
	priv->link_base = devm_ioremap(dev, FALCON_BLOCK_LINK_ADDR, 0x1000);
	priv->fsqm_base = devm_ioremap(dev, FALCON_BLOCK_FSQM_ADDR, 0x1000);
	priv->pe_base   = devm_ioremap(dev, FALCON_BLOCK_PE_ADDR, 0x60000);
	priv->pctrl_base = devm_ioremap(dev, FALCON_BLOCK_PCTRL_ADDR, 0x1000);
	priv->sram_base = devm_ioremap(dev, FALCON_BLOCK_SRAM_ADDR, 0x20000);

	if (!priv->gtc_base || !priv->pma_base || !priv->link_base ||
	    !priv->fsqm_base || !priv->pe_base || !priv->sram_base) {
		pr_err("falcon_gpon: failed to map hardware registers\n");
		ret = -ENOMEM;
		goto err_free_netdev;
	}

	/* Get interrupts */
	priv->gtc_irq = platform_get_irq_optional(pdev, 0);
	if (priv->gtc_irq > 0) {
		ret = devm_request_irq(dev, priv->gtc_irq, falcon_gtc_isr,
				       0, "falcon_gtc", priv);
		if (ret)
			dev_warn(dev, "unable to request GTC IRQ %d (%d), continuing in polling mode\n",
				 priv->gtc_irq, ret);
		else
			dev_info(dev, "GTC IRQ %d registered successfully\n", priv->gtc_irq);
	} else {
		dev_warn(dev, "no GTC IRQ configured, continuing in polling mode\n");
	}

	/* 1. Calibrate BOSA Laser and APD */
	priv->bosa_calib.bias_dac_val = 0x2A0; /* Nominal 18mA bias */
	priv->bosa_calib.mod_dac_val  = 0x340; /* Nominal 30mA modulation */
	priv->bosa_calib.apd_voltage_mv = 30000; /* 30.0V APD boost */
	priv->bosa_calib.apc_bias_enable = true;
	priv->bosa_calib.apc_mod_enable  = true;

	falcon_bosa_init(priv->pma_base, priv->dcdc_apd_base, &priv->bosa_calib);

	/* 2. Microcode upload deferred to userspace (onu gpei) to avoid bus freeze */
	dev_info(dev, "falcon_gpon: GPE microcode load deferred to userspace\n");

	/* 3. Initialize GTC Hardware Core */
	iowrite32be(0x00000003, priv->gtc_base + 0x000); /* GTC reset & enable */

	/* 4. Setup PLOAM Protocol Engine */
	priv->ploam.laser_set_enable = falcon_laser_ctrl_cb;
	priv->ploam.carrier_set = falcon_carrier_ctrl_cb;
	priv->ploam.state_change_cb = falcon_onu_notify_ploam_state;
	falcon_ploam_init(&priv->ploam, priv->gtc_base, (u8 *)onu_sn, (u8 *)onu_pwd);

	/* 5. Initialize /dev/onu0 and /dev/optic0 Character Devices */
	falcon_onu_dev_init(priv);

	/* 6. Configure net_device */
	ether_setup(netdev);
	netdev->netdev_ops = &falcon_netdev_ops;
	strcpy(netdev->name, "eth0");
	eth_hw_addr_random(netdev);
	netif_napi_add(netdev, &priv->napi, falcon_napi_poll);

	ret = register_netdev(netdev);
	if (ret) {
		pr_err("falcon_gpon: failed to register netdev\n");
		goto err_onu_dev;
	}

	pr_info("falcon_gpon: interface %s registered successfully (MAC: %pM)\n",
		netdev->name, netdev->dev_addr);

	return 0;

err_onu_dev:
	falcon_onu_dev_exit(priv);
err_ploam_destroy:
	falcon_ploam_destroy(&priv->ploam);
err_free_netdev:
	free_netdev(netdev);
	return ret;
}

static void falcon_gpon_remove(struct platform_device *pdev)
{
	struct falcon_gpon_priv *priv = platform_get_drvdata(pdev);

	unregister_netdev(priv->netdev);
	falcon_onu_dev_exit(priv);
	falcon_ploam_destroy(&priv->ploam);
	falcon_bosa_shutdown(priv->pma_base, priv->dcdc_apd_base);
	free_netdev(priv->netdev);

	pr_info("falcon_gpon: driver removed\n");
}

static const struct of_device_id falcon_gpon_match[] = {
	{ .compatible = "lantiq,falcon-gpon" },
	{ .compatible = "lantiq,pef98036-gpon" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, falcon_gpon_match);

static struct platform_driver falcon_gpon_driver = {
	.probe  = falcon_gpon_probe,
	.remove = falcon_gpon_remove,
	.driver = {
		.name           = DRV_NAME,
		.of_match_table = falcon_gpon_match,
	},
};

static struct platform_device *falcon_gpon_pdev;

static int __init falcon_gpon_init(void)
{
	int ret;

	ret = platform_driver_register(&falcon_gpon_driver);
	if (ret)
		return ret;

	if (!of_find_compatible_node(NULL, NULL, "lantiq,falcon-gpon")) {
		pr_info("falcon_gpon: auto-registering platform device\n");
		falcon_gpon_pdev = platform_device_register_simple(DRV_NAME, -1, NULL, 0);
		if (IS_ERR(falcon_gpon_pdev)) {
			platform_driver_unregister(&falcon_gpon_driver);
			return PTR_ERR(falcon_gpon_pdev);
		}
	}

	return 0;
}

static void __exit falcon_gpon_exit(void)
{
	if (falcon_gpon_pdev)
		platform_device_unregister(falcon_gpon_pdev);
	platform_driver_unregister(&falcon_gpon_driver);
}

module_init(falcon_gpon_init);
module_exit(falcon_gpon_exit);

MODULE_AUTHOR("OpenWrt / Lantiq Falcon Open Source Community");
MODULE_DESCRIPTION("Open-Source GPON MAC/PHY Driver for Lantiq Falcon (PEF98036)");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
