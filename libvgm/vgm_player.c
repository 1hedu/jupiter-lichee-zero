/*
 * vgm_player.c — Lightweight VGM Player for Jupiter SDK
 *
 * Ported from libvgm (ValleyBell) C++ to bare-metal C.
 * Command table and dispatch logic derived from vgmplayer_cmdhandler.cpp.
 *
 * Chip cores used (all from libvgm, unmodified):
 *   YM2612:  Nuked-OPN2 (Nuke.YKT)         — ym3438.c
 *   SN76489: Maxim's emulator               — sn76489.c
 *   GB DMG:  MAME GB sound                  — gb.c
 *   NES APU: NSFPlay port (ValleyBell)       — np_nes_apu.c + np_nes_dmc.c
 */

#include "vgm_player.h"
vgm_reg_log_t vgm_nes_log;
vgm_reg_log_t vgm_gb_log;
vgm_reg_log_t vgm_ym_log;
#include <string.h>  /* memset */

/* ---- chip core headers ---- */
/* These functions are exposed by removing 'static' or via devDef.
 * For now we declare the ones we call directly. */

/* YM2612: Gens core (fast, table-driven) */
extern void *YM2612_Init(uint32_t clock, uint32_t rate, uint8_t interpolation);
extern void  YM2612_End(void *chip);
extern void  YM2612_Reset(void *chip);
extern void  YM2612_Write(void *chip, uint8_t adr, uint8_t data);
extern void  YM2612_Update(void *chip, int32_t **buf, uint32_t length);
/* Thin wrappers to keep call sites unchanged */
static void *gens_ym2612_init(uint32_t clock, uint32_t rate) { return YM2612_Init(clock, rate, 0); }
static void  gens_ym2612_update(void *chip, uint32_t num, int32_t **buf) { YM2612_Update(chip, buf, num); }

/* SN76489 — need to un-static these in sn76489.c */

/* GB DMG — need to un-static these in gb.c */

/* SN76489 PSG (Jupiter wrappers in sn76489.c) */
extern void *sn76489_jupiter_init(uint32_t clock, uint32_t rate);
extern void  sn76489_jupiter_shutdown(void *chip);
extern void  sn76489_jupiter_reset(void *chip);
extern void  sn76489_jupiter_write(void *chip, uint8_t data);
extern void  sn76489_jupiter_update(void *chip, uint32_t len, int32_t *left, int32_t *right);

/* SN76489 (Jupiter wrappers in sn76489.c) */
extern void *sn76489_jupiter_init(uint32_t clock, uint32_t rate);
extern void  sn76489_jupiter_shutdown(void *chip);
extern void  sn76489_jupiter_reset(void *chip);
extern void  sn76489_jupiter_write(void *chip, uint8_t data);
extern void  sn76489_jupiter_update(void *chip, uint32_t len, int32_t *left, int32_t *right);

/* GB DMG (Jupiter wrappers in gb.c) */
extern void *gb_jupiter_init(uint32_t clock, uint32_t rate);
extern void  gb_jupiter_shutdown(void *chip);
extern void  gb_jupiter_reset(void *chip);
extern void  gb_jupiter_write(void *chip, uint8_t reg, uint8_t val);
extern void  gb_jupiter_wave_write(void *chip, uint8_t offset, uint8_t val);
extern void  gb_jupiter_update(void *chip, uint32_t samples, int32_t *left, int32_t *right);

/* NES APU (NSFPlay port) */
extern void *NES_APU_np_Create(uint32_t clock, uint32_t rate);
extern void  NES_APU_np_Destroy(void *chip);
extern void  NES_APU_np_Reset(void *chip);
extern int   NES_APU_np_Write(void *chip, uint16_t addr, uint8_t val);
extern uint32_t NES_APU_np_Render(void *chip, int32_t b[2]);

extern void *NES_DMC_np_Create(uint32_t clock, uint32_t rate);
extern void  NES_DMC_np_Destroy(void *chip);
extern void  NES_DMC_np_Reset(void *chip);
extern int   NES_DMC_np_Write(void *chip, uint16_t addr, uint8_t val);
extern uint32_t NES_DMC_np_Render(void *chip, int32_t b[2]);
extern void  NES_DMC_np_SetAPU(void *chip, void *apu);

