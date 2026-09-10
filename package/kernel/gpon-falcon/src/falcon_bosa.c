/*
 * Open-source BOSA Optical Transceiver Driver for Lantiq Falcon (PEF98036)
 * Clean-room reverse-engineered from mod_optic.ko
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
    struct falcon_bosa_tx_cfg cfg;
};

static struct falcon_bosa_dev g_bosa;

/*
 * Enables / disables laser transmission in burst mode
 */
int falcon_bosa_tx_enable(int enable)
{
    u32 val;
    unsigned long flags;

    spin_lock_irqsave(&g_bosa.lock, flags);

    if (enable) {
        /* Deassert TX_DISABLE GPIO pin (Active High or Low depending on sfp_pins) */
        if (gpio_is_valid(g_bosa.pin_tx_disable))
            gpio_set_value(g_bosa.pin_tx_disable, 0);

        /* Activate Tx Path Channel 0 in PMA */
        val = ioread32(g_bosa.pma + PMA_REG_TX_ACTIVATE_CH0);
        val &= ~0x08; /* Clear disable bit */
        val |= 0x05;  /* Enable Tx burst clock & data path */
        iowrite32(val, g_bosa.pma + PMA_REG_TX_ACTIVATE_CH0);

        pr_info("falcon_bosa: Laser burst transmitter ENABLED
");
    } else {
        /* Assert TX_DISABLE GPIO pin */
        if (gpio_is_valid(g_bosa.pin_tx_disable))
            gpio_set_value(g_bosa.pin_tx_disable, 1);

        /* Deactivate Tx Path Channel 0 in PMA */
        val = ioread32(g_bosa.pma + PMA_REG_TX_ACTIVATE_CH0);
        val |= 0x08;  /* Set disable bit */
        val |= 0x05;
        iowrite32(val, g_bosa.pma + PMA_REG_TX_ACTIVATE_CH0);

        pr_info("falcon_bosa: Laser transmitter DISABLED
");
    }

    spin_unlock_irqrestore(&g_bosa.lock, flags);
    return 0;
}
EXPORT_SYMBOL(falcon_bosa_tx_enable);

/*
 * Controls Automatic Power Control (APC) Dual-Loop circuits
 */
int falcon_bosa_set_apc_loop(int bias_loop_closed, int mod_loop_closed)
{
    u32 val;
    unsigned long flags;

    spin_lock_irqsave(&g_bosa.lock, flags);

    /* Configure Bias Current APC Loop */
    val = ioread32(g_bosa.pma + PMA_REG_BIAS_LOOP_CTRL) & ~0x03;
    if (bias_loop_closed)
        val |= 0x01; /* Closed loop tracking */
    iowrite32(val, g_bosa.pma + PMA_REG_BIAS_LOOP_CTRL);

    /* Configure Modulation Current APC Loop */
    val = ioread32(g_bosa.pma + PMA_REG_MOD_LOOP_CTRL) & ~0x03;
    if (mod_loop_closed)
        val |= 0x01; /* Closed loop tracking */
    iowrite32(val, g_bosa.pma + PMA_REG_MOD_LOOP_CTRL);

    spin_unlock_irqrestore(&g_bosa.lock, flags);
    return 0;
}
EXPORT_SYMBOL(falcon_bosa_set_apc_loop);

/*
 * Sets APD Avalanche Receiver High Voltage via on-chip step-up DC-DC converter
 */
int falcon_bosa_set_apd_voltage(u8 voltage_dac)
{
    unsigned long flags;

    spin_lock_irqsave(&g_bosa.lock, flags);

    /* Unlock DC-DC controller */
    iowrite32(0, g_bosa.dcdc_apd + DCDC_APD_REG_CONTROL);

    /* Write target high-voltage DAC value */
    iowrite32((u32)voltage_dac, g_bosa.dcdc_apd + DCDC_APD_REG_VOLTAGE_DAC);

    spin_unlock_irqrestore(&g_bosa.lock, flags);
    return 0;
}
EXPORT_SYMBOL(falcon_bosa_set_apd_voltage);

/*
 * Writes calibrated Bias and Modulation DAC values
 */
int falcon_bosa_set_bias_mod_current(u16 bias_dac, u16 mod_dac)
{
    unsigned long flags;

    spin_lock_irqsave(&g_bosa.lock, flags);

    /* Bias current is 11-bit DAC (0..2047) */
    iowrite32(bias_dac & 0x7FF, g_bosa.pma + PMA_REG_BIAS_DAC);

    /* Modulation current is 11-bit DAC (0..2047) */
    iowrite32(mod_dac & 0x7FF, g_bosa.pma + PMA_REG_MOD_DAC);

    spin_unlock_irqrestore(&g_bosa.lock, flags);
    return 0;
}
EXPORT_SYMBOL(falcon_bosa_set_bias_mod_current);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Clean-room Reverse Engineering Project");
MODULE_DESCRIPTION("Open-source BOSA Optical Transceiver Driver for Lantiq Falcon");
