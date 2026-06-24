/*
 * Jupiter HW OPN2 Input Demo — Real YM3438 + Controllers
 *
 * Overlay 2: YM3438 on PB0-PB7 + PG0-PG4, clock from CCU MCLK on PE1,
 * two N64 controllers on PE20 + PG5. VGM playback through real hardware.
 *
 * The YM3438 produces its own analog audio on MOL/MOR — no V3s codec
 * needed for FM. Video rendering runs on VI0/UI0/VI1 as usual.
 *
 * Build: make GAME=examples/opn2_hw_input/main.c
 */
#include "jupiter.h"
#include "nes.h"
#include "gb.h"
#include "genesis.h"
#include "snes.h"
#include "input.h"
#include "pmu.h"
#include <string.h>
#include "../../examples/opn2_rt/fighting_back.h"

/* Pre-converted tile data */
#include "../../examples/nes_ppu/vinci_chr.h"
#include "../../examples/gb_ppu/celebi_chr.h"
#include "../../examples/cedar_snes/soldier_chr.h"

/* Genesis: raw ARGB sprite sheet embedded via objcopy */
extern const uint8_t _binary_tools_pulseman_argb_start[];
extern const uint8_t _binary_tools_pulseman_argb_end[];

/* ================================================================== */
/* VGM parser — direct hardware writes, no software rendering          */
/* ================================================================== */

static const uint8_t *vgm_data;
static uint32_t vgm_len;
static uint32_t vgm_pos;
static uint32_t vgm_data_offset;
static uint32_t vgm_sample_rate;
static uint32_t vgm_wait_samples;
static int vgm_done;

static inline uint32_t rd32le(const uint8_t *p) {
    return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
}

static void vgm_hw_init(const uint8_t *data, uint32_t len)
{
    vgm_data = data;
    vgm_len = len;
    vgm_data_offset = rd32le(data + 0x34) + 0x34;
    vgm_sample_rate = 44100;
    vgm_pos = vgm_data_offset;
    vgm_wait_samples = 0;
    vgm_done = 0;
}

/* Process VGM commands until we hit a wait or end.
 * Returns number of samples to wait before calling again. */
static uint32_t vgm_hw_tick(void)
{
    const uint8_t *d = vgm_data;

    while (vgm_pos < vgm_len) {
        uint8_t cmd = d[vgm_pos++];

        switch (cmd) {
        /* YM2612 port 0 */
        case 0x52: {
            uint8_t addr = d[vgm_pos++];
            uint8_t val  = d[vgm_pos++];
            ym3438_hw_vgm_write(0, addr, val);
            break;
        }
        /* YM2612 port 1 */
        case 0x53: {
            uint8_t addr = d[vgm_pos++];
            uint8_t val  = d[vgm_pos++];
            ym3438_hw_vgm_write(1, addr, val);
            break;
        }
        /* Wait N samples */
        case 0x61: {
            uint16_t n = d[vgm_pos] | (d[vgm_pos+1] << 8);
            vgm_pos += 2;
            return n;
        }
        /* Wait 735 samples (1/60 sec) */
        case 0x62:
            return 735;
        /* Wait 882 samples (1/50 sec) */
        case 0x63:
            return 882;
        /* End of sound data */
        case 0x66: {
            /* Loop: check for loop offset */
            uint32_t loop_off = rd32le(vgm_data + 0x1C);
            if (loop_off) {
                vgm_pos = loop_off + 0x1C;
            } else {
                vgm_done = 1;
                return 0;
            }
            break;
        }
        /* Wait 1-16 samples (0x7N = wait N+1) */
        default:
            if ((cmd & 0xF0) == 0x70) {
                return (cmd & 0x0F) + 1;
            }
            /* PCM data write (0x80-0x8F): YM2612 DAC + wait */
            if ((cmd & 0xF0) == 0x80) {
                /* Write DAC data to register $2A */
                uint32_t pcm_off = rd32le(vgm_data + 0x34);
                (void)pcm_off; /* TODO: PCM data block support */
                ym3438_hw_vgm_write(0, 0x2A, 0x80); /* silence for now */
                return cmd & 0x0F;
            }
            /* SN76489 (0x50): skip */
            if (cmd == 0x50) { vgm_pos++; break; }
            /* Data block (0x67): skip */
            if (cmd == 0x67) {
                vgm_pos++; /* 0x66 */
                vgm_pos++; /* type */
                uint32_t blen = rd32le(d + vgm_pos);
                vgm_pos += 4 + blen;
                break;
            }
            /* Unknown: skip */
            break;
        }
    }
    vgm_done = 1;
    return 0;
}