/* ---- helpers ---- */

static inline uint16_t rd16le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint32_t rd32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* ---- VGM header parsing ---- */

int vgm_load(vgm_player_t *p, const uint8_t *data, uint32_t len,
             uint32_t sample_rate)
{
    memset(p, 0, sizeof(*p));
    p->file_data = data;
    p->file_len  = len;
    p->sample_rate = sample_rate;

    if (len < 0x40) return -1;
    if (data[0] != 'V' || data[1] != 'g' || data[2] != 'm' || data[3] != ' ')
        return -2;

    vgm_header_t *h = &p->hdr;
    h->file_ver      = rd32le(data + 0x08);
    h->eof_ofs       = rd32le(data + 0x04) + 0x04;
    h->total_samples = rd32le(data + 0x18);

    /* Loop */
    uint32_t lo = rd32le(data + 0x1C);
    h->loop_ofs = lo ? (lo + 0x1C) : 0;
    h->loop_samples = rd32le(data + 0x20);

    /* Data offset */
    if (h->file_ver >= 0x0150 && len > 0x38) {
        uint32_t dofs = rd32le(data + 0x34);
        h->data_ofs = dofs ? (dofs + 0x34) : 0x40;
    } else {
        h->data_ofs = 0x40;
    }
    h->data_end = h->eof_ofs;

    /* Chip clocks (0 = not present) */
    h->sn76489_clock = (len > 0x0F) ? rd32le(data + 0x0C) : 0;
    h->ym2612_clock  = (len > 0x2F) ? rd32le(data + 0x2C) : 0;

    /* GB DMG: offset 0x80, version >= 1.61 */
    if (h->file_ver >= 0x0161 && len > 0x83)
        h->gb_dmg_clock = rd32le(data + 0x80);

    /* NES APU: offset 0x84, version >= 1.61 */
    if (h->file_ver >= 0x0161 && len > 0x87)
        h->nes_apu_clock = rd32le(data + 0x84);

    /* Detect chips */
    if (h->ym2612_clock)  p->chips_present |= VGM_CHIP_YM2612;
    if (h->sn76489_clock) p->chips_present |= VGM_CHIP_SN76489;
    if (h->gb_dmg_clock)  p->chips_present |= VGM_CHIP_GB_DMG;
    if (h->nes_apu_clock) p->chips_present |= VGM_CHIP_NES_APU;

    /* Auto-select native DAC rate from primary chip. */
    if (sample_rate == 0) {
        if (p->chips_present & VGM_CHIP_GB_DMG)
            sample_rate = (h->gb_dmg_clock & 0x3FFFFFFF) / 64;  /* 65536 */
        else
            sample_rate = 48000;  /* all consoles use proven 48kHz */
    }
    p->sample_rate = sample_rate;

    /* Initialize chip emulators */
    if (h->ym2612_clock) {
        p->opn2 = gens_ym2612_init(h->ym2612_clock & 0x3FFFFFFF, sample_rate);
        if (p->opn2) YM2612_Reset(p->opn2);
    }

    if (h->sn76489_clock) {
        uint32_t clk = h->sn76489_clock & 0x3FFFFFFF;
        p->sn76489 = sn76489_jupiter_init(clk, sample_rate);
        if (p->sn76489) {
            sn76489_jupiter_reset(p->sn76489);
            extern void sn76489_jupiter_unmute(void *chip);
            sn76489_jupiter_unmute(p->sn76489);
        }
    }

    if (h->nes_apu_clock) {
        uint32_t clk = h->nes_apu_clock & 0x3FFFFFFF;
        p->nes_apu = NES_APU_np_Create(clk, sample_rate);
        p->nes_dmc = NES_DMC_np_Create(clk, sample_rate);
        if (p->nes_apu && p->nes_dmc) {
            NES_DMC_np_SetAPU(p->nes_dmc, p->nes_apu);
            NES_APU_np_Reset(p->nes_apu);
            NES_DMC_np_Reset(p->nes_dmc);
        }
    }

    if (h->gb_dmg_clock) {
        uint32_t clk = h->gb_dmg_clock & 0x3FFFFFFF;
        p->gb = gb_jupiter_init(clk, sample_rate);
        if (p->gb) gb_jupiter_reset(p->gb);
    }

    p->file_pos = h->data_ofs;
    p->state = VGM_STOPPED;
    return 0;
}

