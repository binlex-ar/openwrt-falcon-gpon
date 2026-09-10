#ifndef __FALCON_BOSA_H__
#define __FALCON_BOSA_H__

#include <linux/types.h>

/*
 * Lantiq Falcon (PEF98036) BOSA Optical Transceiver & Laser Calibration
 * Clean-room reverse-engineered from mod_optic.ko
 */

/* Hardware MMIO Base Addresses */
#define FALCON_PHYS_PMA             0x1E803000  /* Physical Medium Attachment / Analog Front-End */
#define FALCON_PHYS_DCDC_CORE       0x1E805000  /* Core DC-DC Converter */
#define FALCON_PHYS_DCDC_APD        0x1DD00600  /* APD Avalanche Receiver High-Voltage Boost */
#define FALCON_PHYS_GTC_PMA         0x1DD00740  /* GTC to PMA Interface */
#define FALCON_PHYS_PMA_INTRX       0x1DD00720  /* PMA Rx Interrupt Controller */
#define FALCON_PHYS_PMA_INTTX       0x1DF00000  /* PMA Tx Interrupt Controller */
#define FALCON_PHYS_FCSIC           0x1DEFFF00  /* FCSI Controller */

/* PMA Hardware Registers (Base: FALCON_PHYS_PMA) */
#define PMA_REG_RX_LOL_THRESH       0x00D0      /* Loss of Light (LOL) Alarm Threshold */
#define PMA_REG_MM_INIT             0x0100      /* Measurement Unit Init */
#define PMA_REG_MOD_DAC             0x0190      /* 11-Bit Laser Modulation Current DAC */
#define PMA_REG_BIAS_DAC            0x0194      /* 11-Bit Laser Bias Current DAC */
#define PMA_REG_BIAS_LOOP_CTRL      0x01A0      /* Bias Current Dual-Loop APC (Bit 0 = APC Enable) */
#define PMA_REG_MOD_LOOP_CTRL       0x01A4      /* Modulation Current Dual-Loop APC (Bit 0 = APC Enable) */
#define PMA_REG_CALIB_CTRL          0x01B8      /* Laser Calibration Mode Register */
#define PMA_REG_CALIB_STATUS        0x01BC      /* Calibration Status */
#define PMA_REG_POWERSAVE_TX        0x01CC      /* Tx Power-Save Mode */
#define PMA_REG_POWERSAVE_RX        0x01D0      /* Rx Power-Save Mode */
#define PMA_REG_BIAS_LOW_SAT        0x01F0      /* Bias Current Lower Saturation Bound */
#define PMA_REG_TX_ACTIVATE_CH0     0x0200      /* Tx Path Activate / Invert Channel 0 */
#define PMA_REG_TX_ACTIVATE_CH1     0x0204      /* Tx Path Activate / Invert Channel 1 */
#define PMA_REG_TX_LATCH_OVERRIDE   0x0208      /* Tx Power-Save & Latch Override */
#define PMA_REG_TX_FIFO             0x020C      /* Tx Burst FIFO Control */
#define PMA_REG_TX_LASER_DELAY      0x0210      /* Tx Laser Fire Delay (Nanoseconds) */
#define PMA_REG_TX_PHASE_INTRP      0x0214      /* Tx Phase Interpolator (PI) */
#define PMA_REG_PLL_VCO             0x028C      /* PLL VCO Tuning */

/* APD High-Voltage DC-DC Converter Registers (Base: FALCON_PHYS_DCDC_APD) */
#define DCDC_APD_REG_VOLTAGE_DAC    0x002C      /* Target APD Bias Voltage DAC */
#define DCDC_APD_REG_CONTROL        0x0054      /* APD Step-Up Converter Enable / Lock */

/* BOSA Calibration Configuration Structure (passed from userspace/EEPROM) */
struct falcon_bosa_tx_cfg {
    u32 magic;              /* Magic identifier */
    u8  bias_dac_min;       /* Minimum Bias Current limit */
    u8  bias_dac_max;       /* Maximum Bias Current limit */
    u8  mod_dac_min;        /* Minimum Modulation Current limit */
    u8  mod_dac_max;        /* Maximum Modulation Current limit */
    u8  target_p0;          /* Optical Power P0 setpoint (dBm / DAC) */
    u8  target_p1;          /* Optical Power P1 setpoint (dBm / DAC) */
    u8  reserved[2];
    u32 slope_efficiency;   /* Laser slope efficiency coefficient */
    u16 temp_points[3];     /* Temperature calibration points: T0, T1, T2 */
    u16 bias_points[3];     /* Calibrated Bias DAC at T0, T1, T2 */
    u16 mod_points[3];      /* Calibrated Mod DAC at T0, T1, T2 */
} __packed;

/* DDM (Digital Diagnostic Monitoring) Telemetry Structure */
struct falcon_optic_ddm {
    s16 temperature;        /* Temperature in 1/256 deg C */
    u16 vcc_voltage;        /* Supply voltage in 100 uV */
    u16 tx_bias_current;    /* Laser bias current in 2 uA */
    u16 tx_power;           /* Tx Optical power in 0.1 uW */
    u16 rx_power;           /* Rx Optical power in 0.1 uW */
};

#endif /* __FALCON_BOSA_H__ */
