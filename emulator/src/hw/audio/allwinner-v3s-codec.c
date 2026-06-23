/*
 * Allwinner V3s audio codec (DAC) — samples from DAC_TXDATA to QEMU audio.
 *
 * Relevant register offsets (from jupiter/include/v3s.h):
 *   +0x00 DAC_DPC      bit 31 = EN_DA, bits 17:12 = digital volume
 *   +0x04 DAC_FIFOC    bits 31:29 sample-rate code, bit 5 TX_24BIT,
 *                      bit 6 MONO, bits 25:24 FIFO_MODE,
 *                      bit 4 DRQ_EN, bit 0 FLUSH
 *   +0x08 DAC_FIFOS    read-only status (we return "room available")
 *   +0x20 DAC_TXDATA   sample FIFO — each write pushes one word
 *   +0x40 DAC_CNT      bytes transferred counter (informational)
 *   +0x60 DAC_DAP_CTL  signal-path enable
 *
 * Part of licheeEmu. SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "exec/address-spaces.h"
#include "audio/audio.h"
#include "hw/audio/allwinner-v3s-codec.h"
#include "hw/dma/allwinner-v3s-dma.h"

#define REG_DAC_DPC      0x00
#define REG_DAC_FIFOC    0x04
#define REG_DAC_FIFOS    0x08
#define REG_DAC_TXDATA   0x20
#define REG_DAC_CNT      0x40
#define REG_DAC_DAP_CTL  0x60

#define FIFOC_FS_SHIFT        29
#define FIFOC_FIFO_MODE_SHIFT 24
#define FIFOC_FIFO_MODE_MASK  (0x3u << 24)
#define FIFOC_TX_24BIT        (1u << 5)
#define FIFOC_MONO            (1u << 6)
#define FIFOC_DRQ_EN          (1u << 4)
#define FIFOC_FLUSH           (1u << 0)

/* DAC FIFO sample-rate code (FIFOC bits 31:29) → MCLK divider against
 * PLL_AUDIO output. Per the V3s codec datasheet §7.3, fed by PLL_AUDIO
 * via the AC_DIG_CLK module clock. The previous fixed `Hz` table was a
 * lie that ignored the guest's PLL_AUDIO reprogramming and made
 * unconventional rates (e.g. Jupiter's 11025 Hz path that runs PLL low
 * + FS=2) come out at the wrong pitch on real HW.
 *
 * Note: divisor 0 is invalid in the table position for the high-rate
 * codes (192000/96000 — codes 6 and 7) which use specially scaled
 * dividers; we pass them through as-is for completeness. */
static const uint32_t codec_divs[8] = {
    512, 768, 1024, 1536, 2048, 3072, 128, 256,
};

/* Read PLL_AUDIO_CTRL from the CCU and decode it to Hz. Field layout:
 *   bits 4:0   M_factor   (encoded value, divisor = M+1)
 *   bits 14:8  N_factor   (encoded value, multiplier = N+1)
 *   bits 17:16 P_factor   (encoded value, divisor = P+1)
 *   bit  31    PLL_EN
 *
 *   f_pll = 24 MHz * (N+1) / ((M+1) * (P+1))
 *
 * Validated against Jupiter's audio.c rate table:
 *   0x80035514 → 24.571 MHz   (label 48000)
 *   0x80034E14 → 22.571 MHz   (label 44100 / and the 11025 alias path)
 *   0x80037A15 → 33.545 MHz   (label 65536)
 *   0x80011815 → 13.636 MHz   (label 53267)
 */
static uint32_t pll_audio_hz(void)
{
    uint32_t reg = 0;
    address_space_read(&address_space_memory, 0x01C20008,
                       MEMTXATTRS_UNSPECIFIED, &reg, sizeof reg);
    if (!(reg & (1u << 31))) {
        return 0;  /* PLL disabled — fall back to the canonical 24.576 MHz
                     elsewhere if needed. */
    }
    uint32_t m = (reg >>  0) & 0x1F;
    uint32_t n = (reg >>  8) & 0x7F;
    uint32_t p = (reg >> 16) & 0x3;
    uint64_t hz = 24000000ULL * (n + 1) / ((m + 1) * (p + 1));
    return (uint32_t)hz;
}

static void codec_audio_callback(void *opaque, int avail);