void vgm_play(vgm_player_t *p)
{
    p->file_pos     = p->hdr.data_ofs;
    p->wait_remain  = 0;
    p->cur_sample   = 0;
    p->cur_loop     = 0;
    p->total_ticks  = 0;
    p->total_samples = 0;
    p->pcm_ofs      = 0;
    p->state        = VGM_PLAYING;

    if (p->opn2) YM2612_Reset(p->opn2);
}

void vgm_stop(vgm_player_t *p)
{
    p->state = VGM_STOPPED;
}

extern void heap_reset(void);

void vgm_unload(vgm_player_t *p)
{
    if (p->opn2)    { YM2612_End(p->opn2);    p->opn2 = NULL; }
    if (p->nes_apu) { NES_APU_np_Destroy(p->nes_apu); p->nes_apu = NULL; }
    if (p->nes_dmc) { NES_DMC_np_Destroy(p->nes_dmc); p->nes_dmc = NULL; }
    if (p->sn76489) { sn76489_jupiter_shutdown(p->sn76489); p->sn76489 = NULL; }
    if (p->gb)      { gb_jupiter_shutdown(p->gb);      p->gb = NULL; }
    if (p->sn76489) { sn76489_jupiter_shutdown(p->sn76489); p->sn76489 = NULL; }
    p->state = VGM_STOPPED;
    heap_reset();  /* reclaim memory for next track */
}

/* ---- command processing ---- */

/*
 * Process commands until we hit a wait or end-of-data.
 * Returns number of VGM samples (44100Hz) to wait.
 */
