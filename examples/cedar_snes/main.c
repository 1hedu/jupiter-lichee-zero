/*
 * SNES PPU Demo — FF6 Magitek Armor Soldier
 *
 * CedarVE encode→decode pipeline + SNES 4bpp sprite renderer.
 * Raw ARGB embedded → H.264 encode on-device → decode → SNES tiles.
 *
 * Build: make GAME=examples/cedar_snes/main.c
 */
#include "jupiter.h"
#include "snes.h"
#include "pmu.h"
#include <string.h>

#include "soldier_thumb.h"
#include "../../tools/h264_template_448x928.h"

extern const uint8_t _binary_tools_ff6soldier_argb_start[];
extern const uint8_t _binary_tools_ff6soldier_argb_end[];

#define SHEET_W  448   /* padded to 16-aligned */
#define SHEET_H  928
#define SHEET_ARGB_ADDR 0x43400000
static uint32_t *sheet = (uint32_t *)SHEET_ARGB_ADDR;

/* Source frame layout: row 4 (Magitek), y=139, h=40, first 4 frames */
#define ANIM_FRAMES  4
#define SRC_PW  30
#define SRC_PH  40
static const int src_fx[ANIM_FRAMES] = { 3, 43, 83, 123 };
static const int src_fy = 139;
#define SRC_SHEET_W 438
#define SRC_SHEET_H 920

/* Metasprite: 3x scale */
#define SCALE 3
#define META_PW (SRC_PW * SCALE)
#define META_PH (SRC_PH * SCALE)
#define META_TW ((META_PW + 7) / 8)
#define META_TH ((META_PH + 7) / 8)
#define META_TPF (META_TW * META_TH)

static uint8_t meta_chr[ANIM_FRAMES * META_TPF * 32];
static uint32_t spr_palette[128];
static uint32_t sheet_bg_color;

static int color_near(uint32_t a, uint32_t b, int t) {
    int dr=(int)((a>>16)&0xFF)-(int)((b>>16)&0xFF);
    int dg=(int)((a>>8)&0xFF)-(int)((b>>8)&0xFF);
    int db=(int)(a&0xFF)-(int)(b&0xFF);
    return dr*dr+dg*dg+db*db < t*t;
}

/* Extract up to 15 non-bg colors from a region */
static int extract_palette(const uint32_t *buf, int bw,
                            int sx, int sy, int sw, int sh,
                            uint32_t *pal)
{
    int count = 0;
    pal[0] = 0x00000000;
    for (int y = sy; y < sy+sh; y++)
        for (int x = sx; x < sx+sw; x++) {
            uint32_t c = buf[y*bw+x] | 0xFF000000;
            if (color_near(c, sheet_bg_color, 40)) continue;
            int dup = 0;
            for (int i = 0; i <= count; i++)
                if (color_near(c, pal[i], 20)) { dup=1; break; }
            if (!dup && count < 15) pal[++count] = c;
        }
    return count + 1;
}

static int nearest_pal(uint32_t c, const uint32_t *pal, int sz) {
    if (color_near(c|0xFF000000, sheet_bg_color, 40)) return 0;
    c |= 0xFF000000;
    int best=0, bd=999999;
    for (int i=1; i<sz; i++) {
        int dr=(int)((c>>16)&0xFF)-(int)((pal[i]>>16)&0xFF);
        int dg=(int)((c>>8)&0xFF)-(int)((pal[i]>>8)&0xFF);
        int db=(int)(c&0xFF)-(int)(pal[i]&0xFF);
        int d=dr*dr+dg*dg+db*db;
        if (d<bd) { bd=d; best=i; }
    }
    return best;
}

