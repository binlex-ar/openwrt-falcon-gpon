/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef __FALCON_GTC_REGS_H__
#define __FALCON_GTC_REGS_H__

#include <linux/types.h>

#define FALCON_GTC_BASE_PHYS      0x1DC00000
#define FALCON_GTC_MEM_SIZE       0x1000

/* Register Offsets */
#define GTC_STATUS_ISR            0x0000
#define GTC_AES_KEY_BASE          0x0040
#define GTC_KEY_SWITCH_TIME       0x0050
#define GTC_PORT_ID_CFG           0x0054
#define GTC_DS_PLOAM_DATA0        0x0060
#define GTC_DS_PLOAM_DATA1        0x0064
#define GTC_DS_PLOAM_DATA2        0x0068
#define GTC_ONU_ID                0x006C
#define GTC_DS_ISTAT              0x0070
#define GTC_DS_IMASK              0x0074
#define GTC_BIP_VALUE             0x007C
#define GTC_GEM_RX_BCNT           0x0080
#define GTC_GEM_RX_FCNT           0x0090
#define GTC_GEM_FUERRCNT          0x009C
#define GTC_US_STATUS             0x0100
#define GTC_RANGED_DELAY          0x0104
#define GTC_US_ISTAT              0x010C
#define GTC_TX_CTRL               0x0114
#define GTC_US_HEADER_CFG         0x0118
#define GTC_US_PLOAM_CTRL         0x0128
#define GTC_US_PLOAM_DATA0        0x012C
#define GTC_US_PLOAM_DATA1        0x0130
#define GTC_US_PLOAM_DATA2        0x0134
#define GTC_US_PLOAM_STAT         0x0138
#define GTC_DELAY_OFFSET          0x014C
#define GTC_BWMAP_CFG             0x02DC
#define GTC_ALLOC_ID_TBL          0x02E0

/* Bitfield Definitions */
#define GTC_STATUS_PLOAM_AVAIL    (1 << 0)
#define GTC_STATUS_PLOAM_OVFL     (1 << 2)

#define GTC_TX_CTRL_ENABLE        (1 << 0)
#define GTC_TX_CTRL_REQ_ONLY      (1 << 1)

#define GTC_US_STAT_TX_BUSY       (1 << 0)

#endif /* __FALCON_GTC_REGS_H__ */