/* ================================================================== */
/* Layout constants                                                     */
/* ================================================================== */
#define GB_X    12
#define GB_Y    64
#define NES_X   188
#define NES_Y   24
#define SNES_VW 160
#define SNES_VH 144
#define SNES_X  312
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

static void composite(uint32_t *bg, const uint32_t *ovl, int count)
{
    for (int i = 0; i < count; i++)
        if (ovl[i] & 0xFF000000) bg[i] = ovl[i];
}

static void blit(volatile uint32_t *dst, const uint32_t *src,
                 int sw, int sh, int dx, int dy)
{
    for (int y = 0; y < sh; y++)
        memcpy((void *)&dst[(dy + y) * LCD_W + dx], &src[y * sw], sw * 4);
}

/* ================================================================== */
/* Genesis runtime ARGB→4bpp (same as trilayer demo)                    */
/* ================================================================== */
#define GEN_SHEET_W  512
#define GEN_SRC_PY   71
#define GEN_FRAMES   5
static const struct { int x, pw; } gen_src[GEN_FRAMES] = {
    {164,32}, {199,31}, {233,31}, {267,32}, {301,32},
};
#define GEN_TPF  28
static uint8_t gen_tiles[(GEN_FRAMES * GEN_TPF + 1) * 32];
static uint32_t gen_cram[64];
static uint32_t gen_bg_color;

static int gen_color_near(uint32_t a, uint32_t b) {
    int dr=(int)((a>>16)&0xFF)-(int)((b>>16)&0xFF);
    int dg=(int)((a>>8)&0xFF)-(int)((b>>8)&0xFF);
    int db=(int)(a&0xFF)-(int)(b&0xFF);
    return (dr*dr+dg*dg+db*db)<40*40;
}
static void gen_extract_palette(const uint32_t *s) {
    gen_bg_color=s[0]|0xFF000000; memset(gen_cram,0,sizeof(gen_cram)); gen_cram[0]=0;
    int c=0;
    for(int f=0;f<GEN_FRAMES&&c<15;f++){int fx=gen_src[f].x,pw=gen_src[f].pw;
    for(int y=GEN_SRC_PY;y<GEN_SRC_PY+56&&c<15;y++)for(int x=fx;x<fx+pw&&c<15;x++){
        uint32_t v=s[y*GEN_SHEET_W+x]|0xFF000000;if(gen_color_near(v,gen_bg_color))continue;
        if((v&0xFFFFFF)<0x080808)continue;int d=0;for(int i=0;i<=c;i++)if(gen_color_near(v,gen_cram[i])){d=1;break;}
        if(!d)gen_cram[++c]=v;}}
}
static int gen_nearest(uint32_t c) {
    if(gen_color_near(c|0xFF000000,gen_bg_color))return 0;if((c&0xFFFFFF)<0x080808)return 0;
    c|=0xFF000000;int b=0,bd=999999;for(int i=1;i<16;i++){if(!gen_cram[i])continue;
    int dr=(int)((c>>16)&0xFF)-(int)((gen_cram[i]>>16)&0xFF);
    int dg=(int)((c>>8)&0xFF)-(int)((gen_cram[i]>>8)&0xFF);
    int db=(int)(c&0xFF)-(int)(gen_cram[i]&0xFF);int d=dr*dr+dg*dg+db*db;
    if(d<bd){bd=d;b=i;}}return b;
}
static void gen_convert_tile(uint8_t *out,const uint32_t *s,int fx,int tpx,int tpy,int pw) {
    for(int r=0;r<8;r++)for(int c=0;c<4;c++){int p0=tpx+c*2,p1=p0+1,py=tpy+r;
    uint8_t hi=0,lo=0;if(p0<pw&&py<56)hi=gen_nearest(s[(GEN_SRC_PY+py)*GEN_SHEET_W+fx+p0]);
    if(p1<pw&&py<56)lo=gen_nearest(s[(GEN_SRC_PY+py)*GEN_SHEET_W+fx+p1]);out[r*4+c]=(hi<<4)|lo;}
}
static void gen_convert_frames(const uint32_t *s) {
    for(int f=0;f<GEN_FRAMES;f++){int fx=gen_src[f].x,pw=gen_src[f].pw;
    uint8_t *base=&gen_tiles[f*GEN_TPF*32];
    for(int tx=0;tx<4;tx++)for(int ty=0;ty<4;ty++)gen_convert_tile(&base[(tx*4+ty)*32],s,fx,tx*8,ty*8,pw);
    for(int tx=0;tx<4;tx++)for(int ty=0;ty<3;ty++)gen_convert_tile(&base[(16+tx*3+ty)*32],s,fx,tx*8,(4+ty)*8,pw);}
}

