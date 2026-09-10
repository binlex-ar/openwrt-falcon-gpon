/*
 * Open-source Linux net_device driver for Lantiq Falcon GPON SoC (PEF98036)
 * Clean-room reverse-engineered from mod_onu_netdev.ko
 * Compatible with modern Linux kernels (5.15 / 6.6) and OpenWrt 23.05 / 24.x
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/phy.h>
#include <linux/platform_device.h>

#define DRV_NAME        "falcon_gpon_netdev"
#define DRV_VERSION     "1.0.0-cleanroom"
#define MIN_PKT_LEN     64

extern int  net_pdu_write(u32 port, u32 len, void *data);
extern int  net_rx_enable(u32 port, int enable);
extern void net_cb_list_register(u32 port, void *cb_list);
extern void net_lan_promisc_mode_enable(u32 port, int enable);
extern int  net_is_port_up(u32 port);
extern int  net_mac_set(u32 port, const u8 *mac_addr);

struct falcon_netdev_priv {
    struct net_device *netdev;
    struct phy_device *phydev;
    u32 port_id;
    struct net_device_stats stats;
    spinlock_t lock;
};

static netdev_tx_t falcon_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct falcon_netdev_priv *priv = netdev_priv(dev);

    if (skb->len < MIN_PKT_LEN) {
        if (skb_padto(skb, MIN_PKT_LEN)) {
            priv->stats.tx_dropped++;
            return NETDEV_TX_OK;
        }
    }

    if (net_pdu_write(priv->port_id, skb->len, skb->data) != 0) {
        priv->stats.tx_dropped++;
    } else {
        priv->stats.tx_packets++;
        priv->stats.tx_bytes += skb->len;
        dev->trans_start = jiffies;
    }

    dev_kfree_skb_any(skb);
    return NETDEV_TX_OK;
}

void falcon_rx_packet_callback(struct net_device *dev, void *pkt_buf, u32 len)
{
    struct falcon_netdev_priv *priv;
    struct sk_buff *skb;

    if (unlikely(!dev || !pkt_buf || len == 0))
        return;

    priv = netdev_priv(dev);

    skb = netdev_alloc_skb(dev, len + 2);
    if (unlikely(!skb)) {
        priv->stats.rx_dropped++;
        return;
    }

    skb_reserve(skb, 2);
    skb_put_data(skb, pkt_buf, len);
    skb->protocol = eth_type_trans(skb, dev);

    priv->stats.rx_packets++;
    priv->stats.rx_bytes += len;

    netif_rx(skb);
}

static int falcon_open(struct net_device *dev)
{
    struct falcon_netdev_priv *priv = netdev_priv(dev);

    net_rx_enable(priv->port_id, 1);
    netif_carrier_on(dev);
    netif_start_queue(dev);

    pr_info("%s: GPON interface UP (Port %d)
", dev->name, priv->port_id);
    return 0;
}

static int falcon_stop(struct net_device *dev)
{
    struct falcon_netdev_priv *priv = netdev_priv(dev);

    netif_stop_queue(dev);
    netif_carrier_off(dev);
    net_rx_enable(priv->port_id, 0);

    pr_info("%s: GPON interface DOWN
", dev->name);
    return 0;
}

static const struct net_device_ops falcon_netdev_ops = {
    .ndo_open            = falcon_open,
    .ndo_stop            = falcon_stop,
    .ndo_start_xmit      = falcon_start_xmit,
    .ndo_set_mac_address = eth_mac_addr,
    .ndo_validate_addr   = eth_validate_addr,
};

static int falcon_netdev_probe(struct platform_device *pdev)
{
    struct net_device *netdev;
    struct falcon_netdev_priv *priv;
    int ret;

    netdev = alloc_etherdev(sizeof(struct falcon_netdev_priv));
    if (!netdev)
        return -ENOMEM;

    SET_NETDEV_DEV(netdev, &pdev->dev);
    priv = netdev_priv(netdev);
    priv->netdev = netdev;
    priv->port_id = 0;
    spin_lock_init(&priv->lock);

    netdev->netdev_ops = &falcon_netdev_ops;
    eth_hw_addr_random(netdev);

    ret = register_netdev(netdev);
    if (ret) {
        free_netdev(netdev);
        return ret;
    }

    platform_set_drvdata(pdev, netdev);
    pr_info("falcon_netdev: registered %s for Lantiq Falcon GPON
", netdev->name);
    return 0;
}

static int falcon_netdev_remove(struct platform_device *pdev)
{
    struct net_device *netdev = platform_get_drvdata(pdev);

    if (netdev) {
        unregister_netdev(netdev);
        free_netdev(netdev);
    }
    return 0;
}

static struct platform_driver falcon_netdev_driver = {
    .probe  = falcon_netdev_probe,
    .remove = falcon_netdev_remove,
    .driver = {
        .name  = DRV_NAME,
        .owner = THIS_MODULE,
    },
};

module_platform_driver(falcon_netdev_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Clean-room Reverse Engineering Project");
MODULE_DESCRIPTION("Open-source Network Adapter for Lantiq Falcon GPON");
