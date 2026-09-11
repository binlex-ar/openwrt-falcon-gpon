/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef __FALCON_BOSA_H__
#define __FALCON_BOSA_H__

#include <linux/types.h>

/* Hardware MMIO Base Addresses */
#define FALCON_PHYS_PMA             0x1E803000
#define FALCON_PHYS_DCDC_CORE       0x1E805000
#define FALCON_PHYS_DCDC_APD        0x1DD00600
#define FALCON_PHYS_GTC_PMA         0x1DD00740
#define FALCON_PHYS_PMA_INTRX       0x1DD00720
#define FALCON_PHYS_PMA_INTTX       0x1DF00000
#define FALCON_PHYS_FCSIC           0x1DEFFF00

/* PMA Hardware Registers (Base: FALCON_PHYS_PMA) */
#define PMA_REG_RX_LOL_THRESH       0x00D0
#define PMA_REG_MM_INIT             0x0100
#define PMA_REG_MOD_DAC             0x0190
#define PMA_REG_BIAS_DAC            0x0194
#define PMA_REG_BIAS_LOOP_CTRL      0x01A0
#define PMA_REG_MOD_LOOP_CTRL       0x01A4
#define PMA_REG_CALIB_CTRL          0x01B8
#define PMA_REG_CALIB_STATUS        0x01BC
#define PMA_REG_POWERSAVE_TX        0x01CC
#define PMA_REG_POWERSAVE_RX        0x01D0
#define PMA_REG_BIAS_LOW_SAT        0x01F0
#define PMA_REG_TX_ACTIVATE_CH0     0x0200
#define PMA_REG_TX_ACTIVATE_CH1     0x0204
#define PMA_REG_TX_LATCH_OVERRIDE   0x0208
#define PMA_REG_TX_FIFO             0x020C
#define PMA_REG_TX_LASER_DELAY      0x0210
#define PMA_REG_TX_PHASE_INTRP      0x0214
#define PMA_REG_PLL_VCO             0x028C

/* APD High-Voltage DC-DC Converter Registers */
#define DCDC_APD_REG_VOLTAGE_DAC    0x002C
#define DCDC_APD_REG_CONTROL        0x0054

struct bosa_calib_data {
    u16 bias_dac_val;
    u16 mod_dac_val;
    u32 apd_voltage_mv;
    bool apc_bias_enable;
    bool apc_mod_enable;
};

int falcon_bosa_init(void __iomem *pma, void __iomem *dcdc_apd,
                     const struct bosa_calib_data *calib);
void falcon_bosa_shutdown(void __iomem *pma, void __iomem *dcdc_apd);
int falcon_bosa_tx_enable(int enable);
int falcon_bosa_set_apc_loop(int bias_loop_closed, int mod_loop_closed);
int falcon_bosa_set_apd_voltage(u8 voltage_dac);
int falcon_bosa_set_bias_mod_current(u16 bias_dac, u16 mod_dac);

#endif /* __FALCON_BOSA_H__ */