/* Scale 3x + convert to SNES 4bpp tiles (column-major) */
static void process_frame(int f, int pal_sz)
{
    int fx = src_fx[f];
    static uint8_t ci[META_PW * META_PH];

    for (int y = 0; y < META_PH; y++) {
        int sy = src_fy + y / SCALE;
        for (int x = 0; x < META_PW; x++) {
            int sx = fx + x / SCALE;
            if (sx >= fx + SRC_PW || sy >= src_fy + SRC_PH) { ci[y*META_PW+x]=0; continue; }
            ci[y*META_PW+x] = nearest_pal(sheet[sy*SHEET_W+sx], spr_palette, pal_sz);
        }
    }

    uint8_t *out = &meta_chr[f * META_TPF * 32];
    for (int tx = 0; tx < META_TW; tx++)
        for (int ty = 0; ty < META_TH; ty++) {
            uint8_t *tile = out + (tx*META_TH+ty)*32;
            for (int r = 0; r < 8; r++) {
                uint8_t b0=0,b1=0,b2=0,b3=0;
                for (int c = 0; c < 8; c++) {
                    int px=tx*8+c, py=ty*8+r;
                    uint8_t v = (px<META_PW && py<META_PH) ? ci[py*META_PW+px] : 0;
                    b0|=((v&1)<<(7-c)); b1|=(((v>>1)&1)<<(7-c));
                    b2|=(((v>>2)&1)<<(7-c)); b3|=(((v>>3)&1)<<(7-c));
                }
                tile[r*2]=b0; tile[r*2+1]=b1; tile[r*2+16]=b2; tile[r*2+17]=b3;
            }
        }
}

static uint8_t bg_chr[256 * 32];
static uint16_t bg_map[32 * 32];
static uint32_t bg_palette[128];

/* ---- BG tile builder — cold night snowfield ---- */
static int bg_tile_n = 1;   /* tile 0 = transparent (backdrop) */

static int hexval(char c) { return c <= '9' ? c - '0' : (c & 0xDF) - 'A' + 10; }

/* Pack one 8x8 tile from 8 strings of hex digits ('0'-'F').
 * The BG compositor reads 4bpp as packed nibbles (2px/byte,
 * 4 bytes/row, high nibble = left pixel) — unlike the planar
 * sprite path. */
static int bg_tile(const char *rows[8])
{
    uint8_t *t = &bg_chr[bg_tile_n * 32];
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 4; c++)
            t[r * 4 + c] = (uint8_t)((hexval(rows[r][c * 2]) << 4) |
                                       hexval(rows[r][c * 2 + 1]));
    return bg_tile_n++;
}

static int bg_solid(char v)
{
    char row[9];
    for (int i = 0; i < 8; i++) row[i] = v;
    row[8] = 0;
    const char *rows[8] = { row, row, row, row, row, row, row, row };
    return bg_tile(rows);
}

