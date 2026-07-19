/*
 * CedarVE → Scale → Genesis VDP Metasprite Demo
 *
 * Two-step pipeline:
 *   Step 1: H.264 hardware decode sprite sheet, scale frames up 3x in ARGB
 *   Step 2: Cut scaled ARGB back into 4bpp Genesis tiles → metasprite
 *           (grid of hardware sprites, the way real Genesis does large characters)
 *
 * Build: make GAME=examples/cedar_genesis/main.c
 */
#include "jupiter.h"
#include "genesis.h"
#include "pmu.h"
#include <string.h>
#include "../../tools/h264_template_512x480.h"

/* Raw ARGB sprite sheet embedded via objcopy */
extern const uint8_t _binary_tools_pulseman_argb_start[];
extern const uint8_t _binary_tools_pulseman_argb_end[];

#define SHEET_W  512
#define SHEET_H  480
#define SHEET_ARGB_ADDR 0x43400000

static uint32_t *sheet = (uint32_t *)SHEET_ARGB_ADDR;

/* ================================================================== */
/* Color/palette helpers                                                */
/* ================================================================== */
static uint32_t sheet_bg_color;

static int color_near(uint32_t a, uint32_t b, int thresh)
{
    int dr = (int)((a>>16)&0xFF) - (int)((b>>16)&0xFF);
    int dg = (int)((a>>8)&0xFF)  - (int)((b>>8)&0xFF);
    int db = (int)(a&0xFF)        - (int)(b&0xFF);
    return (dr*dr + dg*dg + db*db) < thresh * thresh;
}

static uint32_t cram[64];

static int extract_palette_from_buf(const uint32_t *buf, int w, int h,
                                     uint32_t *pal, int pal_off)
{
    int count = 0;
    pal[pal_off] = 0x00000000;
    for (int i = 0; i < w * h; i++) {
        uint32_t c = buf[i] | 0xFF000000;
        if (color_near(c, sheet_bg_color, 40)) continue;
        if ((c & 0x00FFFFFF) < 0x080808) continue;
        int found = 0;
        for (int j = 0; j <= count; j++)
            if (pal[pal_off + j] == c) { found = 1; break; }
        if (!found && count < 15) pal[pal_off + (++count)] = c;
    }
    return count + 1;
}

static int nearest_pal(uint32_t c, const uint32_t *pal, int off, int sz)
{
    if (color_near(c | 0xFF000000, sheet_bg_color, 40)) return 0;
    if ((c & 0x00FFFFFF) < 0x080808) return 0;
    c |= 0xFF000000;
    int best = 0, best_d = 999999;
    for (int i = 0; i < sz; i++) {
        int dr = (int)((c>>16)&0xFF) - (int)((pal[off+i]>>16)&0xFF);
        int dg = (int)((c>>8)&0xFF)  - (int)((pal[off+i]>>8)&0xFF);
        int db = (int)(c&0xFF)        - (int)(pal[off+i]&0xFF);
        int d = dr*dr + dg*dg + db*db;
        if (d < best_d) { best_d = d; best = i; }
    }
    return best;
}

/* ================================================================== */
/* Source frame definitions (from image analysis)                        */
/* ================================================================== */
#define ANIM_FRAMES  8
#define SRC_TH  7  /* source frame height in tiles (56px) */

static const int frame_py = 71;
static const struct { int x, pw; } src_frames[ANIM_FRAMES] = {
    {16,43}, {62,48}, {113,48}, {164,32}, {199,31}, {233,31}, {267,32}, {301,32},
};

/* ================================================================== */
/* Scaled + tiled frame storage                                         */
/* ================================================================== */
#define SCALE 3
#define MAX_TILES 3072
static uint8_t tiledata[MAX_TILES * 32];
static int tile_count = 1;  /* 0 = transparent */

/* Per-frame metasprite: array of chunk descriptors */
#define MAX_CHUNKS_PER_FRAME 40