static void codec_update_format(AwV3sCodecState *s)
{
    uint32_t fifoc = s->regs[REG_DAC_FIFOC / 4];
    uint32_t fs    = (fifoc >> FIFOC_FS_SHIFT) & 0x7;
    uint32_t mode  = (fifoc & FIFOC_FIFO_MODE_MASK) >> FIFOC_FIFO_MODE_SHIFT;

    /* Compute actual codec output rate from current PLL_AUDIO ÷ MCLK
     * divisor. If PLL is disabled (guest hasn't programmed it yet), fall
     * back to the canonical 24.576 MHz so the cold-boot path produces a
     * sensible 48 kHz default. */
    uint32_t pll = pll_audio_hz();
    if (pll == 0) {
        pll = 24576000;
    }
    uint32_t new_rate = pll / codec_divs[fs];

    /* Jupiter's audio pipeline is 16-bit mono regardless of what FIFOC
     * says about stereo — mix_buf is int16 per slot and each DMA word
     * is one mono sample. Force mono on the host side. */
    bool new_stereo   = false;
    (void)(fifoc & FIFOC_MONO);
    bool new_bits24   =  (fifoc & FIFOC_TX_24BIT) && (mode == 0);
    bool new_mode_01  =  (mode == 1);

    bool reopen = (new_rate != s->sample_rate_hz) ||
                  (new_stereo != s->stereo);

    s->sample_rate_hz = new_rate;
    s->stereo         = new_stereo;
    s->bits24         = new_bits24;
    s->fifo_mode_01   = new_mode_01;

    if (reopen) {
        s->as.freq = s->sample_rate_hz;
        s->as.nchannels = s->stereo ? 2 : 1;
        s->as.fmt = AUDIO_FORMAT_S16;
        s->as.endianness = 0;   /* host-endian */
        s->voice = AUD_open_out(&s->card, s->voice,
                                "allwinner-v3s-codec", s,
                                codec_audio_callback, &s->as);
        if (s->voice) {
            AUD_set_active_out(s->voice, 1);
        }

        if (s->dma_peer) {
            aw_v3s_dma_set_sample_rate(
                (AwV3sDmaState *)s->dma_peer, s->sample_rate_hz);
        }
    }
}

/* Decode one TXDATA word per the current format and enqueue int16 samples. */
static void codec_push_txdata(AwV3sCodecState *s, uint32_t word)
{
    int16_t samples[2];
    int count = 0;

    if (s->bits24) {
        /* 32-bit word holds a 24-bit sample in bits 31:8. Drop to 16 for
         * host audio. Stereo interleaves: left channel word, right word. */
        int16_t sample = (int16_t)(word >> 16);
        samples[count++] = sample;
        if (!s->stereo) {
            /* mono: duplicate on next frame; QEMU will replicate as needed */
        }
    } else if (s->fifo_mode_01) {
        /* 16-bit compact: low half of word is the sample. */
        samples[count++] = (int16_t)(word & 0xFFFF);
        if (s->stereo) {
            /* Upper half is the right-channel sample in some configs;
             * Jupiter runs MONO here so we typically don't see this. */
            samples[count++] = (int16_t)(word >> 16);
        }
    } else {
        samples[count++] = (int16_t)(word & 0xFFFF);
    }

    for (int i = 0; i < count; i++) {
        if (s->count >= AW_V3S_CODEC_RING_BYTES / 2) {
            /* ring full — drop oldest */
            s->rd_idx = (s->rd_idx + 1) % (AW_V3S_CODEC_RING_BYTES / 2);
            s->count--;
        }
        s->ring[s->wr_idx] = samples[i];
        s->wr_idx = (s->wr_idx + 1) % (AW_V3S_CODEC_RING_BYTES / 2);
        s->count++;
    }
}

/* QEMU audio callback: host wants samples. We write `avail` bytes worth. */
static void codec_audio_callback(void *opaque, int avail)
{
    AwV3sCodecState *s = opaque;
    const int sample_bytes = s->stereo ? 4 : 2;

    while (avail >= sample_bytes && s->count >= (s->stereo ? 2u : 1u)) {
        int16_t frame[2];
        frame[0] = s->ring[s->rd_idx];
        s->rd_idx = (s->rd_idx + 1) % (AW_V3S_CODEC_RING_BYTES / 2);
        s->count--;
        if (s->stereo) {
            frame[1] = s->ring[s->rd_idx];
            s->rd_idx = (s->rd_idx + 1) % (AW_V3S_CODEC_RING_BYTES / 2);
            s->count--;
        }
        int written = AUD_write(s->voice, frame, sample_bytes);
        if (written <= 0) {
            break;
        }
        avail -= written;
    }
    /* If ring underruns, host will pad with silence automatically. */
}

