/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef __FALCON_GPE_H__
#define __FALCON_GPE_H__

#include <linux/types.h>
#include <linux/device.h>

/*
 * Lantiq Falcon (PEF98036) Packet Engine & DMA Subsystem Architecture
 * Clean-room reverse-engineered from mod_onu.ko and firmware microcode.
 */

/* Physical Memory Base Addresses */
#define FALCON_PHYS_GTC             0x1DC00000  /* GPON Transmission Convergence */
#define FALCON_PHYS_GPEARB          0x1D400100  /* GPE Arbiter */
#define FALCON_PHYS_TMU             0x1D404000  /* Traffic Management Unit / QoS */
#define FALCON_PHYS_IQM             0x1D410000  /* Ingress Queue Manager (DMA Ingress) */
#define FALCON_PHYS_OCTRLG          0x1D420000  /* Egress Controller Global */
#define FALCON_PHYS_OCTRLL          0x1D440000  /* Egress Controller Local */
#define FALCON_PHYS_OCTRLC          0x1D441000  /* Egress Controller Common */
#define FALCON_PHYS_ICTRLG          0x1D450000  /* Ingress Controller Global */
#define FALCON_PHYS_ICTRLL          0x1D460000  /* Ingress Controller Local */
#define FALCON_PHYS_ICTRLC          0x1D461000  /* Ingress Controller Common */
#define FALCON_PHYS_FSQM            0x1D500000  /* Free Segment Queue Manager (Buffer Pool) */
#define FALCON_PHYS_PCTRL           0x1D600000  /* Packet Engine Master Control */
#define FALCON_PHYS_LINK            0x1D600200  /* Link Engine / Mailbox & COP Interface */
#define FALCON_PHYS_DISP            0x1D600500  /* Packet Dispatcher */
#define FALCON_PHYS_MERGE           0x1D600600  /* Packet Merger */
#define FALCON_PHYS_TBM             0x1D600700  /* Token Bucket Meter / Policer */
#define FALCON_PHYS_PE_RAM          0x1D610000  /* Packet Engine Microcode RAM (384 KB) */
#define FALCON_PHYS_EIM             0x1D800000  /* Ethernet Interface Module (MAC) */
#define FALCON_PHYS_SXGMII          0x1D808800  /* 10G/SGMII SerDes */
#define FALCON_PHYS_SGMII           0x1D808C00  /* 1G SGMII SerDes */
#define FALCON_PHYS_SBS0CTRL        0x1F080000  /* Shared Buffer System 0 Controller */
#define FALCON_PHYS_SRAM_BUFFER     0x1F200000  /* Shared Buffer SRAM (Packet Payloads) */

/* Compatibility aliases for driver core */
#define FALCON_BLOCK_GTC_ADDR       FALCON_PHYS_GTC
#define FALCON_BLOCK_PMA_ADDR       0x1E803000
#define FALCON_BLOCK_DCDC_APD_ADDR  0x1DD00600
#define FALCON_BLOCK_LINK_ADDR      FALCON_PHYS_LINK
#define FALCON_BLOCK_FSQM_ADDR      FALCON_PHYS_FSQM
#define FALCON_BLOCK_PE_ADDR        FALCON_PHYS_PE_RAM
#define FALCON_BLOCK_PCTRL_ADDR     FALCON_PHYS_PCTRL
#define FALCON_BLOCK_SRAM_ADDR      FALCON_PHYS_SRAM_BUFFER

/* Packet Buffer Memory Configuration */
#define FALCON_SRAM_SEGMENT_SIZE    64          /* 64 bytes per FSQM segment */
#define FALCON_FSQM_NULL_SEGMENT    0x7FFF      /* End-of-packet segment marker */

/* Link Engine Registers (Base: FALCON_PHYS_LINK) */
#define LINK_REG_CMD                0x0000      /* Command / Trigger Register */
#define LINK_REG_CFG                0x0018      /* Link Configuration */
#define LINK_REG_FIFO_STATUS        0x0020      /* Status (Bits [4:0] = free FIFO slots) */
#define LINK_REG_DATA_LOW           0x0024      /* Descriptor Data Low Word */
#define LINK_REG_DATA_HIGH          0x0028      /* Descriptor Data High Word */
#define LINK_REG_CMD_SEC            0x0100      /* Secondary Command */
#define LINK_REG_CFG_SEC            0x0118      /* Secondary Configuration */
#define LINK_REG_COP_TRIGGER        0x0200      /* Co-Processor Trigger (0x11 = execute) */
#define LINK_REG_COP_STATUS         0x0220      /* Co-Processor Status Register */
#define LINK_REG_COP_DATA0          0x0224      /* Co-Processor Argument Word 0 */
#define LINK_REG_COP_DATA1          0x0228      /* Co-Processor Argument Word 1 */

/* Packet Controller Registers (Base: FALCON_PHYS_PCTRL) */
#define PCTRL_REG_RESET             0x0030      /* Core Reset Assert/Deassert */
#define PCTRL_REG_RUN               0x0040      /* Core Execution Run / Enable */
#define PCTRL_REG_STATUS            0x0050      /* Core Running Status */

/* 16-Byte Hardware DMA Descriptor Format */
struct falcon_dma_desc {
    u32 word0;  /* [31:16]: Segment ID, [15:0]: Total Packet Length */
    u32 word1;  /* [31:24]: Source Port, [23:16]: Dest Port, Flags */
    u32 word2;  /* [31:16]: Next Segment (if multi-seg), [15:0]: Offset */
    u32 word3;  /* [31:0]: VLAN / GEM Port Tag / Classification ID */
} __packed;

/* Public API for GPE & DMA */
int falcon_gpe_load_firmware(struct device *dev, void __iomem *pctrl,
                             void __iomem *pe_ram, const char *fw_name);
int falcon_net_pdu_write(void __iomem *sram_base, void __iomem *fsqm_base,
                         void __iomem *link_base, const void *data, u32 len);

#endif /* __FALCON_GPE_H__ */