static struct {
    int num_chunks;
    struct { int tile_base, cx, cy, cw, ch; } chunks[MAX_CHUNKS_PER_FRAME];
    int tw, th;  /* full grid size in tiles */
} meta_frames[ANIM_FRAMES];

/* Temp buffer for one 3x-scaled frame */
#define SCALED_MAX_W (48 * SCALE)
#define SCALED_MAX_H (56 * SCALE)
static uint32_t scaled_buf[SCALED_MAX_W * SCALED_MAX_H];

/* ================================================================== */
/* Step 1+2: Scale frame 3x, then cut into Genesis 4bpp tiles           */
/* ================================================================== */
static void process_frame(int f, int pal_size)
{
    int src_x = src_frames[f].x;
    int src_w = src_frames[f].pw;
    int src_h = SRC_TH * 8;
    int dst_w = src_w * SCALE;
    int dst_h = src_h * SCALE;

    /* Step 1: Scale up 3x (nearest neighbor) */
    for (int y = 0; y < dst_h; y++) {
        int sy = y / SCALE;
        for (int x = 0; x < dst_w; x++) {
            int sx = x / SCALE;
            scaled_buf[y * dst_w + x] = sheet[(frame_py + sy) * SHEET_W + (src_x + sx)];
        }
    }

    /* Step 2: Split into <=4x4-tile sprite chunks, store tiles per-chunk.
     * Each chunk gets its own column-major tile block so the renderer's
     * tile + tx * sprite.h + ty matches the storage stride. */
    int tw = (dst_w + 7) / 8;
    int th = (dst_h + 7) / 8;
    meta_frames[f].tw = tw;
    meta_frames[f].th = th;
    meta_frames[f].num_chunks = 0;

    for (int cx = 0; cx < tw; cx += 4) {
        for (int cy = 0; cy < th; cy += 4) {
            int cw = (tw - cx) > 4 ? 4 : (tw - cx);
            int ch = (th - cy) > 4 ? 4 : (th - cy);

            int chunk_idx = meta_frames[f].num_chunks++;
            meta_frames[f].chunks[chunk_idx].tile_base = tile_count;
            meta_frames[f].chunks[chunk_idx].cx = cx;
            meta_frames[f].chunks[chunk_idx].cy = cy;
            meta_frames[f].chunks[chunk_idx].cw = cw;
            meta_frames[f].chunks[chunk_idx].ch = ch;

            /* Store tiles for this chunk in column-major with stride=ch */
            for (int ltx = 0; ltx < cw; ltx++) {
                for (int lty = 0; lty < ch; lty++) {
                    if (tile_count >= MAX_TILES) return;
                    uint8_t *d = tiledata + tile_count * 32;
                    int abs_tx = cx + ltx;
                    int abs_ty = cy + lty;

                    for (int r = 0; r < 8; r++) {
                        for (int c = 0; c < 4; c++) {
                            int px = abs_tx * 8 + c * 2;
                            int py = abs_ty * 8 + r;
                            uint8_t hi = 0, lo = 0;
                            if (px < dst_w && py < dst_h)
                                hi = nearest_pal(scaled_buf[py * dst_w + px], cram, 0, pal_size);
                            if (px + 1 < dst_w && py < dst_h)
                                lo = nearest_pal(scaled_buf[py * dst_w + px + 1], cram, 0, pal_size);
                            d[r * 4 + c] = (hi << 4) | lo;
                        }
                    }
                    tile_count++;
                }
            }
        }
    }
}

/* ================================================================== */
/* Build metasprite: grid of genesis_sprite_t from tile grid             */
/* Genesis sprites max 4x4 tiles (32x32px). Split into a grid.          */
/* ================================================================== */
#define MAX_METASPRITES 40