/* ================================================================== */
/* System state                                                         */
/* ================================================================== */
static uint8_t gb_chr[256*16], gb_map[GB_MAP_SIZE];
static uint32_t gb_palette[32], gb_spr_pal[32];
static gb_oam_entry_t gb_oam[GB_MAX_SPRITES];
static uint8_t nes_chr[256*16], nes_palette_ram[32], nes_nametable[NES_NT_TILES], nes_attribute[NES_NT_ATTRS];
static nes_oam_entry_t nes_oam[NES_MAX_SPRITES];
static uint16_t gen_bg_map[64*32];
static genesis_sprite_t gen_sprites[2];
static uint8_t snes_bg_chr[256*32];
static uint16_t snes_bg_map[32*32];
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
    uart_puts("  Jupiter HW OPN2 Input Demo\n");
    uart_puts("  Real YM3438 + N64 controllers\n");
    uart_puts("========================================\n\n");

    /* ---- N64 controller (Overlay 2: PE20) ---- */
    input_init(INPUT_N64);

    /* NOTE: YM3438 init is AFTER video_init() below because
     * video_init() overwrites PE_CFG0, killing the MCLK output on PE1. */
    /* TODO: second N64 on PG5 */

    /* ---- Genesis tile conversion ----
     * Scratch in VRAM at 0x43D00000 (safe in any binary size; cedar's
     * region starts at 0x43E00000 leaving 1 MB here for the 960 KB
     * ARGB sheet). Was 0x43400000 which is inside the CODE bss
     * region — collides with menu-built BSS once it grows past. */
    uint32_t *gen_sheet = (uint32_t *)0x43D00000;
    uint32_t gen_sz = (uint32_t)(_binary_tools_pulseman_argb_end -
                                  _binary_tools_pulseman_argb_start);
    memcpy(gen_sheet, _binary_tools_pulseman_argb_start, gen_sz);
    gen_extract_palette(gen_sheet);
    gen_convert_frames(gen_sheet);

    /* ---- GB/NES/SNES/Genesis tile setup (same as trilayer) ---- */
    memset(gb_chr,0,sizeof(gb_chr));
    for(int r=0;r<8;r++){gb_chr[1*16+r]=(r<3)?0xAA:0xFF;gb_chr[1*16+r+8]=(r<3)?0x55:0x00;}
    for(int r=0;r<8;r++){gb_chr[2*16+r]=0xFF;gb_chr[2*16+r+8]=0x00;}
    memset(gb_map,0,sizeof(gb_map));
    for(int x=0;x<GB_MAP_W;x++){gb_map[15*GB_MAP_W+x]=1;for(int y=16;y<GB_MAP_H;y++)gb_map[y*GB_MAP_W+x]=2;}
    gb_palette[0]=0xFF88CC88;gb_palette[1]=0xFF306830;gb_palette[2]=0xFF58A858;gb_palette[3]=0xFF98D898;
    gb_spr_pal[0]=0;gb_spr_pal[1]=celebi_pal[1];gb_spr_pal[2]=celebi_pal[2];gb_spr_pal[3]=celebi_pal[3];

    memset(nes_chr,0,sizeof(nes_chr));
    for(int r=0;r<8;r++){nes_chr[1*16+r]=0xFF;nes_chr[1*16+r+8]=0x00;}
    for(int r=0;r<8;r++){if(r<1){nes_chr[2*16+r]=0xFF;nes_chr[2*16+r+8]=0xFF;}
    else if(r<3){nes_chr[2*16+r]=(r&1)?0xAA:0x55;nes_chr[2*16+r+8]=0xFF;}
    else{nes_chr[2*16+r]=0xFF;nes_chr[2*16+r+8]=0x00;}}
    for(int r=0;r<8;r++){if(r==0||r==4){nes_chr[3*16+r]=0x00;nes_chr[3*16+r+8]=0xFF;}
    else if(r<4){nes_chr[3*16+r]=0xEF;nes_chr[3*16+r+8]=0x00;}
    else{nes_chr[3*16+r]=0xFE;nes_chr[3*16+r+8]=0x00;}}
    memset(nes_nametable,0,sizeof(nes_nametable));
    for(int x=0;x<NES_NT_W;x++){nes_nametable[25*NES_NT_W+x]=2;for(int y=26;y<NES_NT_H;y++)nes_nametable[y*NES_NT_W+x]=3;}
    memset(nes_attribute,0,sizeof(nes_attribute));
    for(int ax=0;ax<8;ax++){nes_attribute[6*8+ax]=NES_ATTR(0,0,1,1);nes_attribute[7*8+ax]=NES_ATTR(1,1,1,1);}
    memset(nes_palette_ram,0x0D,sizeof(nes_palette_ram));
    nes_palette_ram[0]=vinci_bg_color;
    nes_palette_ram[1]=0x02;nes_palette_ram[2]=0x12;nes_palette_ram[3]=0x21;
    nes_palette_ram[5]=0x17;nes_palette_ram[6]=0x27;nes_palette_ram[7]=0x37;
    nes_palette_ram[17]=vinci_spr_palette[1];nes_palette_ram[18]=vinci_spr_palette[2];nes_palette_ram[19]=vinci_spr_palette[3];

    {int ft=GEN_FRAMES*GEN_TPF;uint8_t *d=&gen_tiles[ft*32];
    for(int r=0;r<8;r++)for(int c=0;c<4;c++)d[r*4+c]=(r<2)?0x11:0x00;
    gen_cram[16]=0xFF1A1A3A;gen_cram[17]=0xFF2A2A5A;
    memset(gen_bg_map,0,sizeof(gen_bg_map));
    for(int y=0;y<28;y++)for(int x=0;x<40;x++)if(y>=22)gen_bg_map[y*64+x]=GEN_ENTRY(ft,1,0,0);}

    genesis_plane_t gen_plane_b={.tiles=gen_tiles,.map=gen_bg_map,.cram=gen_cram,
        .scroll_x=0,.scroll_y=0,.line_hscroll=NULL,.map_w=64,.map_h=32,.enabled=1};
    static uint16_t gen_empty_map[64*32];memset(gen_empty_map,0,sizeof(gen_empty_map));
    genesis_plane_t gen_plane_a={.tiles=gen_tiles,.map=gen_empty_map,.cram=gen_cram,
        .scroll_x=0,.scroll_y=0,.line_hscroll=NULL,.map_w=64,.map_h=32,.enabled=1};

    memset(snes_bg_chr,0,sizeof(snes_bg_chr));
    for(int r=0;r<8;r++){snes_bg_chr[32+r*2]=0xFF;snes_bg_chr[32+r*2+1]=0;snes_bg_chr[32+r*2+16]=0;snes_bg_chr[32+r*2+17]=0;}
    for(int r=0;r<8;r++){if(r<2){snes_bg_chr[64+r*2]=0xFF;snes_bg_chr[64+r*2+1]=0xFF;snes_bg_chr[64+r*2+16]=0;snes_bg_chr[64+r*2+17]=0;}
    else{snes_bg_chr[64+r*2]=0xFF;snes_bg_chr[64+r*2+1]=0;snes_bg_chr[64+r*2+16]=0;snes_bg_chr[64+r*2+17]=0;}}
    memset(snes_bg_map,0,sizeof(snes_bg_map));
    for(int x=0;x<32;x++){snes_bg_map[15*32+x]=SNES_ENTRY(2,0,0,0,0);for(int y=16;y<32;y++)snes_bg_map[y*32+x]=SNES_ENTRY(1,0,0,0,0);}
    memset(snes_bg_pal,0,sizeof(snes_bg_pal));
    snes_bg_pal[0]=0xFF2040A0;snes_bg_pal[1]=0xFF604020;snes_bg_pal[2]=0xFF806040;snes_bg_pal[3]=0xFF40A040;
    snes_bg_t snes_bg1={.tiles=snes_bg_chr,.map=snes_bg_map,.palette=snes_bg_pal,
        .scroll_x=0,.scroll_y=0,.map_w=32,.map_h=32,.bpp=4,.enabled=1};

    /* ---- Display ---- */
    video_init();
    volatile uint32_t *fb0=(volatile uint32_t*)FB0_ADDR;
    volatile uint32_t *fb1=(volatile uint32_t*)FB1_ADDR;
    for(uint32_t i=0;i<LCD_W*LCD_H;i++){fb0[i]=0xFF000000;fb1[i]=0xFF000000;}
    dcache_clean_range(FB0_ADDR,LCD_FB_BYTES);dcache_clean_range(FB1_ADDR,LCD_FB_BYTES);
    video_vi1_init(SNES_X,SNES_Y,SNES_VW,SNES_VH);

    /* ---- YM3438 init AFTER video_init (which clobbers PE_CFG0) ---- */
    ym3438_hw_set_clock(YM_CLK_CCU_MCLK);
    ym3438_hw_init();
    vgm_hw_init(vgm_fighting_back, vgm_fighting_back_len);
    uart_puts("[vgm] Fighting Back loaded, hardware playback\n");

    nes_bg_t nes_bg={.chr=nes_chr,.nametable=nes_nametable,.attribute=nes_attribute,
        .palette_ram=nes_palette_ram,.scroll_x=0,.scroll_y=0,.line_scroll_x=NULL,.enabled=1};
    gb_bg_t gb_bg={.chr=gb_chr,.map=gb_map,.map_attr=NULL,
        .palette=gb_palette,.scroll_x=0,.scroll_y=0,.enabled=1};

    uart_puts("[main] go!\n");

    /* ---- State ---- */
    int buf=0;
    int gb_frame=0,gb_x=60,gb_y_off=0,gb_flip=0;
    int nes_frame=0,nes_x=120,nes_y_off=0,nes_flip=0;
    int gen_frame=0,gen_x=140,gen_y_off=0,gen_flip=0;
    int snes_frame=0,snes_sx=70,snes_sy_off=0,snes_flip=0;
    uint32_t anim_t=timer_read();
    int moving=0;
    static int needs_render=2;
    uint32_t vgm_accum=0; /* accumulated wait samples */
    uint32_t frame_count=0,stat_timer=timer_read();

    while (1) {
        volatile uint32_t *fb=buf?fb1:fb0;
        volatile uint32_t *ovl=(volatile uint32_t*)(buf?OVL1_ADDR:OVL_ADDR);
        uint32_t fb_addr=buf?FB1_ADDR:FB0_ADDR;
        uint32_t ovl_addr=buf?OVL1_ADDR:OVL_ADDR;

        /* ---- VGM hardware playback: process ~1 frame of commands ---- */
        if (!vgm_done) {
            /* Process commands until we've waited ~735 samples (1/60s) */
            while (vgm_accum < 735) {
                uint32_t wait = vgm_hw_tick();
                if (vgm_done) break;
                vgm_accum += wait;
            }
            vgm_accum -= 735;
        }

        /* ---- Input (N64 only in Overlay 2) ---- */
        input_state_t n64_pad = input_poll_n64();
        uint32_t btn = n64_pad.buttons;

        int dx=0, dy=0;
        if (btn & BTN_LEFT)  dx=-2;
        if (btn & BTN_RIGHT) dx=2;
        if (btn & BTN_UP)    dy=-2;
        if (btn & BTN_DOWN)  dy=2;
        if (n64_pad.stick_x > 20) dx = n64_pad.stick_x / 25;
        else if (n64_pad.stick_x < -20) dx = n64_pad.stick_x / 25;
        if (n64_pad.stick_y > 20) dy = -n64_pad.stick_y / 25;
        else if (n64_pad.stick_y < -20) dy = -n64_pad.stick_y / 25;

        if (dx<0){gb_flip=1;nes_flip=1;gen_flip=1;snes_flip=1;}
        if (dx>0){gb_flip=0;nes_flip=0;gen_flip=0;snes_flip=0;}
        moving=(dx!=0||dy!=0);
        if (moving) needs_render=2;

        gb_x+=dx;gb_y_off+=dy;
        if(gb_x<0)gb_x=0;if(gb_x>GB_NATIVE_W-CELEBI_PW)gb_x=GB_NATIVE_W-CELEBI_PW;
        if(gb_y_off<-60)gb_y_off=-60;if(gb_y_off>20)gb_y_off=20;
        nes_x+=dx;nes_y_off+=dy;
        if(nes_x<0)nes_x=0;if(nes_x>NES_NATIVE_W-16)nes_x=NES_NATIVE_W-16;
        if(nes_y_off<-100)nes_y_off=-100;if(nes_y_off>20)nes_y_off=20;
        gen_x+=dx;gen_y_off+=dy;
        if(gen_x<0)gen_x=0;if(gen_x>GEN_NATIVE_W-32)gen_x=GEN_NATIVE_W-32;
        if(gen_y_off<-100)gen_y_off=-100;if(gen_y_off>20)gen_y_off=20;
        snes_sx+=dx;snes_sy_off+=dy;
        if(snes_sx<0)snes_sx=0;if(snes_sx>SNES_VW-SOLDIER_PW)snes_sx=SNES_VW-SOLDIER_PW;
        if(snes_sy_off<-60)snes_sy_off=-60;if(snes_sy_off>20)snes_sy_off=20;

        uint32_t now=timer_read();
        if(moving&&ticks_to_ms(timer_elapsed(anim_t,now))>100){
            gb_frame=(gb_frame+1)%CELEBI_FRAMES;nes_frame=(nes_frame+1)%8;
            gen_frame=(gen_frame+1)%GEN_FRAMES;snes_frame=(snes_frame+1)%SOLDIER_FRAMES;anim_t=now;}

        /* ---- Render (skip when idle) ---- */
        if (needs_render > 0) {
            needs_render--;
            int gen_y=22*8-56+gen_y_off;
            gen_sprites[0]=(genesis_sprite_t){.x=gen_x,.y=gen_y,.tile=gen_frame*GEN_TPF,
                .w=4,.h=4,.pal=0,.fliph=gen_flip,.flipv=0,.enabled=1};
            gen_sprites[1]=(genesis_sprite_t){.x=gen_x,.y=gen_y+32,.tile=gen_frame*GEN_TPF+16,
                .w=4,.h=3,.pal=0,.fliph=gen_flip,.flipv=0,.enabled=1};
            memset((void*)ovl,0,LCD_FB_BYTES);
            genesis_render((uint32_t*)fb,(uint32_t*)ovl,LCD_W,LCD_H,
                0xFF1A1A3A,&gen_plane_a,&gen_plane_b,NULL,gen_sprites,2,1);

            {int spr_y=15*8-CELEBI_PH+gb_y_off;int n=0;
            const uint8_t *chr=&celebi_chr[gb_frame*CELEBI_TPF*16];
            for(int ty=0;ty<CELEBI_TH&&n<GB_MAX_SPRITES;ty++)
                for(int tx=0;tx<CELEBI_TW&&n<GB_MAX_SPRITES;tx++){
                    int ox=gb_flip?(CELEBI_TW-1-tx)*8:tx*8;
                    gb_oam[n++]=(gb_oam_entry_t){.y=(uint8_t)(spr_y+ty*8+16),.x=(uint8_t)(gb_x+ox+8),
                        .tile=(uint8_t)(ty*CELEBI_TW+tx),.attr=GB_SPR_PAL(0)|(gb_flip?GB_SPR_HFLIP:0)};}
            memset(gb_ovl,0,sizeof(gb_ovl));
            gb_render(gb_fb,gb_ovl,GB_NATIVE_W,GB_NATIVE_H,&gb_bg,chr,gb_spr_pal,gb_oam,n,0);
            composite(gb_fb,gb_ovl,GB_NATIVE_W*GB_NATIVE_H);
            blit(fb,gb_fb,GB_NATIVE_W,GB_NATIVE_H,GB_X,GB_Y);}

            {int spr_y=25*8-24+nes_y_off;int n=0;
            const uint8_t *chr=&vinci_spr_chr[nes_frame*6*16];
            for(int ty=0;ty<3&&n<NES_MAX_SPRITES;ty++)
                for(int tx=0;tx<2&&n<NES_MAX_SPRITES;tx++){
                    int ox=nes_flip?(1-tx)*8:tx*8;
                    nes_oam[n++]=(nes_oam_entry_t){.y=(uint8_t)(spr_y+ty*8-1),.tile=(uint8_t)(ty*2+tx),
                        .attr=NES_SPR_PAL(0)|(nes_flip?NES_SPR_HFLIP:0),.x=(uint8_t)(nes_x+ox)};}
            memset(nes_ovl,0,sizeof(nes_ovl));
            nes_render(nes_fb,nes_ovl,NES_NATIVE_W,NES_NATIVE_H,&nes_bg,chr,nes_oam,n,0);
            composite(nes_fb,nes_ovl,NES_NATIVE_W*NES_NATIVE_H);
            blit(fb,nes_fb,NES_NATIVE_W,NES_NATIVE_H,NES_X,NES_Y);}

            memset(snes_tmp,0,sizeof(snes_tmp));
            snes_mode1_render(snes_comp,snes_tmp,SNES_VW,SNES_VH,snes_bg_pal[0],&snes_bg1,NULL,NULL,0);
            snes_sprites[0]=(snes_sprite_t){.x=snes_sx,.y=15*8-SOLDIER_PH+snes_sy_off,
                .tile=snes_frame*SOLDIER_TPF,.w=SOLDIER_TW,.h=SOLDIER_TH,
                .pal=0,.priority=0,.fliph=snes_flip,.flipv=0,.enabled=1};
            snes_render_sprites(snes_tmp,SNES_VW,SNES_VH,soldier_chr,soldier_pal,snes_sprites,1,0);
            composite(snes_comp,snes_tmp,SNES_VW*SNES_VH);
            memcpy((void*)SPR_ADDR,snes_comp,SNES_VW*SNES_VH*4);
            dcache_clean_range(SPR_ADDR,SNES_VW*SNES_VH*4);
            dcache_clean_range(fb_addr,LCD_FB_BYTES);
            dcache_clean_range(ovl_addr,LCD_FB_BYTES);
        }

        /* ---- Vblank ---- */
        video_wait_vblank();
        video_swap(fb_addr);
        video_set_overlay(ovl_addr);
        buf=!buf;
        frame_count++;

        /* Stats every 5s */
        uint32_t elapsed=ticks_to_ms(timer_elapsed(stat_timer,timer_read()));
        if(elapsed>=5000){
            uart_puts("[hw] fps=");uart_putdec(frame_count*1000/elapsed);
            uart_puts(" vgm=");uart_puts(vgm_done?"done":"playing");
            uart_puts("\n");
            stat_timer=timer_read();frame_count=0;
        }
    }
    return 0;
}