static uint32_t vgm_process_cmds(vgm_player_t *p)
{
    const uint8_t *d = p->file_data;
    uint32_t pos = p->file_pos;

    for (;;) {
        if (pos >= p->hdr.data_end) {
            /* End of data — check for loop */
            if (p->hdr.loop_ofs) {
                pos = p->hdr.loop_ofs;
                p->cur_loop++;
                continue;
            }
            p->state = VGM_STOPPED;
            p->file_pos = pos;
            return 0;
        }

        uint8_t cmd = d[pos++];

        switch (cmd) {
        /* ---- Wait commands ---- */
        case 0x61: { /* wait N samples */
            uint32_t n = rd16le(d + pos); pos += 2;
            p->file_pos = pos;
            return n;
        }
        case 0x62: /* wait 735 (60Hz) */
            p->file_pos = pos;
            return 735;
        case 0x63: /* wait 882 (50Hz) */
            p->file_pos = pos;
            return 882;

        /* ---- End of data ---- */
        case 0x66:
            if (p->hdr.loop_ofs) {
                pos = p->hdr.loop_ofs;
                p->cur_loop++;
                continue;
            }
            p->state = VGM_STOPPED;
            p->file_pos = pos;
            return 0;

        /* ---- Data block ---- */
        case 0x67: {
            /* 67 66 tt ss ss ss ss [data...] */
            pos++; /* skip 0x66 */
            uint8_t blk_type = d[pos++];
            uint32_t blk_size = rd32le(d + pos); pos += 4;

            /* Store PCM data blocks (type 0x00 = YM2612 PCM) */
            if (blk_type == 0x00 && p->pcm_block_count < VGM_MAX_PCM_BLOCKS) {
                int idx = p->pcm_block_count++;
                p->pcm_blocks[idx].data = d + pos;
                p->pcm_blocks[idx].size = blk_size;
            }
            pos += blk_size;
            break;
        }

        /* ---- SN76489 ---- */
        case 0x50:
            if (p->sn76489)
                sn76489_jupiter_write(p->sn76489, d[pos]);
            pos++;
            break;

        /* ---- YM2612 port 0 ---- */
        case 0x52:
            if (p->opn2) {
                uint8_t a = d[pos], v = d[pos+1];
                uint32_t li = vgm_ym_log.wr_idx & (VGM_REG_LOG_SIZE - 1);
                vgm_ym_log.log[li].addr = a;
                vgm_ym_log.log[li].val = v;
                vgm_ym_log.log[li].tick = p->cur_sample;
                vgm_ym_log.wr_idx++;
                vgm_ym_log.total_writes++;
                vgm_ym_log.writes_per_addr[a]++;
                YM2612_Write(p->opn2, 0, a);
                YM2612_Write(p->opn2, 1, v);
            }
            pos += 2;
            break;

        /* ---- YM2612 port 1 ---- */
        case 0x53:
            if (p->opn2) {
                uint8_t a = d[pos], v = d[pos+1];
                uint32_t li = vgm_ym_log.wr_idx & (VGM_REG_LOG_SIZE - 1);
                vgm_ym_log.log[li].addr = a | 0x80; /* mark as port 1 */
                vgm_ym_log.log[li].val = v;
                vgm_ym_log.log[li].tick = p->cur_sample;
                vgm_ym_log.wr_idx++;
                vgm_ym_log.total_writes++;
                vgm_ym_log.writes_per_addr[a]++;
                YM2612_Write(p->opn2, 2, a);
                YM2612_Write(p->opn2, 3, v);
            }
            pos += 2;
            break;

        /* ---- GB DMG register write ---- */
        case 0xB3: {
            uint8_t ofs = d[pos] & 0x7F;
            uint8_t val = d[pos + 1];
            uint32_t li = vgm_gb_log.wr_idx & (VGM_REG_LOG_SIZE - 1);
            vgm_gb_log.log[li].tick = p->cur_sample;
            vgm_gb_log.log[li].addr = ofs;
            vgm_gb_log.log[li].val  = val;
            vgm_gb_log.wr_idx++;
            vgm_gb_log.total_writes++;
            vgm_gb_log.writes_per_addr[ofs]++;
            if (p->gb)
                gb_jupiter_write(p->gb, ofs, val);
            pos += 2;
            break;
        }

        /* ---- NES APU register write ---- */
        case 0xB4: {
            uint8_t ofs = d[pos] & 0x7F;
            uint8_t val = d[pos + 1];
            /* Log */
            uint32_t li = vgm_nes_log.wr_idx & (VGM_REG_LOG_SIZE - 1);
            vgm_nes_log.log[li].tick = p->cur_sample;
            vgm_nes_log.log[li].addr = ofs;
            vgm_nes_log.log[li].val  = val;
            vgm_nes_log.wr_idx++;
            vgm_nes_log.total_writes++;
            vgm_nes_log.writes_per_addr[ofs]++;
            /* Route to BOTH APU and DMC */
            if (p->nes_apu)
                NES_APU_np_Write(p->nes_apu, 0x4000 | ofs, val);
            if (p->nes_dmc)
                NES_DMC_np_Write(p->nes_dmc, 0x4000 | ofs, val);
            pos += 2;
            break;
        }

        /* ---- YM2612 PCM seek ---- */
        case 0xE0:
            p->pcm_ofs = rd32le(d + pos);
            pos += 4;
            break;

        default:
            /* ---- Short waits: 0x70-0x7F ---- */
            if (cmd >= 0x70 && cmd <= 0x7F) {
                p->file_pos = pos;
                return (cmd & 0x0F) + 1;
            }

            /* ---- YM2612 DAC write + short wait: 0x80-0x8F ---- */
            if (cmd >= 0x80 && cmd <= 0x8F) {
                if (p->opn2 && p->pcm_block_count > 0) {
                    const vgm_pcm_block_t *blk = &p->pcm_blocks[0];
                    if (p->pcm_ofs < blk->size) {
                        YM2612_Write(p->opn2, 0, 0x2A);
                        YM2612_Write(p->opn2, 1, blk->data[p->pcm_ofs]);
                    }
                    p->pcm_ofs++;
                }
                uint32_t wait = cmd & 0x0F;
                if (wait > 0) {
                    p->file_pos = pos;
                    return wait;
                }
                break; /* wait=0: continue processing */
            }

            /* ---- SN76489 2nd chip ---- */
            if (cmd == 0x30) {
                pos++; /* skip data byte, we only support 1 SN76489 */
                break;
            }

            /* ---- 2nd chip variants (0xA0-0xBF range): skip ---- */
            if (cmd >= 0xA0 && cmd <= 0xBF) {
                pos += 2;
                break;
            }

            /* ---- 4-byte commands (0xC0-0xDF, 0xE1-0xEF) ---- */
            if ((cmd >= 0xC0 && cmd <= 0xDF) || (cmd >= 0xE1 && cmd <= 0xEF)) {
                pos += 3;
                break;
            }

            /* ---- PCM RAM write (0x68) ---- */
            if (cmd == 0x68) {
                pos += 11; /* skip entire command */
                break;
            }

            /* ---- DAC stream commands (0x90-0x95) ---- */
            if (cmd >= 0x90 && cmd <= 0x95) {
                static const uint8_t dac_cmd_len[] = {5, 5, 6, 11, 1, 5};
                pos += dac_cmd_len[cmd - 0x90] - 1;
                break;
            }

            /* ---- Unknown: skip 1 byte as fallback ---- */
            break;
        }
    }
}