static int build_metasprite(genesis_sprite_t *out, int frame,
                             int screen_x, int screen_y, int flip)
{
    int pw = meta_frames[frame].tw * 8;
    int n = meta_frames[frame].num_chunks;
    int count = 0;

    for (int i = 0; i < n && count < MAX_METASPRITES; i++) {
        int cx = meta_frames[frame].chunks[i].cx;
        int cy = meta_frames[frame].chunks[i].cy;
        int cw = meta_frames[frame].chunks[i].cw;
        int ch = meta_frames[frame].chunks[i].ch;
        int base = meta_frames[frame].chunks[i].tile_base;

        int sx = flip ? screen_x + (pw - (cx + cw) * 8)
                      : screen_x + cx * 8;

        out[count++] = (genesis_sprite_t){
            .x = sx,
            .y = screen_y + cy * 8,
            .tile = base,
            .w = cw, .h = ch,
            .pal = 0,
            .fliph = flip, .flipv = 0,
            .enabled = 1,
        };
    }
    return count;
}

/* ================================================================== */
/* Background tilemap — synthwave dusk (palette line 1)                 */
/* ================================================================== */
static uint16_t bg_map[64 * 32];

/* Private BG tile bank — plane B carries its own tile pointer, so BG
 * indices stay small regardless of how many sprite tiles were cut. */
#define BG_MAX_TILES 64
static uint8_t bg_tiles[BG_MAX_TILES * 32];
static int bg_tile_count = 1;   /* tile 0 = transparent (backdrop) */

/* Pack one 8x8 tile from 8 strings of hex digits '0'-'F'.
 * Genesis 4bpp: 8 rows x 4 bytes, high nibble = left pixel. */
static int hexval(char c) { return c <= '9' ? c - '0' : (c & 0xDF) - 'A' + 10; }

static int bg_tile(const char *rows[8])
{
    if (bg_tile_count >= BG_MAX_TILES) return 0;
    uint8_t *d = bg_tiles + bg_tile_count * 32;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 4; c++)
            d[r * 4 + c] = (uint8_t)((hexval(rows[r][c * 2]) << 4) |
                                       hexval(rows[r][c * 2 + 1]));
    return bg_tile_count++;
}

static int bg_solid(char v)
{
    char row[9];
    for (int i = 0; i < 8; i++) row[i] = v;
    row[8] = 0;
    const char *rows[8] = { row, row, row, row, row, row, row, row };
    return bg_tile(rows);
}

/* Chunky 4x4-block checkerboard of two shades — cell-level dither */
static int bg_checker(char a, char b)
{
    char ra[9], rb[9];
    for (int i = 0; i < 8; i++) { ra[i] = i < 4 ? a : b; rb[i] = i < 4 ? b : a; }
    ra[8] = rb[8] = 0;
    const char *rows[8] = { ra, ra, ra, ra, rb, rb, rb, rb };
    return bg_tile(rows);
}