/* Chunky 4x4-block checkerboard — cell-level dither between bands */
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
    uart_puts("  FF6 Magitek — CedarVE + SNES 4bpp\n");
    uart_puts("========================================\n\n");

    /* ---- Embed raw ARGB ---- */
    uint32_t argb_sz = (uint32_t)(_binary_tools_ff6soldier_argb_end -
                                   _binary_tools_ff6soldier_argb_start);
    memcpy(sheet, _binary_tools_ff6soldier_argb_start, argb_sz);
    sheet_bg_color = sheet[0] | 0xFF000000;
    uart_puts("[main] raw: "); uart_putdec(argb_sz/1024); uart_puts("KB bg=0x");
    uart_puthex(sheet_bg_color); uart_puts("\n");

    /* ---- CedarVE encode ---- */
    cedar_argb_to_nv12(sheet, SHEET_W, SHEET_W, SHEET_H);

    int enc_sz = cedar_h264_encode(SHEET_W, SHEET_H, 10);
    if (enc_sz > 0) {
        uart_puts("[main] encoded: "); uart_putdec(argb_sz/1024);
        uart_puts("KB → "); uart_putdec(enc_sz/1024); uart_puts("KB\n");

        /* ---- CedarVE decode ---- */
        /* Decode the on-device encoded bitstream.
         * SPS/PPS/slice header were written by put_bits with:
         * pic_init_qp=QP(10), slice_qp_delta=0, chroma_qp_off=0,
         * deblocking disabled(1). Header bit size: count from NAL start
         * through slice header fields. Our write_slice_header writes:
         * start_code(32) + NAL(8) + first_mb(1) + slice_type_I(3) +
         * pps_id(1) + frame_num(8) + idr_pic_id(1) + poc_lsb(8) +
         * no_output(1) + long_term(1) + qp_delta(1) + deblk(3) = 68 bits */
        int rc = cedar_h264_decode((const uint8_t *)cedar_enc_stream_addr(),
                                   enc_sz, SHEET_W, SHEET_H, 36, 10, 0, 0, 1);
        if (rc == 0) {
            /* decode invalidates its output buffers internally */
            cedar_nv12_to_argb(sheet, SHEET_W, SHEET_W, SHEET_H);
            uart_puts("[main] ROUND-TRIP COMPLETE!\n");
        } else {
            uart_puts("[main] decode fail\n");
            memcpy(sheet, _binary_tools_ff6soldier_argb_start, argb_sz);
        }
    } else {
        uart_puts("[main] encode fail — raw fallback\n");
    }

    /* ---- Extract palette + process frames ---- */
    memset(spr_palette, 0, sizeof(spr_palette));
    int pal_sz = 1;
    for (int f = 0; f < ANIM_FRAMES; f++)
        pal_sz = extract_palette(sheet, SHEET_W, src_fx[f], src_fy, SRC_PW, SRC_PH, spr_palette);
    uart_puts("[main] palette: "); uart_putdec(pal_sz); uart_puts(" colors\n");


    for (int f = 0; f < ANIM_FRAMES; f++) process_frame(f, pal_sz);
    uart_puts("[main] "); uart_putdec(META_TPF); uart_puts(" tiles/frame\n");

    /* ---- BG: cold night snowfield (Magitek march) ---- */
    memset(bg_chr, 0, sizeof(bg_chr));
    memset(bg_palette, 0, sizeof(bg_palette));
    static const uint32_t night_pal[12] = {
        0xFF10142E,  /* 0  backdrop / transparent — deep indigo */
        0xFF10142E,  /* 1  sky, deepest indigo                  */
        0xFF161E3E,  /* 2  sky, indigo                          */
        0xFF20304E,  /* 3  sky, steel horizon                   */
        0xFF0A0D1E,  /* 4  mountain silhouette                  */
        0xFF3A4E6E,  /* 5  snow, darkest (bottom)               */
        0xFF546E90,  /* 6  snow, dark                           */
        0xFF7690AE,  /* 7  snow, mid steel blue                 */
        0xFF9CB6D0,  /* 8  snow, moonlit pale                   */
        0xFFC8D8EE,  /* 9  snow, pale                           */
        0xFFF2F8FF,  /* A  moon / snow caps / glints            */
        0xFF66789A,  /* B  dim star                             */
    };
    for (int i = 0; i < 12; i++) bg_palette[i] = night_pal[i];

    /* Solids + chunky dither checkers */
    int t_sky2 = bg_solid('2'), t_sky3 = bg_solid('3'), t_mtn = bg_solid('4');
    int t_g9 = bg_solid('9'), t_g8 = bg_solid('8'), t_g7 = bg_solid('7');
    int t_g6 = bg_solid('6'), t_g5 = bg_solid('5');
    int t_ck12 = bg_checker('1', '2'), t_ck23 = bg_checker('2', '3');
    int t_ck49 = bg_checker('4', '9');                 /* mountain base scatter */
    int t_ck98 = bg_checker('9', '8'), t_ck87 = bg_checker('8', '7');
    int t_ck76 = bg_checker('7', '6'), t_ck65 = bg_checker('6', '5');

    /* Sparse stars + drifting flakes */
    int t_star1 = bg_tile((const char *[8]){
        "11111111", "1111A111", "111AAA11", "1111A111",
        "11111111", "11111111", "11111111", "11111111" });
    int t_flk1 = bg_tile((const char *[8]){
        "11111111", "11111111", "11B11111", "11111111",
        "11111111", "111111A1", "11111111", "11111111" });
    int t_flk2 = bg_tile((const char *[8]){
        "22222222", "22222B22", "22222222", "22222222",
        "2A222222", "22222222", "22222222", "22222222" });
    int t_flk3 = bg_tile((const char *[8]){
        "33333333", "33333333", "333B3333", "33333333",
        "33333333", "33333A33", "33333333", "33333333" });

    /* Moon quarter — one tile, other quadrants via SNES flip bits */
    int t_moon = bg_tile((const char *[8]){
        "00000AAA", "000AAAAA", "00AAAAAA", "0AAAAAAA",
        "0AAAAAAA", "AAAAAAAA", "AAAAAAAA", "AAAAAAAA" });

    /* Jagged snow-capped mountain ridge */
    int t_peak = bg_tile((const char *[8]){        /* high tip above ridge */
        "33333333", "3333A333", "333AA333", "333A4433",
        "33A44443", "33A44443", "3A444444", "A4444444" });
    int t_slope = bg_tile((const char *[8]){       /* rises to the right */
        "3333333A", "333333A4", "33333A44", "3333A444",
        "333A4444", "33A44444", "3A444444", "A4444444" });
    int t_plat = bg_tile((const char *[8]){        /* snow-capped plateau */
        "AAAAAAAA", "44444444", "44444444", "44444444",
        "44444444", "44444444", "44444444", "44444444" });
    int t_mtop = bg_tile((const char *[8]){        /* valley snow line */
        "3A3333A3", "AAAA3AAA", "44444444", "44444444",
        "44444444", "44444444", "44444444", "44444444" });

    /* Moonlit ground glints */
    int t_gl9 = bg_tile((const char *[8]){
        "99999999", "99A99999", "99999999", "99999999",
        "999999A9", "99999999", "99999999", "99999999" });
    int t_gl8 = bg_tile((const char *[8]){
        "88888888", "88888888", "888A8888", "88888888",
        "88888888", "88888A88", "88888888", "88888888" });
    int t_gl7 = bg_tile((const char *[8]){
        "77777777", "77777777", "77777777", "77877777",
        "77777777", "77777787", "77777777", "77777777" });

    /* ---- Lay out the 32x32 map ---- */
    memset(bg_map, 0, sizeof(bg_map));
    #define BGSET(x, y, t) bg_map[(y) * 32 + (x)] = SNES_ENTRY((t), 0, 0, 0, 0)
    for (int x = 0; x < 32; x++) {
        /* night sky bands, rows 0-9 (rows 0-2 = backdrop tile 0) */
        BGSET(x, 3, t_ck12);
        BGSET(x, 4, t_sky2); BGSET(x, 5, t_sky2);
        BGSET(x, 6, t_ck23);
        BGSET(x, 7, t_sky3); BGSET(x, 8, t_sky3); BGSET(x, 9, t_sky3);

        if (x % 9 == 1)  BGSET(x, (x * 3) % 3, t_star1);
        if (x % 7 == 3)  BGSET(x, 1 + (x % 2), t_flk1);
        if (x % 6 == 5)  BGSET(x, 4 + (x % 2), t_flk2);
        if (x % 8 == 6)  BGSET(x, 7 + (x % 3), t_flk3);

        /* jagged two-tier ridge (rows 10-11) over body (rows 12-13) */
        switch (x % 8) {
        case 0:  BGSET(x, 10, t_slope); BGSET(x, 11, t_mtn);  break;
        case 1:  BGSET(x, 10, t_plat);  BGSET(x, 11, t_mtn);
                 if ((x & 8) == 0) { BGSET(x, 9, t_peak); }
                 break;
        case 2:  bg_map[10 * 32 + x] = SNES_ENTRY(t_slope, 0, 0, 1, 0);
                 BGSET(x, 11, t_mtn);                          break;
        case 3:  BGSET(x, 10, t_sky3); BGSET(x, 11, t_slope); break;
        case 4:  BGSET(x, 10, t_sky3); BGSET(x, 11, t_plat);  break;
        case 5:  BGSET(x, 10, t_sky3);
                 bg_map[11 * 32 + x] = SNES_ENTRY(t_slope, 0, 0, 1, 0); break;
        case 6:  BGSET(x, 10, t_sky3); BGSET(x, 11, t_mtop);  break;
        default: BGSET(x, 10, t_sky3); BGSET(x, 11, t_slope); break;
        }
        BGSET(x, 12, t_mtn); BGSET(x, 13, t_mtn);

        /* snowfield falling darker toward the bottom, rows 14-31 */
        BGSET(x, 14, (x % 5 == 3) ? t_g9 : t_ck49);
        BGSET(x, 15, (x % 6 == 2) ? t_gl9 : t_g9);
        BGSET(x, 16, t_ck98);
        BGSET(x, 17, t_g8);
        BGSET(x, 18, (x % 7 == 4) ? t_gl8 : t_g8);
        BGSET(x, 19, t_ck87);
        BGSET(x, 20, t_g7);
        BGSET(x, 21, (x % 8 == 5) ? t_gl7 : t_g7);
        BGSET(x, 22, t_ck76);
        BGSET(x, 23, t_g6); BGSET(x, 24, t_g6);
        BGSET(x, 25, t_ck65);
        for (int y = 26; y < 32; y++) BGSET(x, y, t_g5);
    }

    /* Moon, upper left — one quarter tile + flips */
    bg_map[1 * 32 + 5] = SNES_ENTRY(t_moon, 0, 0, 0, 0);
    bg_map[1 * 32 + 6] = SNES_ENTRY(t_moon, 0, 0, 1, 0);
    bg_map[2 * 32 + 5] = SNES_ENTRY(t_moon, 0, 0, 0, 1);
    bg_map[2 * 32 + 6] = SNES_ENTRY(t_moon, 0, 0, 1, 1);
    #undef BGSET

    snes_bg_t bg1 = { .tiles=bg_chr, .map=bg_map, .palette=bg_palette,
        .scroll_x=0, .scroll_y=0, .map_w=32, .map_h=32, .bpp=4, .enabled=1 };
    /* disabled second layer — mode 1 dereferences both overlay BGs */
    snes_bg_t bg2 = { .tiles=bg_chr, .map=bg_map, .palette=bg_palette,
        .scroll_x=0, .scroll_y=0, .map_w=32, .map_h=32, .bpp=4, .enabled=0 };

    /* ---- Display ---- */
    video_init();
    volatile uint32_t *fb0=(volatile uint32_t*)FB0_ADDR;
    volatile uint32_t *fb1=(volatile uint32_t*)FB1_ADDR;
    for (uint32_t i=0;i<LCD_W*LCD_H;i++) { fb0[i]=0xFF000000; fb1[i]=0xFF000000; }
    dcache_clean_range(FB0_ADDR, LCD_FB_BYTES); dcache_clean_range(FB1_ADDR, LCD_FB_BYTES);

    /* VI1 */
    #define VIS_B 2
    video_vi1_init(LCD_W-SOLDIER_THUMB_W-VIS_B*2-4, 4,
                   SOLDIER_THUMB_W+VIS_B*2, SOLDIER_THUMB_H+VIS_B*2);
    volatile uint32_t *vi1=(volatile uint32_t*)SPR_ADDR;
    int vi1_p=SOLDIER_THUMB_W+VIS_B*2;
    for (int y=0;y<SOLDIER_THUMB_H+VIS_B*2;y++)
        for (int x=0;x<vi1_p;x++)
            vi1[y*vi1_p+x]=(x<VIS_B||x>=SOLDIER_THUMB_W+VIS_B||y<VIS_B||y>=SOLDIER_THUMB_H+VIS_B)
                ? 0xFF00FF00 : soldier_thumb[((y-VIS_B)*SOLDIER_THUMB_W)+(x-VIS_B)];
    dcache_clean_range(SPR_ADDR, vi1_p*(SOLDIER_THUMB_H+VIS_B*2)*4);

    snes_sprite_t sprites[1];
    uart_puts("[main] go!\n");

    int buf=0, frame=0, spr_x=80, spr_dx=1, flip=0;
    uint32_t last_t=timer_read();

    while (1) {
        volatile uint32_t *fb=buf?fb1:fb0;
        volatile uint32_t *ovl=(volatile uint32_t*)(buf?OVL1_ADDR:OVL_ADDR);
        uint32_t fb_addr=buf?FB1_ADDR:FB0_ADDR;
        uint32_t ovl_addr=buf?OVL1_ADDR:OVL_ADDR;

        uint32_t now=timer_read();
        if (ticks_to_ms(timer_elapsed(last_t,now))>150) { frame=(frame+1)%ANIM_FRAMES; last_t=now; }

        spr_x+=spr_dx;
        if (spr_x>SNES_NATIVE_W-META_PW-4) { spr_dx=-1; flip=1; }
        if (spr_x<4) { spr_dx=1; flip=0; }

        sprites[0]=(snes_sprite_t){ .x=spr_x, .y=24*8-META_PH,
            .tile=frame*META_TPF, .w=META_TW, .h=META_TH,
            .pal=0, .priority=0, .fliph=flip, .flipv=0, .enabled=1 };

        /* VI1 */
        { static int pf=-1; if (frame!=pf) {
            for (int y=0;y<SOLDIER_THUMB_H;y++) for (int x=0;x<SOLDIER_THUMB_W;x++)
                vi1[(y+VIS_B)*vi1_p+(x+VIS_B)]=soldier_thumb[y*SOLDIER_THUMB_W+x];
            int rx=src_fx[frame]*SOLDIER_THUMB_W/SRC_SHEET_W;
            int ry=src_fy*SOLDIER_THUMB_H/SRC_SHEET_H;
            int rw=SRC_PW*SOLDIER_THUMB_W/SRC_SHEET_W; int rh=SRC_PH*SOLDIER_THUMB_H/SRC_SHEET_H;
            if(rw<2)rw=2; if(rh<2)rh=2;
            for(int x=rx;x<rx+rw&&x<SOLDIER_THUMB_W;x++) {
                vi1[(ry+VIS_B)*vi1_p+x+VIS_B]=0xFFFF0000;
                vi1[(ry+rh-1+VIS_B)*vi1_p+x+VIS_B]=0xFFFF0000; }
            for(int y=ry;y<ry+rh&&y<SOLDIER_THUMB_H;y++) {
                vi1[(y+VIS_B)*vi1_p+rx+VIS_B]=0xFFFF0000;
                vi1[(y+VIS_B)*vi1_p+rx+rw-1+VIS_B]=0xFFFF0000; }
            dcache_clean_range(SPR_ADDR, vi1_p*(SOLDIER_THUMB_H+VIS_B*2)*4);
            pf=frame;
        }}

        memset((void*)ovl, 0, LCD_FB_BYTES);
        snes_mode1_render((uint32_t*)fb,(uint32_t*)ovl,LCD_W,LCD_H,bg_palette[0],&bg1,&bg2,NULL,1);
        snes_render_sprites((uint32_t*)ovl,LCD_W,LCD_H,meta_chr,spr_palette,sprites,1,1);

        dcache_clean_range(fb_addr, LCD_FB_BYTES);
        dcache_clean_range(ovl_addr, LCD_FB_BYTES);
        video_wait_vblank();
        video_swap(fb_addr);
        video_set_overlay(ovl_addr);
        buf=!buf;
    }
    return 0;
}
