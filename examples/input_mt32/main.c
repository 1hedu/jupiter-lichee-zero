/*
 * Tri-Layer + MT-32 — Four systems, three controllers, Monkey Island
 *
 * Same tri-layer layout, sprites controlled by NES/N64/Genesis pads,
 * MT-32 playing the Monkey Island theme in real-time.
 *
 * Build: make GAME=examples/input_mt32/main.c
 */
#include "jupiter.h"
#include "nes.h"
#include "gb.h"
#include "genesis.h"
#include "snes.h"
#include "input.h"
#include "pmu.h"
#include <string.h>
#include "c_interface/c_interface.h"
#include "../../examples/mt32_poc/mt32_control_rom.h"
#include "../../examples/mt32_poc/mt32_pcm_rom.h"
#include "../../examples/mt32_monkey/monkey_theme.h"

/* Pre-converted tile data */
#include "../../examples/cedar_nes/vinci_chr.h"
#include "../../examples/cedar_gb/celebi_chr.h"
#include "../../examples/cedar_snes/soldier_chr.h"

/* Genesis: raw ARGB sprite sheet embedded via objcopy */
extern const uint8_t _binary_tools_pulseman_argb_start[];
extern const uint8_t _binary_tools_pulseman_argb_end[];

/* ================================================================== */
/* SMF parser (from mt32_monkey)                                        */
/* ================================================================== */
#define MAX_TRACKS 16

typedef struct {
    const uint8_t *cursor;
    const uint8_t *trk_end;
    uint8_t  running_status;
    uint64_t next_event_us;
    uint8_t  done;
} smf_track_t;

typedef struct {
    const uint8_t *data;
    uint32_t size;
    uint16_t format, ntrks, division;
    smf_track_t tracks[MAX_TRACKS];
    uint32_t num_tracks;
    uint32_t us_per_tick, us_per_quarter;
    uint8_t  done;
} smf_t;

static inline uint16_t be16(const uint8_t *p) { return ((uint16_t)p[0]<<8)|p[1]; }
static inline uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
static inline uint32_t read_vlq(const uint8_t **p) {
    uint32_t v=0; uint8_t b;
    do { b=**p; (*p)++; v=(v<<7)|(b&0x7F); } while (b&0x80);
    return v;
}

static int smf_init(smf_t *s, const uint8_t *data, uint32_t size) {
    if (size<14||data[0]!='M'||data[1]!='T'||data[2]!='h'||data[3]!='d') return -1;
    uint32_t hdr_len=be32(data+4);
    s->data=data; s->size=size; s->format=be16(data+8);
    s->ntrks=be16(data+10); s->division=be16(data+12); s->done=0;
    s->us_per_quarter=500000;
    uint16_t div=s->division?s->division:96;
    s->us_per_tick=s->us_per_quarter/div;
    const uint8_t *p=data+8+hdr_len, *end=data+size;
    s->num_tracks=0;
    while (p+8<=end && s->num_tracks<MAX_TRACKS) {
        uint32_t clen=be32(p+4);
        if (p[0]=='M'&&p[1]=='T'&&p[2]=='r'&&p[3]=='k') {
            smf_track_t *t=&s->tracks[s->num_tracks];
            t->cursor=p+8; t->trk_end=p+8+clen;
            if (t->trk_end>end) t->trk_end=end;
            t->running_status=0; t->done=0;
            const uint8_t *cp=t->cursor;
            uint32_t delta=read_vlq(&cp); t->cursor=cp;
            t->next_event_us=(uint64_t)delta*s->us_per_tick;
            s->num_tracks++;
        }
        p+=8+clen;
    }
    return s->num_tracks?0:-2;
}