int main(void)
{
    timer_init();
    mmu_init();
    pmu_init();
    cedar_init();

    uart_puts("\n========================================\n");
    uart_puts("  Pulseman — CedarVE Encode+Decode\n");
    uart_puts("========================================\n\n");

    /* ---- Step 1: Raw ARGB sprite sheet (embedded in binary) ---- */
    uint32_t argb_sz = (uint32_t)(_binary_tools_pulseman_argb_end -
                                   _binary_tools_pulseman_argb_start);
    uart_puts("[main] raw ARGB: "); uart_putdec(argb_sz / 1024); uart_puts("KB\n");

    /* Copy to SHEET_ARGB_ADDR for source reference */
    memcpy(sheet, _binary_tools_pulseman_argb_start, argb_sz);

    /* Detect bg color from raw ARGB BEFORE encode (encode may change it) */
    sheet_bg_color = sheet[0] | 0xFF000000;
    uart_puts("[main] bg=0x"); uart_puthex(sheet_bg_color); uart_puts("\n");

    /* Debug: sample a known-red pixel through the pipeline */
    /* Pulseman body: approximately (40, 85) in the sheet */
    #define DBG_X 24
    #define DBG_Y 104
    {
        uint32_t px = sheet[DBG_Y * SHEET_W + DBG_X];
        uart_puts("[dbg] raw ARGB @(40,85)=0x"); uart_puthex(px); uart_puts("\n");
    }

    /* ---- Step 2: ARGB → NV12 (software conversion for encoder input) ---- */
    uart_puts("[main] ARGB→NV12...\n");
    uint32_t t0 = pmu_cycles();
    cedar_argb_to_nv12(sheet, SHEET_W, SHEET_W, SHEET_H);
    uint32_t t1 = pmu_cycles();
    uart_puts("[main] converted in "); uart_putdec((t1-t0)/1200); uart_puts("us\n");

    /* ---- Step 3: H.264 ENCODE on CedarVE (AVC engine +0xB00) ---- */
    int enc_sz = cedar_h264_encode(SHEET_W, SHEET_H, 10);
    if (enc_sz < 0) {
        uart_puts("[main] ENCODE FAILED — falling back to raw ARGB\n");
        /* Fallback: use the raw ARGB directly, skip decode */
        sheet_bg_color = sheet[0] | 0xFF000000;
    } else {
        uart_puts("[main] encoded: "); uart_putdec(enc_sz);
        uart_puts("B ("); uart_putdec(argb_sz / 1024); uart_puts("KB → ");
        uart_putdec(enc_sz / 1024); uart_puts("KB)\n");

        /* ---- Step 4: H.264 DECODE the encoded bitstream ---- */
        /* NOTE: The encoder produces raw macroblock data. Our manually
         * written SPS/PPS headers may not be correct yet. Try decode,
         * fall back to raw ARGB if output is bad. */
        int rc = cedar_h264_decode((const uint8_t *)cedar_enc_stream_addr(),
                                   enc_sz, SHEET_W, SHEET_H, 36, 10, 0, 0, 1);
        if (rc == 0) {
            /* decode invalidates its output buffers internally */
            cedar_nv12_to_argb(sheet, SHEET_W, SHEET_W, SHEET_H);
            uart_puts("[main] ROUND-TRIP COMPLETE!\n");
        } else {
            uart_puts("[main] decode fail\n");
            memcpy(sheet, _binary_tools_pulseman_argb_start, argb_sz);
        }
    }

    /* ---- Extract palette from all source frames ---- */
    memset(cram, 0, sizeof(cram));
    /* Build palette from the first scaled frame (contains all colors) */
    {
        int sw = src_frames[0].pw * SCALE;
        int sh = SRC_TH * 8 * SCALE;
        for (int y = 0; y < sh; y++)
            for (int x = 0; x < sw; x++)
                scaled_buf[y * sw + x] = sheet[(frame_py + y/SCALE) * SHEET_W +
                                               (src_frames[0].x + x/SCALE)];
        extract_palette_from_buf(scaled_buf, sw, sh, cram, 0);
    }
    /* Add colors from other frames too */
    for (int f = 1; f < ANIM_FRAMES; f++) {
        int sw = src_frames[f].pw * SCALE;
        int sh = SRC_TH * 8 * SCALE;
        for (int y = 0; y < sh; y++)
            for (int x = 0; x < sw; x++)
                scaled_buf[y * sw + x] = sheet[(frame_py + y/SCALE) * SHEET_W +
                                               (src_frames[f].x + x/SCALE)];
        extract_palette_from_buf(scaled_buf, sw, sh, cram, 0);
    }

    int pal_size = 0;
    for (int i = 0; i < 16; i++) if (cram[i]) pal_size = i + 1;
    uart_puts("[main] palette: "); uart_putdec(pal_size); uart_puts(" colors\n");

    /* ---- Process all frames: scale 3x → cut into 4bpp tiles ---- */
    t0 = pmu_cycles();
    for (int f = 0; f < ANIM_FRAMES; f++)
        process_frame(f, pal_size);
    t1 = pmu_cycles();

    uart_puts("[main] tiles: "); uart_putdec(tile_count);
    uart_puts(" ("); uart_putdec((t1-t0)/1200); uart_puts("us)\n");

    for (int f = 0; f < ANIM_FRAMES; f++) {
        uart_puts("  ["); uart_putdec(f); uart_puts("] ");
        uart_putdec(meta_frames[f].tw); uart_puts("x");
        uart_putdec(meta_frames[f].th); uart_puts(" tiles, ");
        uart_putdec(meta_frames[f].num_chunks); uart_puts(" chunks\n");
    }

    /* ---- Setup Genesis VDP ---- */
    video_init();

    /* Backdrop + BG palette line 1 (cram[16..31]) — synthwave dusk */
    uint32_t bg_color = 0xFF141030;              /* deep violet-navy sky */
    static const uint32_t bg_pal[16] = {
        0xFF141030,  /* 0  transparent slot (= backdrop) */
        0xFF141030,  /* 1  sky, deepest violet-navy      */
        0xFF201646,  /* 2  sky violet                    */
        0xFF2E1E5C,  /* 3  sky lighter violet            */
        0xFF3E2670,  /* 4  dusk violet                   */
        0xFF612B82,  /* 5  dusk magenta-purple           */
        0xFF9C3390,  /* 6  glow magenta                  */
        0xFFE0518C,  /* 7  glow hot pink                 */
        0xFFFF8A50,  /* 8  glow orange                   */
        0xFFFFD08A,  /* 9  sun, pale gold                */
        0xFF0C081E,  /* A  city silhouette               */
        0xFF07060F,  /* B  grid floor near-black         */
        0xFF00E0D0,  /* C  neon cyan                     */
        0xFFFF37B8,  /* D  neon magenta                  */
        0xFFB8F4FF,  /* E  star bright                   */
        0xFF56508C,  /* F  star dim / far grid           */
    };
    for (int i = 0; i < 16; i++) cram[16 + i] = bg_pal[i];

    /* ---- Build BG tiles ---- */
    int t_sky2 = bg_solid('2'), t_sky3 = bg_solid('3');
    int t_sky4 = bg_solid('4'), t_sky5 = bg_solid('5');
    int t_ck12 = bg_checker('1', '2'), t_ck23 = bg_checker('2', '3');
    int t_ck34 = bg_checker('3', '4'), t_ck45 = bg_checker('4', '5');
    int t_sil  = bg_solid('A'), t_sun = bg_solid('9');
    int t_gap  = bg_solid('8');

    int t_star1 = bg_tile((const char *[8]){    /* bright star on sky 1 */
        "11111111", "11111111", "111E1111", "11EEE111",
        "111E1111", "11111111", "11111111", "11111111" });
    int t_star2 = bg_tile((const char *[8]){    /* dim twinkle on sky 2 */
        "22222222", "22222F22", "22222222", "22222222",
        "22F22222", "22222222", "22222222", "22222222" });
    int t_star3 = bg_tile((const char *[8]){    /* dim twinkle on sky 3 */
        "33333333", "33333333", "33333333", "333F3333",
        "33333333", "33333333", "33333F33", "33333333" });

    /* Sunset glow band — chunky 2px horizontal dither stripes */
    int t_gl56 = bg_tile((const char *[8]){
        "55555555", "55555555", "66666666", "55555555",
        "66666666", "66666666", "55555555", "66666666" });
    int t_gl67 = bg_tile((const char *[8]){
        "66666666", "66666666", "77777777", "66666666",
        "77777777", "77777777", "66666666", "77777777" });
    int t_gl78 = bg_tile((const char *[8]){
        "77777777", "77777777", "88888888", "77777777",
        "88888888", "88888888", "77777777", "88888888" });

    /* Sun slit tiles — horizontal cuts through the low sun */
    int t_sun_a = bg_tile((const char *[8]){
        "99999999", "99999999", "99999999", "99999999",
        "99999999", "77777777", "99999999", "99999999" });
    int t_sun_b = bg_tile((const char *[8]){
        "99999999", "99999999", "88888888", "99999999",
        "99999999", "88888888", "88888888", "99999999" });

    /* City skyline rooftops against the orange glow */
    int t_roof_hi = bg_tile((const char *[8]){
        "88888888", "AAAAAA88", "AAAAAA88", "AAAAAA88",
        "AAAAAAAA", "AAAAAAAA", "AAAAAAAA", "AAAAAAAA" });
    int t_roof_md = bg_tile((const char *[8]){
        "88888888", "88888888", "88888888", "88AAAAAA",
        "88AAAAAA", "AAAAAAAA", "AAAAAAAA", "AAAAAAAA" });
    int t_roof_lo = bg_tile((const char *[8]){
        "88888888", "88888888", "88888888", "88888888",
        "88888888", "88888888", "AAAAAAAA", "AAAAAAAA" });
    int t_roof_ant = bg_tile((const char *[8]){    /* antenna spire */
        "888A8888", "888A8888", "88AAA888", "888A8888",
        "888A8888", "8AAAAA88", "AAAAAAAA", "AAAAAAAA" });
    int t_win = bg_tile((const char *[8]){         /* lit windows */
        "AAAAAAAA", "AA8AAAAA", "AAAAAAAA", "AAAAACAA",
        "AAAAAAAA", "A8AAAAAA", "AAAAAAAA", "AAAAAAAA" });

    /* Neon wireframe grid floor — 3 depth bands fake the convergence */
    int t_grid_far = bg_tile((const char *[8]){    /* 4px cells, dim */
        "CCCCCCCC", "FBBBFBBB", "FBBBFBBB", "FBBBFBBB",
        "FFFFFFFF", "FBBBFBBB", "FBBBFBBB", "FBBBFBBB" });
    int t_grid_mid = bg_tile((const char *[8]){    /* 8px cells, dim verts */
        "CCCCCCCC", "FBBBBBBB", "FBBBBBBB", "FBBBBBBB",
        "FBBBBBBB", "FBBBBBBB", "FBBBBBBB", "FBBBBBBB" });
    int t_near_tl = bg_tile((const char *[8]){     /* 16px cells, 1px lines */
        "CCCCCCCC", "DBBBBBBB", "DBBBBBBB", "DBBBBBBB",
        "DBBBBBBB", "DBBBBBBB", "DBBBBBBB", "DBBBBBBB" });
    int t_near_tr = bg_tile((const char *[8]){
        "CCCCCCCC", "BBBBBBBB", "BBBBBBBB", "BBBBBBBB",
        "BBBBBBBB", "BBBBBBBB", "BBBBBBBB", "BBBBBBBB" });
    int t_near_bl = bg_tile((const char *[8]){
        "DBBBBBBB", "DBBBBBBB", "DBBBBBBB", "DBBBBBBB",
        "DBBBBBBB", "DBBBBBBB", "DBBBBBBB", "DBBBBBBB" });
    int t_floor = bg_solid('B');

    /* ---- Lay out the map (64 cols so scrolling stays safe) ---- */
    memset(bg_map, 0, sizeof(bg_map));
    #define BGSET(x, y, t) bg_map[(y) * 64 + (x)] = GEN_ENTRY((t), 1, 0, 0)
    for (int x = 0; x < 64; x++) {
        /* sky bands with cell-dither transitions (rows 0-13) */
        for (int y = 0; y <= 2; y++) BGSET(x, y, 0);   /* backdrop */
        BGSET(x, 3, t_ck12);
        BGSET(x, 4, t_sky2); BGSET(x, 5, t_sky2);
        BGSET(x, 6, t_ck23);
        BGSET(x, 7, t_sky3); BGSET(x, 8, t_sky3);
        BGSET(x, 9, t_ck34);
        BGSET(x, 10, t_sky4); BGSET(x, 11, t_sky4);
        BGSET(x, 12, t_ck45);
        BGSET(x, 13, t_sky5);

        /* sparse stars, deterministic scatter */
        if (x % 9 == 2)  BGSET(x, (x * 5 % 3), t_star1);
        if (x % 7 == 4)  BGSET(x, 4 + (x % 2), t_star2);
        if (x % 11 == 6) BGSET(x, 7 + (x % 2), t_star3);

        /* sunset glow stripes (rows 14-16) */
        BGSET(x, 14, t_gl56);
        BGSET(x, 15, t_gl67);
        BGSET(x, 16, t_gl78);

        /* skyline (rows 17-18) */
        switch ((x * 7 + 3) % 5) {
        case 0:  BGSET(x, 17, t_roof_hi);  break;
        case 1:  BGSET(x, 17, t_roof_lo);  break;
        case 2:  BGSET(x, 17, t_roof_md);  break;
        case 3:  BGSET(x, 17, (x % 3) ? t_roof_md : t_roof_ant); break;
        default: BGSET(x, 17, (x % 4) ? t_roof_hi : t_gap); break;
        }
        BGSET(x, 18, (x % 5 == 1) ? t_win : t_sil);

        /* neon grid floor (rows 19-27, feet stay on row 22) */
        BGSET(x, 19, t_grid_far);
        BGSET(x, 20, t_grid_mid);
        BGSET(x, 21, t_grid_mid);
        for (int y = 22; y < 32; y++) {
            int top  = ((y - 22) & 1) == 0;
            int left = (x & 1) == 0;
            BGSET(x, y, top ? (left ? t_near_tl : t_near_tr)
                            : (left ? t_near_bl : t_floor));
        }
    }

    /* Low stepped sun sitting on the skyline, left third of the screen */
    for (int x = 9;  x <= 10; x++) BGSET(x, 12, t_sun);
    for (int x = 8;  x <= 11; x++) BGSET(x, 13, t_sun);
    for (int x = 7;  x <= 12; x++) BGSET(x, 14, t_sun);
    for (int x = 7;  x <= 12; x++) BGSET(x, 15, t_sun_a);
    for (int x = 7;  x <= 12; x++) BGSET(x, 16, t_sun_b);
    #undef BGSET

    genesis_plane_t plane_b = {
        .tiles = bg_tiles, .map = bg_map, .cram = cram,
        .scroll_x = 0, .scroll_y = 0, .line_hscroll = NULL,
        .map_w = 64, .map_h = 32, .enabled = 1,
    };

    static uint16_t empty_map[64 * 32];
    memset(empty_map, 0, sizeof(empty_map));
    genesis_plane_t plane_a = {
        .tiles = tiledata, .map = empty_map, .cram = cram,
        .scroll_x = 0, .scroll_y = 0, .line_hscroll = NULL,
        .map_w = 64, .map_h = 32, .enabled = 1,
    };

    /* Metasprite array */
    genesis_sprite_t sprites[MAX_METASPRITES];

    /* Pre-clear framebuffers */
    volatile uint32_t *fb0 = (volatile uint32_t *)FB0_ADDR;
    volatile uint32_t *fb1 = (volatile uint32_t *)FB1_ADDR;
    for (uint32_t i = 0; i < LCD_W * LCD_H; i++) {
        fb0[i] = 0xFF000000;
        fb1[i] = 0xFF000000;
    }
    dcache_clean_range(FB0_ADDR, LCD_FB_BYTES);
    dcache_clean_range(FB1_ADDR, LCD_FB_BYTES);

    /* ---- VI1 visualizer ---- */
    #define VIS_W 160
    #define VIS_H 100
    #define VIS_B 2
    video_vi1_init(LCD_W - VIS_W - VIS_B*2 - 4, 4, VIS_W + VIS_B*2, VIS_H + VIS_B*2);
    volatile uint32_t *vi1 = (volatile uint32_t *)SPR_ADDR;
    int vi1_p = VIS_W + VIS_B * 2;

    uart_puts("[main] go!\n");

    /* ---- Animate ---- */
    int buf = 0, frame = 0, spr_x = 80, spr_dx = 1, flip = 0;
    uint32_t last_t = timer_read();

    while (1) {
        volatile uint32_t *fb  = buf ? fb1 : fb0;
        volatile uint32_t *ovl = (volatile uint32_t *)(buf ? OVL1_ADDR : OVL_ADDR);
        uint32_t fb_addr  = buf ? FB1_ADDR : FB0_ADDR;
        uint32_t ovl_addr = buf ? OVL1_ADDR : OVL_ADDR;

        uint32_t now = timer_read();
        if (ticks_to_ms(timer_elapsed(last_t, now)) > 120) {
            frame = (frame + 1) % ANIM_FRAMES;
            last_t = now;
        }

        /* Move */
        int pw = meta_frames[frame].tw * 8;
        spr_x += spr_dx;
        if (spr_x > GEN_NATIVE_W - pw - 4) { spr_dx = -2; flip = 1; }
        if (spr_x < 4) { spr_dx = 2; flip = 0; }
        int spr_y = 22 * 8 - meta_frames[frame].th * 8;  /* feet on floor (row 22) */

        /* Build metasprite for this frame */
        int num_spr = build_metasprite(sprites, frame, spr_x, spr_y, flip);

        /* Update VI1 only when animation frame changes */
        {
            static int prev_frame = -1;
            if (frame != prev_frame) {
                /* Redraw full sheet + new rectangle */
                for (int y = 0; y < VIS_H + VIS_B*2; y++)
                    for (int x = 0; x < vi1_p; x++)
                        vi1[y * vi1_p + x] = (x < VIS_B || x >= VIS_W+VIS_B ||
                                               y < VIS_B || y >= VIS_H+VIS_B)
                                              ? 0xFF00FF00 : 0xFF000000;
                for (int y = 0; y < VIS_H; y++) {
                    int sy = y * SHEET_H / VIS_H;
                    for (int x = 0; x < VIS_W; x++) {
                        int sx = x * SHEET_W / VIS_W;
                        vi1[(y+VIS_B)*vi1_p + (x+VIS_B)] = sheet[sy*SHEET_W+sx] | 0xFF000000;
                    }
                }
                int rx = src_frames[frame].x * VIS_W / SHEET_W;
                int ry = frame_py * VIS_H / SHEET_H;
                int rw = src_frames[frame].pw * VIS_W / SHEET_W;
                int rh = SRC_TH * 8 * VIS_H / SHEET_H;
                if (rw < 2) rw = 2; if (rh < 2) rh = 2;
                for (int x = rx; x < rx+rw && x < VIS_W; x++) {
                    vi1[(ry+VIS_B)*vi1_p + x+VIS_B] = 0xFFFF0000;
                    vi1[(ry+rh-1+VIS_B)*vi1_p + x+VIS_B] = 0xFFFF0000;
                }
                for (int y = ry; y < ry+rh && y < VIS_H; y++) {
                    vi1[(y+VIS_B)*vi1_p + rx+VIS_B] = 0xFFFF0000;
                    vi1[(y+VIS_B)*vi1_p + rx+rw-1+VIS_B] = 0xFFFF0000;
                }
                dcache_clean_range(SPR_ADDR, vi1_p * (VIS_H+VIS_B*2) * 4);
                prev_frame = frame;
            }
        }

        /* Render */
        memset((void *)ovl, 0, LCD_FB_BYTES);
        genesis_render((uint32_t *)fb, (uint32_t *)ovl, LCD_W, LCD_H,
                       bg_color, &plane_a, &plane_b, NULL,
                       sprites, num_spr, 1);

        dcache_clean_range(fb_addr, LCD_FB_BYTES);
        dcache_clean_range(ovl_addr, LCD_FB_BYTES);

        /* Sync everything to vblank — no tearing on any layer */
        video_wait_vblank();
        video_swap(fb_addr);
        video_set_overlay(ovl_addr);
        buf = !buf;
    }
    return 0;
}