/* ---- render ---- */

/* Advance VGM command stream by 1 tick (1/44100th second) */
static void vgm_tick(vgm_player_t *p)
{
    p->total_ticks++;
    if (p->wait_remain > 0) {
        p->wait_remain--;
        return;
    }
    /* Process commands until next wait */
    p->wait_remain = vgm_process_cmds(p);
    if (p->wait_remain > 0)
        p->wait_remain--;  /* consume the current tick */
}

void vgm_render(vgm_player_t *p, int16_t *out_buf, uint32_t num_samples)
{
    if (p->state == VGM_STOPPED) {
        memset(out_buf, 0, num_samples * 2 * sizeof(int16_t));
        return;
    }

    /* VGM ticks at 44100Hz. Two paths:
     *  1. DAC at 44100Hz → 1:1, one tick per output sample. Zero jitter.
     *  2. DAC at other rate → fractional accumulator for VGM timing. */
    int native_vgm = (p->sample_rate == 44100);

    /* 16.16 fixed-point step for non-44100 rates */
    uint32_t tick_step = native_vgm ? 0 :
        (uint32_t)(44100ULL * 65536 / p->sample_rate);

    for (uint32_t i = 0; i < num_samples; i++) {
        if (p->state == VGM_STOPPED) {
            out_buf[i * 2 + 0] = 0;
            out_buf[i * 2 + 1] = 0;
            continue;
        }

        /* Advance VGM time */
        if (native_vgm) {
            /* Perfect 1:1 — every output sample is exactly 1 VGM tick */
            vgm_tick(p);
        } else {
            /* Fractional: accumulate and tick when whole ticks elapse */
            p->tick_accum += tick_step;
            uint32_t ticks = p->tick_accum >> 16;
            p->tick_accum &= 0xFFFF;
            for (uint32_t t = 0; t < ticks; t++)
                vgm_tick(p);
        }

        p->total_samples++;
        /* Clock chip emulators for 1 output sample */
        int32_t left = 0, right = 0;

        if (p->opn2) {
            int32_t opn_buf[2] = {0, 0};
            int32_t *opn_ptrs[2] = {&opn_buf[0], &opn_buf[1]};
            gens_ym2612_update(p->opn2, 1, opn_ptrs);
            left  += opn_buf[0];
            right += opn_buf[1];
        }

        if (p->nes_apu && p->nes_dmc) {
            int32_t apu_buf[2] = {0, 0};
            int32_t dmc_buf[2] = {0, 0};
            NES_APU_np_Render(p->nes_apu, apu_buf);
            NES_DMC_np_Render(p->nes_dmc, dmc_buf);
            left  += apu_buf[0] + dmc_buf[0];
            right += apu_buf[1] + dmc_buf[1];
        }

        if (p->sn76489) {
            int32_t psg_l = 0, psg_r = 0;
            sn76489_jupiter_update(p->sn76489, 1, &psg_l, &psg_r);
            left  += psg_l / 6;  /* PSG is ~6x louder than FM at chip output — attenuate to match */
            right += psg_r / 6;
        }

        if (p->gb) {
            int32_t gl = 0, gr = 0;
            gb_jupiter_update(p->gb, 1, &gl, &gr);
            left  += gl;
            right += gr;
        }

        /* Clip to 16-bit */
        if (left  >  32767) left  =  32767;
        if (left  < -32768) left  = -32768;
        if (right >  32767) right =  32767;
        if (right < -32768) right = -32768;

        out_buf[i * 2 + 0] = (int16_t)left;
        out_buf[i * 2 + 1] = (int16_t)right;
    }
}
