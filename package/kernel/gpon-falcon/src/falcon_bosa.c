// SPDX-License-Identifier: GPL-2.0+
/*
 * falcon_bosa.c - Open-source BOSA Optical Transceiver Driver for Lantiq Falcon
 *
 * Clean-room reverse-engineered from mod_optic.ko
 *
 * Copyright (C) 2026
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include "falcon_bosa.h"

struct falcon_bosa_dev {
    void __iomem *pma;
    void __iomem *dcdc_core;
    void __iomem *dcdc_apd;
    int pin_tx_disable;
    int pin_tx_fault;
    spinlock_t lock;
};

static struct falcon_bosa_dev g_bosa;

int falcon_bosa_tx_enable(int enable)
{
    u32 val;
    unsigned long flags;

    spin_lock_irqsave(&g_bosa.lock, flags);

    if (enable) {
        if (gpio_is_valid(g_bosa.pin_tx_disable))
            gpio_set_value(g_bosa.pin_tx_disable, 0);

        val = ioread32be(g_bosa.pma + PMA_REG_TX_ACTIVATE_CH0);
        val &= ~0x08;
        val |= 0x05;
        iowrite32be(val, g_bosa.pma + PMA_REG_TX_ACTIVATE_CH0);

        pr_info("falcon_bosa: Laser burst transmitter ENABLED\n");
    } else {
        if (gpio_is_valid(g_bosa.pin_tx_disable))
            gpio_set_value(g_bosa.pin_tx_disable, 1);

        val = ioread32be(g_bosa.pma + PMA_REG_TX_ACTIVATE_CH0);
        val |= 0x08;
        val |= 0x05;
        iowrite32be(val, g_bosa.pma + PMA_REG_TX_ACTIVATE_CH0);

        pr_info("falcon_bosa: Laser transmitter DISABLED\n");
    }

    spin_unlock_irqrestore(&g_bosa.lock, flags);
    return 0;
}
EXPORT_SYMBOL_GPL(falcon_bosa_tx_enable);

int falcon_bosa_set_apc_loop(int bias_loop_closed, int mod_loop_closed)
{
    u32 val;
    unsigned long flags;

    spin_lock_irqsave(&g_bosa.lock, flags);

    val = ioread32be(g_bosa.pma + PMA_REG_BIAS_LOOP_CTRL) & ~0x03;
    if (bias_loop_closed)
        val |= 0x01;
    iowrite32be(val, g_bosa.pma + PMA_REG_BIAS_LOOP_CTRL);

    val = ioread32be(g_bosa.pma + PMA_REG_MOD_LOOP_CTRL) & ~0x03;
    if (mod_loop_closed)
        val |= 0x01;
    iowrite32be(val, g_bosa.pma + PMA_REG_MOD_LOOP_CTRL);

    spin_unlock_irqrestore(&g_bosa.lock, flags);
    return 0;
}
EXPORT_SYMBOL_GPL(falcon_bosa_set_apc_loop);

int falcon_bosa_set_apd_voltage(u8 voltage_dac)
{
    unsigned long flags;

    spin_lock_irqsave(&g_bosa.lock, flags);
    iowrite32be(0, g_bosa.dcdc_apd + DCDC_APD_REG_CONTROL);
    iowrite32be((u32)voltage_dac, g_bosa.dcdc_apd + DCDC_APD_REG_VOLTAGE_DAC);
    spin_unlock_irqrestore(&g_bosa.lock, flags);
    return 0;
}
EXPORT_SYMBOL_GPL(falcon_bosa_set_apd_voltage);

int falcon_bosa_set_bias_mod_current(u16 bias_dac, u16 mod_dac)
{
    unsigned long flags;

    spin_lock_irqsave(&g_bosa.lock, flags);
    iowrite32be(bias_dac & 0x7FF, g_bosa.pma + PMA_REG_BIAS_DAC);
    iowrite32be(mod_dac & 0x7FF, g_bosa.pma + PMA_REG_MOD_DAC);
    spin_unlock_irqrestore(&g_bosa.lock, flags);
    return 0;
}
EXPORT_SYMBOL_GPL(falcon_bosa_set_bias_mod_current);

int falcon_bosa_init(void __iomem *pma, void __iomem *dcdc_apd,
                     const struct bosa_calib_data *calib)
{
    g_bosa.pma = pma;
    g_bosa.dcdc_apd = dcdc_apd;
    g_bosa.pin_tx_disable = -1;
    g_bosa.pin_tx_fault = -1;
    spin_lock_init(&g_bosa.lock);

    if (calib) {
        falcon_bosa_set_bias_mod_current(calib->bias_dac_val, calib->mod_dac_val);
        falcon_bosa_set_apc_loop(calib->apc_bias_enable, calib->apc_mod_enable);
        falcon_bosa_set_apd_voltage(0x96);
    }
    return 0;
}
EXPORT_SYMBOL_GPL(falcon_bosa_init);

void falcon_bosa_shutdown(void __iomem *pma, void __iomem *dcdc_apd)
{
    falcon_bosa_tx_enable(0);
    falcon_bosa_set_apc_loop(0, 0);
}
EXPORT_SYMBOL_GPL(falcon_bosa_shutdown);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Open-source BOSA Optical Transceiver Driver for Lantiq Falcon");
