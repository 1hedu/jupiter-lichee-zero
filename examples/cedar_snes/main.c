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

    /* Verify NV12 data is where we think it is */
    {
        volatile uint8_t *y = (volatile uint8_t *)0x43100000;
        volatile uint8_t *uv = (volatile uint8_t *)0x43200000;
        uart_puts("[dbg] NV12@0x43100000 Y[0..7]=");
        for (int i = 0; i < 8; i++) { uart_puthex(y[i]); uart_puts(" "); }
        uart_puts("\n[dbg] NV12@0x43200000 UV[0..7]=");
        for (int i = 0; i < 8; i++) { uart_puthex(uv[i]); uart_puts(" "); }
        uart_puts("\n");
    }

    int enc_sz = cedar_h264_encode(SHEET_W, SHEET_H, 10,
                                    h264_hdr_448_928, H264_HDR_448_928_LEN);
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
        int rc = cedar_h264_decode((const uint8_t *)0x43700000, enc_sz,
                                   SHEET_W, SHEET_H, 36, 10, 0, 0, 1);
        /* Dump first 80 bytes of encoded bitstream */
        {
            dcache_invalidate_range(0x43000000, 256);
            volatile uint8_t *bs = (volatile uint8_t *)0x43000000;
            uart_puts("[dbg] bitstream[0..79]:\n  ");
            for (int i = 0; i < 80; i++) {
                uart_puthex(bs[i]); uart_puts(" ");
                if ((i & 15) == 15) { uart_puts("\n  "); }
            }
            uart_puts("\n");
        }
        if (rc == 0) {
            uint32_t dsz = ((SHEET_W+15)&~15) * ((SHEET_H+15)&~15);
            dcache_invalidate_range(0x43100000, dsz);
            dcache_invalidate_range(0x43200000, dsz/2);
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

    /* ---- BG ---- */
    memset(bg_chr, 0, sizeof(bg_chr));
    for (int r=0;r<8;r++) { bg_chr[32+r*2]=0xFF; bg_chr[32+r*2+1]=0; bg_chr[32+r*2+16]=0; bg_chr[32+r*2+17]=0; }
    for (int r=0;r<8;r++) {
        if (r<2) { bg_chr[64+r*2]=0xFF; bg_chr[64+r*2+1]=0xFF; bg_chr[64+r*2+16]=0; bg_chr[64+r*2+17]=0; }
        else { bg_chr[64+r*2]=0xFF; bg_chr[64+r*2+1]=0; bg_chr[64+r*2+16]=0; bg_chr[64+r*2+17]=0; }
    }
    memset(bg_map, 0, sizeof(bg_map));
    for (int x=0; x<32; x++) {
        bg_map[24*32+x] = SNES_ENTRY(2,0,0,0,0);
        for (int y=25;y<32;y++) bg_map[y*32+x] = SNES_ENTRY(1,0,0,0,0);
    }
    memset(bg_palette, 0, sizeof(bg_palette));
    bg_palette[0]=0xFF2040A0; bg_palette[1]=0xFF604020; bg_palette[2]=0xFF806040; bg_palette[3]=0xFF40A040;

    snes_bg_t bg1 = { .tiles=bg_chr, .map=bg_map, .palette=bg_palette,
        .scroll_x=0, .scroll_y=0, .map_w=32, .map_h=32, .bpp=4, .enabled=1 };

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
        snes_mode1_render((uint32_t*)fb,(uint32_t*)ovl,LCD_W,LCD_H,bg_palette[0],&bg1,NULL,NULL,1);
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