static int smf_step_track(smf_t *s, smf_track_t *t, mt32emu_const_context ctx) {
    if (t->done||t->cursor>=t->trk_end) { t->done=1; return 0; }
    const uint8_t *p=t->cursor;
    uint8_t status=*p;
    if (status&0x80) p++; else status=t->running_status;
    if (status==0xFF) {
        uint8_t type=*p++; uint32_t len=read_vlq(&p);
        if (type==0x51&&len==3) {
            s->us_per_quarter=((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|(uint32_t)p[2];
            s->us_per_tick=s->us_per_quarter/(s->division?s->division:96);
        } else if (type==0x2F) { t->done=1; return 0; }
        p+=len;
    } else if (status==0xF0||status==0xF7) {
        uint32_t len=read_vlq(&p);
        static uint8_t sysex_buf[2048];
        if (status==0xF0&&len+1<=sizeof(sysex_buf)) {
            sysex_buf[0]=0xF0;
            for (uint32_t i=0;i<len;i++) sysex_buf[1+i]=p[i];
            mt32emu_play_sysex(ctx,sysex_buf,len+1);
        }
        p+=len;
    } else {
        uint8_t top=status&0xF0, d1=*p++, d2=0;
        if (top!=0xC0&&top!=0xD0) d2=*p++;
        t->running_status=status;
        mt32emu_play_msg(ctx,(uint32_t)status|((uint32_t)d1<<8)|((uint32_t)d2<<16));
    }
    t->cursor=p;
    if (t->cursor>=t->trk_end) { t->done=1; return 0; }
    uint32_t delta=read_vlq(&t->cursor);
    t->next_event_us+=(uint64_t)delta*s->us_per_tick;
    return 1;
}

static void smf_advance(smf_t *s, uint64_t cur_us, mt32emu_const_context ctx) {
    for (;;) {
        int best=-1; uint64_t best_us=~0ULL;
        for (uint32_t i=0;i<s->num_tracks;i++)
            if (!s->tracks[i].done&&s->tracks[i].next_event_us<best_us)
                { best_us=s->tracks[i].next_event_us; best=(int)i; }
        if (best<0||best_us>cur_us) break;
        if (!smf_step_track(s,&s->tracks[best],ctx)) continue;
    }
    s->done=1;
    for (uint32_t i=0;i<s->num_tracks;i++)
        if (!s->tracks[i].done) { s->done=0; break; }
}

/* Audio ring buffer + diagnostics (shared with DMA ISR in audio.c) */
#define MIX_BUF_SIZE 4096
#define MIX_BUF_MASK (MIX_BUF_SIZE - 1)
extern int16_t  mix_buf[MIX_BUF_SIZE];
extern volatile uint32_t mix_wr;
extern volatile uint32_t mix_rd;
extern volatile uint32_t audio_underruns;
extern volatile uint32_t audio_transitions;

#define CHUNK_FRAMES 128
static int16_t chunk[CHUNK_FRAMES * 2];
#define OUTPUT_RATE 48000
#define TARGET_DEPTH 3800

static mt32emu_context mt32_ctx;
static uint64_t total_frames_rendered;
static smf_t smf;

/* Pre-rendered PCM buffer in upper DRAM (above framebuffers/CedarVE) */
#define PRERENDER_ADDR   0x42C00000
#define PRERENDER_SECS   100   /* 100 seconds of audio */
#define PRERENDER_FRAMES (PRERENDER_SECS * OUTPUT_RATE)
#define PRERENDER_SAMPLES (PRERENDER_FRAMES * 2)  /* stereo */
static int16_t *prerender_buf = (int16_t *)PRERENDER_ADDR;
static uint32_t prerender_total_samples;
static uint32_t prerender_play_pos;
static int use_prerender = 1;

static int32_t peak_sample = 0;
static uint32_t clip_count = 0;
static uint32_t glitch_count = 0;    /* sample-to-sample jumps > threshold */
static int32_t glitch_max_delta = 0; /* largest jump seen */
static int16_t last_l = 0, last_r = 0;
static uint32_t drop_count = 0;      /* samples dropped (ring full) */
#define GLITCH_THRESH 8000           /* ~24% of full scale */

static void mt32_fill(void) {
    mt32emu_render_bit16s(mt32_ctx, chunk, CHUNK_FRAMES);
    for (uint32_t i = 0; i < CHUNK_FRAMES; i++) {
        if ((mix_wr - mix_rd) >= (MIX_BUF_SIZE - 2)) {
            drop_count += (CHUNK_FRAMES - i);
            break;
        }
        int16_t l = chunk[i * 2], r = chunk[i * 2 + 1];

        /* Peak tracking */
        int32_t al = l < 0 ? -l : l;
        int32_t ar = r < 0 ? -r : r;
        if (al > peak_sample) peak_sample = al;
        if (ar > peak_sample) peak_sample = ar;
        if (l == 32767 || l == -32768) clip_count++;
        if (r == 32767 || r == -32768) clip_count++;

        /* Discontinuity detection: large sample-to-sample jump */
        int32_t dl = (int32_t)l - (int32_t)last_l;
        int32_t dr = (int32_t)r - (int32_t)last_r;
        if (dl < 0) dl = -dl;
        if (dr < 0) dr = -dr;
        if (dl > GLITCH_THRESH) { glitch_count++; if (dl > glitch_max_delta) glitch_max_delta = dl; }
        if (dr > GLITCH_THRESH) { glitch_count++; if (dr > glitch_max_delta) glitch_max_delta = dr; }
        last_l = l;
        last_r = r;

        mix_buf[mix_wr & MIX_BUF_MASK] = l; mix_wr++;
        mix_buf[mix_wr & MIX_BUF_MASK] = r; mix_wr++;
    }
    total_frames_rendered += CHUNK_FRAMES;
    audio_update();  /* poll DMA refill alongside ISR */
}

/* ================================================================== */
/* Layout constants                                                     */
/* ================================================================== */
#define GB_X    12
#define GB_Y    64   /* (272-144)/2 */
#define NES_X   188  /* 12 + 160 + 16 */
#define NES_Y   24   /* (272-224)/2 */
#define SNES_VW 160  /* VI1 viewport */
#define SNES_VH 144
#define SNES_X  312  /* bottom-right */
#define SNES_Y  120

/* ================================================================== */
/* Compositing temp buffers                                             */
/* ================================================================== */
static uint32_t gb_fb[GB_NATIVE_W * GB_NATIVE_H];
static uint32_t gb_ovl[GB_NATIVE_W * GB_NATIVE_H];
static uint32_t nes_fb[NES_NATIVE_W * NES_NATIVE_H];
static uint32_t nes_ovl[NES_NATIVE_W * NES_NATIVE_H];
static uint32_t snes_comp[SNES_VW * SNES_VH];
static uint32_t snes_tmp[SNES_VW * SNES_VH];

/* Composite overlay onto bg buffer (where overlay has alpha) */
static void composite(uint32_t *bg, const uint32_t *ovl, int count)
{
    for (int i = 0; i < count; i++)
        if (ovl[i] & 0xFF000000) bg[i] = ovl[i];
}

/* Blit rect from src (pitch=sw) to dst (pitch=LCD_W) at (dx,dy) */
static void blit(volatile uint32_t *dst, const uint32_t *src,
                 int sw, int sh, int dx, int dy)
{
    for (int y = 0; y < sh; y++)
        memcpy((void *)&dst[(dy + y) * LCD_W + dx], &src[y * sw], sw * 4);
}

/* ================================================================== */
/* Genesis runtime ARGB→4bpp conversion                                 */
/* ================================================================== */
#define GEN_SHEET_W  512
#define GEN_SHEET_H  480
#define GEN_SRC_PY   71
#define GEN_SRC_TH   7     /* 56px / 8 */
#define GEN_FRAMES   5     /* use frames 3-7 (32px wide) */

static const struct { int x, pw; } gen_src[GEN_FRAMES] = {
    {164,32}, {199,31}, {233,31}, {267,32}, {301,32},
};

/* Per-frame: top sprite 4×4=16 tiles + bottom sprite 4×3=12 tiles = 28 */
#define GEN_TPF  28
static uint8_t gen_tiles[GEN_FRAMES * GEN_TPF * 32];
static uint32_t gen_cram[64];
static uint32_t gen_bg_color;

static int gen_color_near(uint32_t a, uint32_t b)
{
    int dr = (int)((a >> 16) & 0xFF) - (int)((b >> 16) & 0xFF);
    int dg = (int)((a >> 8) & 0xFF)  - (int)((b >> 8) & 0xFF);
    int db = (int)(a & 0xFF)          - (int)(b & 0xFF);
    return (dr * dr + dg * dg + db * db) < 40 * 40;
}

static void gen_extract_palette(const uint32_t *sheet)
{
    gen_bg_color = sheet[0] | 0xFF000000;
    memset(gen_cram, 0, sizeof(gen_cram));
    gen_cram[0] = 0x00000000;
    int count = 0;
    for (int f = 0; f < GEN_FRAMES && count < 15; f++) {
        int fx = gen_src[f].x, pw = gen_src[f].pw;
        for (int y = GEN_SRC_PY; y < GEN_SRC_PY + 56 && count < 15; y++)
            for (int x = fx; x < fx + pw && count < 15; x++) {
                uint32_t c = sheet[y * GEN_SHEET_W + x] | 0xFF000000;
                if (gen_color_near(c, gen_bg_color)) continue;
                if ((c & 0x00FFFFFF) < 0x080808) continue;
                int dup = 0;
                for (int i = 0; i <= count; i++)
                    if (gen_color_near(c, gen_cram[i])) { dup = 1; break; }
                if (!dup) gen_cram[++count] = c;
            }
    }
}

static int gen_nearest(uint32_t c, const uint32_t *sheet)
{
    if (gen_color_near(c | 0xFF000000, gen_bg_color)) return 0;
    if ((c & 0x00FFFFFF) < 0x080808) return 0;
    c |= 0xFF000000;
    int best = 0, bd = 999999;
    for (int i = 1; i < 16; i++) {
        if (!gen_cram[i]) continue;
        int dr = (int)((c >> 16) & 0xFF) - (int)((gen_cram[i] >> 16) & 0xFF);
        int dg = (int)((c >> 8) & 0xFF)  - (int)((gen_cram[i] >> 8) & 0xFF);
        int db = (int)(c & 0xFF)          - (int)(gen_cram[i] & 0xFF);
        int d = dr * dr + dg * dg + db * db;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static void gen_convert_tile(uint8_t *out, const uint32_t *sheet,
                              int fx, int tile_px, int tile_py, int pw)
{
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 4; c++) {
            int px0 = tile_px + c * 2;
            int px1 = px0 + 1;
            int py  = tile_py + r;
            uint8_t hi = 0, lo = 0;
            if (px0 < pw && py < 56)
                hi = gen_nearest(sheet[(GEN_SRC_PY + py) * GEN_SHEET_W + fx + px0], sheet);
            if (px1 < pw && py < 56)
                lo = gen_nearest(sheet[(GEN_SRC_PY + py) * GEN_SHEET_W + fx + px1], sheet);
            out[r * 4 + c] = (hi << 4) | lo;
        }
}

static void gen_convert_frames(const uint32_t *sheet)
{
    for (int f = 0; f < GEN_FRAMES; f++) {
        int fx = gen_src[f].x;
        int pw = gen_src[f].pw;
        uint8_t *base = &gen_tiles[f * GEN_TPF * 32];

        /* Top sprite: 4×4 tiles (column-major with h=4) */
        for (int tx = 0; tx < 4; tx++)
            for (int ty = 0; ty < 4; ty++)
                gen_convert_tile(&base[(tx * 4 + ty) * 32],
                                 sheet, fx, tx * 8, ty * 8, pw);

        /* Bottom sprite: 4×3 tiles (column-major with h=3) */
        for (int tx = 0; tx < 4; tx++)
            for (int ty = 0; ty < 3; ty++)
                gen_convert_tile(&base[(16 + tx * 3 + ty) * 32],
                                 sheet, fx, tx * 8, (4 + ty) * 8, pw);
    }
}

/* ================================================================== */
/* GB state                                                             */
/* ================================================================== */
static uint8_t gb_chr[256 * 16];
static uint8_t gb_map[GB_MAP_SIZE];
static uint32_t gb_palette[32];
static uint32_t gb_spr_pal[32];
static gb_oam_entry_t gb_oam[GB_MAX_SPRITES];

/* ================================================================== */
/* NES state                                                            */
/* ================================================================== */
static uint8_t nes_chr[256 * 16];
static uint8_t nes_palette_ram[32];
static uint8_t nes_nametable[NES_NT_TILES];
static uint8_t nes_attribute[NES_NT_ATTRS];
static nes_oam_entry_t nes_oam[NES_MAX_SPRITES];

/* ================================================================== */
/* Genesis state                                                        */
/* ================================================================== */
static uint16_t gen_bg_map[64 * 32];
static genesis_sprite_t gen_sprites[2];

/* ================================================================== */
/* SNES state                                                           */
/* ================================================================== */
static uint8_t snes_bg_chr[256 * 32];
static uint16_t snes_bg_map[32 * 32];
static uint32_t snes_bg_pal[128];
static snes_sprite_t snes_sprites[1];

/* ================================================================== */
/* Main                                                                 */
/* ================================================================== */
int main(void)
{
    timer_init();
    mmu_init();
    pmu_init();

    uart_puts("\n========================================\n");
    uart_puts("  NES Controller Input Test\n");
    uart_puts("========================================\n\n");

    /* ---- GIC must be set up BEFORE audio_start registers its DMA handler ---- */
    /* Verify CPU clock */
    {
        uint32_t pll = REG32(0x01C20000);
        uint32_t n = ((pll >> 8) & 0x1F) + 1;
        uint32_t k = ((pll >> 4) & 0x3) + 1;
        uint32_t m = (pll & 0x3) + 1;
        uart_puts("[cpu] PLL_CPUX=0x"); uart_puthex(pll);
        uart_puts(" freq="); uart_putdec(24 * n * k / m);
        uart_puts("MHz\n");
    }

    uart_puts("[dbg] irq_init...\n");
    irq_init();

    /* ---- MT-32 synth init ---- */
    uart_puts("[dbg] cpp_init...\n");
    cpp_init();
    uart_puts("[dbg] audio_init_24bit...\n");
    audio_init_24bit();
    {
        uart_puts("[dbg] mt32 create_context...\n");
        mt32emu_report_handler_i null_report = { 0 };
        mt32_ctx = mt32emu_create_context(null_report, 0);
        uart_puts("[dbg] mt32 add ROMs...\n");
        mt32emu_add_rom_data(mt32_ctx, mt32_control_rom, mt32_control_rom_len, 0);
        mt32emu_add_rom_data(mt32_ctx, mt32_pcm_rom, mt32_pcm_rom_len, 0);
        uart_puts("[dbg] mt32 open_synth...\n");
        /* ACCURATE mode renders natively at 48kHz — no SRC needed.
         * COARSE+SRC is 10x slower (sinc filter per sample). */
        mt32emu_set_analog_output_mode(mt32_ctx, MT32EMU_AOM_ACCURATE);
        mt32emu_open_synth(mt32_ctx);
        {
            extern uint32_t heap_used(void);
            uart_puts("[dbg] heap used: "); uart_putdec(heap_used() / 1024);
            uart_puts("KB / 1024KB\n");
        }
        mt32emu_set_output_gain(mt32_ctx, 2.0f);
        smf_init(&smf, smf_monkey_theme, smf_monkey_theme_len);

        /* Pre-fill mix_buf then start DMA */
        audio_set_rate(OUTPUT_RATE);
        total_frames_rendered = 0;
        for (int i = 0; i < 16; i++) mt32_fill();
        audio_start();
        irq_global_enable();
        uart_puts("[mt32] Monkey Island playing\n");
    }

    /* ---- Controller init (all three, different pins, no conflict) ---- */
    input_init(INPUT_NES);
    input_init(INPUT_N64);
    input_init(INPUT_GENESIS);

    /* ---- Genesis: runtime ARGB→4bpp conversion ----
     * Scratch in VRAM at 0x43D00000 (safe regardless of binary BSS
     * size; cedar's region starts at 0x43E00000 leaving 1 MB here for
     * the 960 KB ARGB sheet). 0x43400000 was inside the CODE bss
     * region — collides with bundled menu builds. */
    uint32_t *gen_sheet = (uint32_t *)0x43D00000;
    uint32_t gen_sz = (uint32_t)(_binary_tools_pulseman_argb_end -
                                  _binary_tools_pulseman_argb_start);
    memcpy(gen_sheet, _binary_tools_pulseman_argb_start, gen_sz);
    uart_puts("[gen] ARGB: "); uart_putdec(gen_sz / 1024); uart_puts("KB\n");

    gen_extract_palette(gen_sheet);
    gen_convert_frames(gen_sheet);
    uart_puts("[gen] converted "); uart_putdec(GEN_FRAMES);
    uart_puts(" frames ("); uart_putdec(GEN_TPF);
    uart_puts(" tiles each)\n");

    /* ---- GB setup ---- */
    memset(gb_chr, 0, sizeof(gb_chr));
    for (int r = 0; r < 8; r++) {
        gb_chr[1 * 16 + r]     = (r < 3) ? 0xAA : 0xFF;
        gb_chr[1 * 16 + r + 8] = (r < 3) ? 0x55 : 0x00;
    }
    for (int r = 0; r < 8; r++) {
        gb_chr[2 * 16 + r] = 0xFF; gb_chr[2 * 16 + r + 8] = 0x00;
    }
    memset(gb_map, 0, sizeof(gb_map));
    for (int x = 0; x < GB_MAP_W; x++) {
        gb_map[15 * GB_MAP_W + x] = 1;
        for (int y = 16; y < GB_MAP_H; y++) gb_map[y * GB_MAP_W + x] = 2;
    }
    gb_palette[0] = 0xFF88CC88; gb_palette[1] = 0xFF306830;
    gb_palette[2] = 0xFF58A858; gb_palette[3] = 0xFF98D898;
    gb_spr_pal[0] = 0x00000000;
    gb_spr_pal[1] = celebi_pal[1];
    gb_spr_pal[2] = celebi_pal[2];
    gb_spr_pal[3] = celebi_pal[3];

    /* ---- NES setup ---- */
    memset(nes_chr, 0, sizeof(nes_chr));
    for (int r = 0; r < 8; r++) {
        nes_chr[1 * 16 + r] = 0xFF; nes_chr[1 * 16 + r + 8] = 0x00;
    }
    for (int r = 0; r < 8; r++) {
        if (r < 1)      { nes_chr[2*16+r]=0xFF; nes_chr[2*16+r+8]=0xFF; }
        else if (r < 3) { nes_chr[2*16+r]=(r&1)?0xAA:0x55; nes_chr[2*16+r+8]=0xFF; }
        else             { nes_chr[2*16+r]=0xFF; nes_chr[2*16+r+8]=0x00; }
    }
    for (int r = 0; r < 8; r++) {
        if (r==0||r==4) { nes_chr[3*16+r]=0x00; nes_chr[3*16+r+8]=0xFF; }
        else if (r<4)   { nes_chr[3*16+r]=0xEF; nes_chr[3*16+r+8]=0x00; }
        else            { nes_chr[3*16+r]=0xFE; nes_chr[3*16+r+8]=0x00; }
    }
    memset(nes_nametable, 0, sizeof(nes_nametable));
    for (int x = 0; x < NES_NT_W; x++) {
        nes_nametable[25 * NES_NT_W + x] = 2;
        for (int y = 26; y < NES_NT_H; y++) nes_nametable[y * NES_NT_W + x] = 3;
    }
    memset(nes_attribute, 0, sizeof(nes_attribute));
    for (int ax = 0; ax < 8; ax++) {
        nes_attribute[6 * 8 + ax] = NES_ATTR(0, 0, 1, 1);
        nes_attribute[7 * 8 + ax] = NES_ATTR(1, 1, 1, 1);
    }
    memset(nes_palette_ram, 0x0D, sizeof(nes_palette_ram));
    nes_palette_ram[0] = vinci_bg_color;
    nes_palette_ram[1] = 0x02; nes_palette_ram[2] = 0x12; nes_palette_ram[3] = 0x21;
    nes_palette_ram[5] = 0x17; nes_palette_ram[6] = 0x27; nes_palette_ram[7] = 0x37;
    nes_palette_ram[17] = vinci_spr_palette[1];
    nes_palette_ram[18] = vinci_spr_palette[2];
    nes_palette_ram[19] = vinci_spr_palette[3];

    /* ---- Genesis BG (simple floor) ---- */
    {
        uint8_t floor_tile[32];
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 4; c++)
                floor_tile[r * 4 + c] = (r < 2) ? 0x11 : 0x00;
        /* Append floor tile after sprite tiles */
        int ft_idx = GEN_FRAMES * GEN_TPF;
        memcpy(&gen_tiles[ft_idx * 32], floor_tile, 32);

        gen_cram[16] = 0xFF1A1A3A; /* bg palette 1 */
        gen_cram[17] = 0xFF2A2A5A;

        memset(gen_bg_map, 0, sizeof(gen_bg_map));
        for (int y = 0; y < 28; y++)
            for (int x = 0; x < 40; x++)
                if (y >= 22)
                    gen_bg_map[y * 64 + x] = GEN_ENTRY(ft_idx, 1, 0, 0);
    }

    genesis_plane_t gen_plane_b = {
        .tiles = gen_tiles, .map = gen_bg_map, .cram = gen_cram,
        .scroll_x = 0, .scroll_y = 0, .line_hscroll = NULL,
        .map_w = 64, .map_h = 32, .enabled = 1,
    };
    static uint16_t gen_empty_map[64 * 32];
    memset(gen_empty_map, 0, sizeof(gen_empty_map));
    genesis_plane_t gen_plane_a = {
        .tiles = gen_tiles, .map = gen_empty_map, .cram = gen_cram,
        .scroll_x = 0, .scroll_y = 0, .line_hscroll = NULL,
        .map_w = 64, .map_h = 32, .enabled = 1,
    };

    /* ---- SNES BG ---- */
    memset(snes_bg_chr, 0, sizeof(snes_bg_chr));
    /* Tile 1: solid ground */
    for (int r = 0; r < 8; r++) {
        snes_bg_chr[32 + r*2] = 0xFF; snes_bg_chr[32 + r*2+1] = 0;
        snes_bg_chr[32 + r*2+16] = 0; snes_bg_chr[32 + r*2+17] = 0;
    }
    /* Tile 2: surface */
    for (int r = 0; r < 8; r++) {
        if (r < 2) {
            snes_bg_chr[64 + r*2] = 0xFF; snes_bg_chr[64 + r*2+1] = 0xFF;
            snes_bg_chr[64 + r*2+16] = 0; snes_bg_chr[64 + r*2+17] = 0;
        } else {
            snes_bg_chr[64 + r*2] = 0xFF; snes_bg_chr[64 + r*2+1] = 0;
            snes_bg_chr[64 + r*2+16] = 0; snes_bg_chr[64 + r*2+17] = 0;
        }
    }
    memset(snes_bg_map, 0, sizeof(snes_bg_map));
    for (int x = 0; x < 32; x++) {
        snes_bg_map[15 * 32 + x] = SNES_ENTRY(2, 0, 0, 0, 0);
        for (int y = 16; y < 32; y++)
            snes_bg_map[y * 32 + x] = SNES_ENTRY(1, 0, 0, 0, 0);
    }
    memset(snes_bg_pal, 0, sizeof(snes_bg_pal));
    snes_bg_pal[0] = 0xFF2040A0;
    snes_bg_pal[1] = 0xFF604020;
    snes_bg_pal[2] = 0xFF806040;
    snes_bg_pal[3] = 0xFF40A040;

    snes_bg_t snes_bg1 = {
        .tiles = snes_bg_chr, .map = snes_bg_map, .palette = snes_bg_pal,
        .scroll_x = 0, .scroll_y = 0, .map_w = 32, .map_h = 32,
        .bpp = 4, .enabled = 1,
    };

    /* ---- Display init ---- */
    video_init();

    volatile uint32_t *fb0 = (volatile uint32_t *)FB0_ADDR;
    volatile uint32_t *fb1 = (volatile uint32_t *)FB1_ADDR;
    for (uint32_t i = 0; i < LCD_W * LCD_H; i++) {
        fb0[i] = 0xFF000000; fb1[i] = 0xFF000000;
    }
    dcache_clean_range(FB0_ADDR, LCD_FB_BYTES);
    dcache_clean_range(FB1_ADDR, LCD_FB_BYTES);

    /* VI1: SNES PIP */
    video_vi1_init(SNES_X, SNES_Y, SNES_VW, SNES_VH);

    /* NES BG descriptor */
    nes_bg_t nes_bg = {
        .chr = nes_chr, .nametable = nes_nametable, .attribute = nes_attribute,
        .palette_ram = nes_palette_ram, .scroll_x = 0, .scroll_y = 0,
        .line_scroll_x = NULL, .enabled = 1,
    };

    /* GB BG descriptor */
    gb_bg_t gb_bg = {
        .chr = gb_chr, .map = gb_map, .map_attr = NULL,
        .palette = gb_palette, .scroll_x = 0, .scroll_y = 0, .enabled = 1,
    };

    uart_puts("[main] go!\n");

    /* ---- Sprite positions (D-pad controlled) ---- */
    int buf = 0;
    int gb_frame = 0, gb_x = 60, gb_y_off = 0, gb_flip = 0;
    int nes_frame = 0, nes_x = 120, nes_y_off = 0, nes_flip = 0;
    int gen_frame = 0, gen_x = 140, gen_y_off = 0, gen_flip = 0;
    int snes_frame = 0, snes_sx = 70, snes_sy_off = 0, snes_flip = 0;
    uint32_t anim_t = timer_read();
    int moving = 0;

    /* ---- Audio/frame telemetry ---- */
    uint32_t frame_count = 0;
    uint32_t stat_timer = timer_read();
    uint32_t stat_frames = 0;
    uint32_t stat_render_total = 0, stat_audio_total = 0, stat_frame_total = 0;
    uint32_t stat_render_max = 0, stat_audio_max = 0, stat_frame_max = 0;
    uint32_t stat_underruns_last = audio_underruns;
    uint32_t stat_depth_min = TARGET_DEPTH;

    /* Codec FIFO status: 0x01C22C08
     * Bit 0: TXU_INT — TX FIFO underrun (w1c)
     * Bit 3: TXE_INT — TX FIFO empty
     * Bits [22:8]: TX FIFO available count */
    #define DAC_FIFOS REG32(0x01C22C08)
    uint32_t codec_fifo_xruns = 0;

    while (1) {
        uint32_t frame_t0 = pmu_cycles();

        volatile uint32_t *fb  = buf ? fb1 : fb0;
        volatile uint32_t *ovl = (volatile uint32_t *)(buf ? OVL1_ADDR : OVL_ADDR);
        uint32_t fb_addr  = buf ? FB1_ADDR : FB0_ADDR;
        uint32_t ovl_addr = buf ? OVL1_ADDR : OVL_ADDR;

        /* ---- Poll all three controllers, merge input ---- */
        input_state_t nes_pad = input_poll_nes();
        input_state_t n64_pad = input_poll_n64();
        input_state_t gen_pad = input_poll_genesis();
        uint32_t btn = nes_pad.buttons | n64_pad.buttons | gen_pad.buttons;

        /* Move all sprites together via D-pad + N64 analog stick */
        int dx = 0, dy = 0;
        if (btn & BTN_LEFT)  dx = -2;
        if (btn & BTN_RIGHT) dx = 2;
        if (btn & BTN_UP)    dy = -2;
        if (btn & BTN_DOWN)  dy = 2;
        if (n64_pad.stick_x > 20)       dx = n64_pad.stick_x / 25;
        else if (n64_pad.stick_x < -20) dx = n64_pad.stick_x / 25;
        if (n64_pad.stick_y > 20)       dy = -n64_pad.stick_y / 25;
        else if (n64_pad.stick_y < -20) dy = -n64_pad.stick_y / 25;

        if (dx < 0) { gb_flip = 1; nes_flip = 1; gen_flip = 1; snes_flip = 1; }
        if (dx > 0) { gb_flip = 0; nes_flip = 0; gen_flip = 0; snes_flip = 0; }
        moving = (dx != 0 || dy != 0);

        gb_x += dx; gb_y_off += dy;
        if (gb_x < 0) gb_x = 0;
        if (gb_x > GB_NATIVE_W - CELEBI_PW) gb_x = GB_NATIVE_W - CELEBI_PW;
        if (gb_y_off < -60) gb_y_off = -60;
        if (gb_y_off > 20) gb_y_off = 20;

        nes_x += dx; nes_y_off += dy;
        if (nes_x < 0) nes_x = 0;
        if (nes_x > NES_NATIVE_W - 16) nes_x = NES_NATIVE_W - 16;
        if (nes_y_off < -100) nes_y_off = -100;
        if (nes_y_off > 20) nes_y_off = 20;

        gen_x += dx; gen_y_off += dy;
        if (gen_x < 0) gen_x = 0;
        if (gen_x > GEN_NATIVE_W - 32) gen_x = GEN_NATIVE_W - 32;
        if (gen_y_off < -100) gen_y_off = -100;
        if (gen_y_off > 20) gen_y_off = 20;

        snes_sx += dx; snes_sy_off += dy;
        if (snes_sx < 0) snes_sx = 0;
        if (snes_sx > SNES_VW - SOLDIER_PW) snes_sx = SNES_VW - SOLDIER_PW;
        if (snes_sy_off < -60) snes_sy_off = -60;
        if (snes_sy_off > 20) snes_sy_off = 20;

        uint32_t now = timer_read();
        if (moving && ticks_to_ms(timer_elapsed(anim_t, now)) > 100) {
            gb_frame = (gb_frame + 1) % CELEBI_FRAMES;
            nes_frame = (nes_frame + 1) % 8;
            gen_frame = (gen_frame + 1) % GEN_FRAMES;
            snes_frame = (snes_frame + 1) % SOLDIER_FRAMES;
            anim_t = now;
        }

        /* ============================================================ */
        /* RENDER (timed) — skip if nothing changed                      */
        /* ============================================================ */
        static int needs_render = 2;  /* render first 2 frames (double buffer) */
        if (moving) needs_render = 2;

        uint32_t render_t0 = pmu_cycles();
        uint32_t render_us = 0;

        if (needs_render > 0) {
        needs_render--;

        /* 1) Genesis → VI0 + UI0 */
        int gen_y = 22 * 8 - 56 + gen_y_off;
        gen_sprites[0] = (genesis_sprite_t){
            .x = gen_x, .y = gen_y,
            .tile = gen_frame * GEN_TPF,
            .w = 4, .h = 4, .pal = 0,
            .fliph = gen_flip, .flipv = 0, .enabled = 1,
        };
        gen_sprites[1] = (genesis_sprite_t){
            .x = gen_x, .y = gen_y + 32,
            .tile = gen_frame * GEN_TPF + 16,
            .w = 4, .h = 3, .pal = 0,
            .fliph = gen_flip, .flipv = 0, .enabled = 1,
        };
        memset((void *)ovl, 0, LCD_FB_BYTES);
        genesis_render((uint32_t *)fb, (uint32_t *)ovl, LCD_W, LCD_H,
                       0xFF1A1A3A, &gen_plane_a, &gen_plane_b, NULL,
                       gen_sprites, 2, 1);

        /* 2) GB → composite → blit to VI0 left */
        {
            int spr_y = 15 * 8 - CELEBI_PH + gb_y_off;
            int num_oam = 0;
            const uint8_t *chr = &celebi_chr[gb_frame * CELEBI_TPF * 16];
            for (int ty = 0; ty < CELEBI_TH && num_oam < GB_MAX_SPRITES; ty++)
                for (int tx = 0; tx < CELEBI_TW && num_oam < GB_MAX_SPRITES; tx++) {
                    int ox = gb_flip ? (CELEBI_TW - 1 - tx) * 8 : tx * 8;
                    gb_oam[num_oam++] = (gb_oam_entry_t){
                        .y = (uint8_t)(spr_y + ty * 8 + 16),
                        .x = (uint8_t)(gb_x + ox + 8),
                        .tile = (uint8_t)(ty * CELEBI_TW + tx),
                        .attr = GB_SPR_PAL(0) | (gb_flip ? GB_SPR_HFLIP : 0),
                    };
                }
            memset(gb_ovl, 0, sizeof(gb_ovl));
            gb_render(gb_fb, gb_ovl, GB_NATIVE_W, GB_NATIVE_H,
                      &gb_bg, chr, gb_spr_pal, gb_oam, num_oam, 0);
            composite(gb_fb, gb_ovl, GB_NATIVE_W * GB_NATIVE_H);
            blit(fb, gb_fb, GB_NATIVE_W, GB_NATIVE_H, GB_X, GB_Y);
        }

        /* 3) NES → composite → blit to VI0 right */
        {
            int spr_y = 25 * 8 - 24 + nes_y_off;
            int num_oam = 0;
            const uint8_t *chr = &vinci_spr_chr[nes_frame * 6 * 16];
            for (int ty = 0; ty < 3 && num_oam < NES_MAX_SPRITES; ty++)
                for (int tx = 0; tx < 2 && num_oam < NES_MAX_SPRITES; tx++) {
                    int ox = nes_flip ? (1 - tx) * 8 : tx * 8;
                    nes_oam[num_oam++] = (nes_oam_entry_t){
                        .y = (uint8_t)(spr_y + ty * 8 - 1),
                        .tile = (uint8_t)(ty * 2 + tx),
                        .attr = NES_SPR_PAL(0) | (nes_flip ? NES_SPR_HFLIP : 0),
                        .x = (uint8_t)(nes_x + ox),
                    };
                }
            memset(nes_ovl, 0, sizeof(nes_ovl));
            nes_render(nes_fb, nes_ovl, NES_NATIVE_W, NES_NATIVE_H,
                       &nes_bg, chr, nes_oam, num_oam, 0);
            composite(nes_fb, nes_ovl, NES_NATIVE_W * NES_NATIVE_H);
            blit(fb, nes_fb, NES_NATIVE_W, NES_NATIVE_H, NES_X, NES_Y);
        }

        /* 4) SNES → VI1 PIP */
        memset(snes_tmp, 0, sizeof(snes_tmp));
        snes_mode1_render(snes_comp, snes_tmp, SNES_VW, SNES_VH,
                          snes_bg_pal[0], &snes_bg1, NULL, NULL, 0);
        snes_sprites[0] = (snes_sprite_t){
            .x = snes_sx, .y = 15 * 8 - SOLDIER_PH + snes_sy_off,
            .tile = snes_frame * SOLDIER_TPF,
            .w = SOLDIER_TW, .h = SOLDIER_TH,
            .pal = 0, .priority = 0,
            .fliph = snes_flip, .flipv = 0, .enabled = 1,
        };
        snes_render_sprites(snes_tmp, SNES_VW, SNES_VH,
                            soldier_chr, soldier_pal,
                            snes_sprites, 1, 0);
        composite(snes_comp, snes_tmp, SNES_VW * SNES_VH);
        memcpy((void *)SPR_ADDR, snes_comp, SNES_VW * SNES_VH * 4);
        dcache_clean_range(SPR_ADDR, SNES_VW * SNES_VH * 4);

        render_us = (pmu_cycles() - render_t0) / 1200;
        } /* end if (needs_render) */

        /* ============================================================ */
        /* MT-32 AUDIO PUMP (timed)                                      */
        /* ============================================================ */
        uint32_t audio_t0 = pmu_cycles();
        uint32_t depth_before = mix_wr - mix_rd;
        if (depth_before < stat_depth_min) stat_depth_min = depth_before;

        /* MT-32 real-time render + poll DMA refill */
        while ((mix_wr - mix_rd) < TARGET_DEPTH)
            mt32_fill();
        {
            uint64_t cur_us = (total_frames_rendered * 1000000ULL) / OUTPUT_RATE;
            smf_advance(&smf, cur_us, mt32_ctx);
            if (smf.done) {
                for (int ch = 0; ch < 16; ch++)
                    mt32emu_play_msg(mt32_ctx, 0xB0 | ch | (0x7B << 8));
                uint64_t base_us = cur_us;
                smf_init(&smf, smf_monkey_theme, smf_monkey_theme_len);
                for (uint32_t i = 0; i < smf.num_tracks; i++)
                    smf.tracks[i].next_event_us += base_us;
            }
        }
        audio_update();

        uint32_t audio_us = (pmu_cycles() - audio_t0) / 1200;

        /* Check codec hardware FIFO for underrun */
        {
            uint32_t fifos = DAC_FIFOS;
            if (fifos & 1) {           /* TXU_INT: TX FIFO underrun */
                codec_fifo_xruns++;
                DAC_FIFOS = 1;         /* w1c: clear the flag */
            }
        }

        /* ============================================================ */
        /* Flush + sync                                                  */
        /* ============================================================ */
        dcache_clean_range(fb_addr, LCD_FB_BYTES);
        dcache_clean_range(ovl_addr, LCD_FB_BYTES);
        video_wait_vblank();
        video_swap(fb_addr);
        video_set_overlay(ovl_addr);
        buf = !buf;

        uint32_t frame_us = (pmu_cycles() - frame_t0) / 1200;

        /* ============================================================ */
        /* Telemetry: once per second                                    */
        /* ============================================================ */
        stat_render_total += render_us;
        stat_audio_total += audio_us;
        stat_frame_total += frame_us;
        if (render_us > stat_render_max) stat_render_max = render_us;
        if (audio_us > stat_audio_max) stat_audio_max = audio_us;
        if (frame_us > stat_frame_max) stat_frame_max = frame_us;
        stat_frames++;
        frame_count++;

        uint32_t elapsed = ticks_to_ms(timer_elapsed(stat_timer, timer_read()));
        if (elapsed >= 1000) {
            uint32_t new_underruns = audio_underruns - stat_underruns_last;
            uint32_t depth_now = mix_wr - mix_rd;

            uart_puts("[stats] fps="); uart_putdec(stat_frames);
            uart_puts(" render="); uart_putdec(stat_render_total / stat_frames);
            uart_puts("/"); uart_putdec(stat_render_max);
            uart_puts("us audio="); uart_putdec(stat_audio_total / stat_frames);
            uart_puts("/"); uart_putdec(stat_audio_max);
            uart_puts("us frame="); uart_putdec(stat_frame_total / stat_frames);
            uart_puts("/"); uart_putdec(stat_frame_max);
            uart_puts("us buf="); uart_putdec(depth_now);
            uart_puts(" min="); uart_putdec(stat_depth_min);
            uart_puts(" xrun="); uart_putdec(new_underruns);
            uart_puts(" peak="); uart_putdec(peak_sample);
            uart_puts(" clip="); uart_putdec(clip_count);
            uart_puts(" glitch="); uart_putdec(glitch_count);
            uart_puts(" gdelta="); uart_putdec(glitch_max_delta);
            uart_puts(" drop="); uart_putdec(drop_count);
            uart_puts(" hwxrun="); uart_putdec(codec_fifo_xruns);
            uart_puts(" dma="); uart_putdec(audio_transitions);
            uart_puts(" t="); uart_putdec(frame_count / 60);
            uart_puts("s\n");
            peak_sample = 0;
            clip_count = 0;
            glitch_count = 0;
            glitch_max_delta = 0;
            drop_count = 0;

            stat_timer = timer_read();
            stat_frames = 0;
            stat_render_total = stat_audio_total = stat_frame_total = 0;
            stat_render_max = stat_audio_max = stat_frame_max = 0;
            stat_depth_min = TARGET_DEPTH;
            stat_underruns_last = audio_underruns;
        }
    }
    return 0;
}
