// SPDX-License-Identifier: GPL-2.0+
/*
 * falcon_dma.c - Lantiq Falcon Packet Engine DMA & Segment Memory Driver
 *
 * Clean-room reverse-engineered implementation from mod_onu.ko.
 *
 * Copyright (C) 2026
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/spinlock.h>

#include "falcon_gpe.h"

static DEFINE_SPINLOCK(fsqm_lock);

/*
 * Load GPE microcode (falcon_gpe_fw.bin) into PE instruction memory
 */
int falcon_gpe_load_firmware(struct device *dev, void __iomem *pctrl,
                             void __iomem *pe_ram, const char *fw_name)
{
    const struct firmware *fw;
    const u32 *fw_words;
    int ret, core, i, word_count;
    u32 pe_offset;

    ret = request_firmware(&fw, fw_name ? fw_name : "sfu/falcon_gpe_fw.bin", dev);
    if (ret) {
        dev_err(dev, "falcon_gpe: failed to load firmware '%s' (err=%d)\n",
                fw_name, ret);
        return ret;
    }

    if (fw->size < 16) {
        release_firmware(fw);
        return -EINVAL;
    }

    fw_words = (const u32 *)(fw->data + 16);
    word_count = (fw->size - 16) / 4;

    dev_info(dev, "falcon_gpe: loading microcode (%d words) into Packet Engine...\n",
             word_count);

    /* Falcon supports up to 8 SCE processing threads/cores (core 0 and 1 active) */
    for (core = 0; core < 2; core++) {
        /* 1. Put core into reset and gate clock */
        iowrite32be(1 << core, pctrl + PCTRL_REG_RESET);
        udelay(10);

        /* 2. Upload 32-bit VLIW instructions into Core Instruction RAM */
        for (i = 0; i < word_count; i++) {
            pe_offset = (((core << 14) + i + 0x2000) << 2);
            iowrite32be(be32_to_cpu(fw_words[i]), pe_ram + pe_offset);
        }

        /* 3. Release core reset and enable execution */
        iowrite32be(0xFFFFFFFF, pctrl + PCTRL_REG_RUN);
        udelay(10);

        dev_info(dev, "falcon_gpe: core %d initialized and running\n", core);
    }

    release_firmware(fw);
    return 0;
}
EXPORT_SYMBOL_GPL(falcon_gpe_load_firmware);

/*
 * Allocates a 64-byte segment from Free Segment Queue Manager
 */
static inline u16 falcon_fsqm_alloc_segment(void __iomem *fsqm_base)
{
    u32 seg = ioread32be(fsqm_base + 0x00);
    return (u16)(seg & 0x7FFF);
}

/*
 * Returns segment back to Free Segment Queue Manager
 */
static inline void falcon_fsqm_free_segment(void __iomem *fsqm_base, u16 segment_id)
{
    iowrite32be(segment_id & 0x7FFF, fsqm_base + 0x04);
}

/*
 * Transmits an Ethernet packet to GPE via Shared Buffer SRAM and Link Engine
 */
int falcon_net_pdu_write(void __iomem *sram_base, void __iomem *fsqm_base,
                         void __iomem *link_base, const void *data, u32 len)
{
    unsigned long flags;
    u16 seg_id;
    u32 fifo_status;
    void __iomem *dest_buf;
    int timeout = 1000;
    struct falcon_dma_desc desc;

    if (unlikely(!data || len == 0))
        return -EINVAL;

    spin_lock_irqsave(&fsqm_lock, flags);

    /* 1. Allocate buffer segment from FSQM */
    seg_id = falcon_fsqm_alloc_segment(fsqm_base);
    if (seg_id == FALCON_FSQM_NULL_SEGMENT) {
        spin_unlock_irqrestore(&fsqm_lock, flags);
        return -ENOMEM;
    }

    /* 2. Copy packet payload directly into on-chip SBS SRAM buffer */
    dest_buf = sram_base + (seg_id << 6);
    memcpy_toio(dest_buf, data, len);

    /* 3. Wait for free slot in Link Engine FIFO */
    do {
        fifo_status = ioread32be(link_base + LINK_REG_FIFO_STATUS);
        if ((fifo_status & 0x1F) > 0)
            break;
        udelay(1);
    } while (--timeout > 0);

    if (timeout == 0) {
        falcon_fsqm_free_segment(fsqm_base, seg_id);
        spin_unlock_irqrestore(&fsqm_lock, flags);
        pr_warn_ratelimited("falcon_gpe: Link FIFO full timeout\n");
        return -EBUSY;
    }

    /* 4. Prepare hardware DMA descriptor */
    desc.word0 = (seg_id << 16) | (len & 0xFFFF);
    desc.word1 = (0x00 << 24) | 0x00010000; /* Source port 0, Tx flags */
    desc.word2 = (FALCON_FSQM_NULL_SEGMENT << 16); /* Single segment */
    desc.word3 = 0; /* GEM / Classification tag */

    /* 5. Push descriptor into Link Engine FIFO */
    iowrite32be(desc.word0, link_base + LINK_REG_DATA_LOW);
    iowrite32be(desc.word1, link_base + LINK_REG_DATA_HIGH);
    iowrite32be(desc.word2, link_base + LINK_REG_DATA_LOW);
    iowrite32be(desc.word3, link_base + LINK_REG_DATA_HIGH);

    /* 6. Trigger transmit */
    iowrite32be(0x01, link_base + LINK_REG_CMD);

    spin_unlock_irqrestore(&fsqm_lock, flags);
    return 0;
}
EXPORT_SYMBOL_GPL(falcon_net_pdu_write);