/* ---------- MMIO --------------------------------------------------- */

static uint64_t codec_read(void *opaque, hwaddr off, unsigned size)
{
    AwV3sCodecState *s = opaque;

    switch (off) {
    case REG_DAC_FIFOS:
        /* Report FIFO always has room; guest doesn't stall. Bit 23 = TXE. */
        return (1u << 23);
    case REG_DAC_CNT:
        return s->regs[REG_DAC_CNT / 4];
    }
    if (off + size > AW_V3S_CODEC_IOSIZE) return 0;
    return s->regs[off / 4];
}

static void codec_write(void *opaque, hwaddr off, uint64_t val, unsigned size)
{
    AwV3sCodecState *s = opaque;

    if (off + size > AW_V3S_CODEC_IOSIZE) return;

    switch (off) {
    case REG_DAC_FIFOC: {
        uint32_t new_val = (uint32_t)val;
        if (new_val & FIFOC_FLUSH) {
            s->rd_idx = s->wr_idx = s->count = 0;
        }
        s->regs[off / 4] = new_val & ~FIFOC_FLUSH;  /* FLUSH is self-clearing */
        codec_update_format(s);
        return;
    }
    case REG_DAC_TXDATA:
        codec_push_txdata(s, (uint32_t)val);
        s->regs[REG_DAC_CNT / 4] += size;
        return;
    }
    s->regs[off / 4] = (uint32_t)val;
}

static const MemoryRegionOps codec_ops = {
    .read  = codec_read,
    .write = codec_write,
    /* The DMA does 16-bit bursts into DAC_TXDATA. Accept 2-byte accesses
     * directly — forcing a 4-byte impl triggered a spurious second write
     * that doubled every sample on the stream (each sample appeared
     * twice in /tmp/codec-dump.raw after ~2s of audio). */
    .valid = { .min_access_size = 2, .max_access_size = 4,
               .unaligned = false },
    .impl  = { .min_access_size = 2, .max_access_size = 4 },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/* ---------- public --------------------------------------------- */

void aw_v3s_codec_attach_dma(AwV3sCodecState *s, void *dma)
{
    s->dma_peer = dma;
    if (dma) {
        aw_v3s_dma_set_sample_rate((AwV3sDmaState *)dma, s->sample_rate_hz);
    }
}

/* ---------- QOM ---------------------------------------------------- */

static void aw_v3s_codec_reset(DeviceState *dev)
{
    AwV3sCodecState *s = AW_V3S_CODEC(dev);
    memset(s->regs, 0, sizeof s->regs);
    s->sample_rate_hz = 48000;
    s->stereo = false;
    s->bits24 = false;
    s->fifo_mode_01 = false;
    s->wr_idx = s->rd_idx = s->count = 0;
}

static void aw_v3s_codec_realize(DeviceState *dev, Error **errp)
{
    AwV3sCodecState *s = AW_V3S_CODEC(dev);

    memory_region_init_io(&s->iomem, OBJECT(dev), &codec_ops, s,
                          "allwinner-v3s-codec", AW_V3S_CODEC_IOSIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);

    if (!AUD_register_card("allwinner-v3s-codec", &s->card, errp)) {
        return;
    }

    s->as.freq = 48000;
    s->as.nchannels = 1;
    s->as.fmt = AUDIO_FORMAT_S16;
    s->as.endianness = 0;
    s->voice = AUD_open_out(&s->card, s->voice,
                            "allwinner-v3s-codec", s,
                            codec_audio_callback, &s->as);
    if (s->voice) {
        AUD_set_active_out(s->voice, 1);
    }
}

static void aw_v3s_codec_unrealize(DeviceState *dev)
{
    AwV3sCodecState *s = AW_V3S_CODEC(dev);
    if (s->voice) {
        AUD_close_out(&s->card, s->voice);
        s->voice = NULL;
    }
    AUD_remove_card(&s->card);
}

static void aw_v3s_codec_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    dc->realize   = aw_v3s_codec_realize;
    dc->unrealize = aw_v3s_codec_unrealize;
    device_class_set_legacy_reset(dc, aw_v3s_codec_reset);
    dc->desc = "Allwinner V3s audio codec (DAC)";
}

static const TypeInfo aw_v3s_codec_type_info = {
    .name          = TYPE_AW_V3S_CODEC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AwV3sCodecState),
    .class_init    = aw_v3s_codec_class_init,
};

static void aw_v3s_codec_register_types(void)
{
    type_register_static(&aw_v3s_codec_type_info);
}

type_init(aw_v3s_codec_register_types)
