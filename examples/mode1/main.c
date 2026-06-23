/*
 * Jupiter SDK — SNES Mode 1 Demo
 *
 * Three background layers with parallax scroll:
 *   BG1 (4bpp): City foreground — buildings, ground, bushes
 *   BG2 (4bpp): Mountains — slow parallax
 *   BG3 (2bpp): Status bar overlay (high priority)
 *
 * Build: make GAME=examples/mode1/main.c
 */
#include "jupiter.h"
#include "snes.h"

/* ================================================================
 *  TILE DATA
 * ================================================================ */

#define BG2_TILES 8
#define BG1_TILES 16
#define BG3_TILES 4
static uint8_t bg2_tiledata[BG2_TILES * 32];
static uint8_t bg1_tiledata[BG1_TILES * 32];
static uint8_t bg3_tiledata[BG3_TILES * 16];

static void set_4bpp_tile(uint8_t *base, int tidx, const uint8_t px[64])
{
    uint8_t *d = base + tidx * 32;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 4; c++)
            d[r*4+c] = (px[r*8+c*2] << 4) | px[r*8+c*2+1];
}
static void set_2bpp_tile(uint8_t *base, int tidx, const uint8_t px[64])
{
    uint8_t *d = base + tidx * 16;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 2; c++)
            d[r*2+c] = (px[r*8+c*4]<<6)|(px[r*8+c*4+1]<<4)|
                        (px[r*8+c*4+2]<<2)|px[r*8+c*4+3];
}
static void fill_tile_4bpp(uint8_t *base, int tidx, uint8_t idx)
{
    uint8_t px[64]; for (int i=0;i<64;i++) px[i]=idx;
    set_4bpp_tile(base, tidx, px);
}

static void init_tiles(void)
{
    /* BG2: mountains */
    /* 0=transparent(BSS) 1=dark mountain 2=slope-left 3=slope-right 4=distant 5=cloud */
    fill_tile_4bpp(bg2_tiledata, 1, 1);  /* solid mountain */
    fill_tile_4bpp(bg2_tiledata, 4, 4);  /* distant mountain */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=(x>=(7-y))?1:0;
      set_4bpp_tile(bg2_tiledata,2,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=(x<=y)?1:0;
      set_4bpp_tile(bg2_tiledata,3,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=(x>=(7-y))?4:0;
      set_4bpp_tile(bg2_tiledata,5,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        int cx=x-4,cy=y-3; d[y*8+x]=(cx*cx+cy*cy*2<12)?3:0;
      } set_4bpp_tile(bg2_tiledata,6,d); }

    /* BG1: city */
    /* 0=transparent 1=dirt 2=grass-top 3=brick 4=brick-cap 5=window 6=door 7=bush */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=((x+y)&3)==0?2:1;
      set_4bpp_tile(bg1_tiledata,1,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=y<2?4:y<3?3:((x+y)&3)==0?2:1;
      set_4bpp_tile(bg1_tiledata,2,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        if(y==3||y==7) d[y*8+x]=6;
        else if(y<3&&x==7) d[y*8+x]=6;
        else if(y>3&&x==3) d[y*8+x]=6;
        else d[y*8+x]=5;
      } set_4bpp_tile(bg1_tiledata,3,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=y<2?7:((y==3||y==7)?6:((y>3&&x==3)?6:5));
      set_4bpp_tile(bg1_tiledata,4,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=(y==0||y==7||x==0||x==7||x==3||y==3)?7:8;
      set_4bpp_tile(bg1_tiledata,5,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=(x==0||x==7||y==0)?7:(x==5&&y==4)?9:10;
      set_4bpp_tile(bg1_tiledata,6,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=y>=5?1:((x+y)&1)?((y<2)?11:3):3;
      set_4bpp_tile(bg1_tiledata,7,d); }

    /* BG3: status bar */
    /* 0=transparent 1=bar 2=bar+border */
    { uint8_t d[64]; for(int i=0;i<64;i++) d[i]=1;
      set_2bpp_tile(bg3_tiledata,1,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=(y==7)?2:1;
      set_2bpp_tile(bg3_tiledata,2,d); }
}

/* ================================================================
 *  PALETTES
 * ================================================================ */

static const uint32_t bg2_pal[16] = {
    0x00000000, 0xFF3A4A6A, 0xFF5A6A8A, 0xFFCCDDEE,
    0xFF5A6A9A, [5 ... 15] = 0xFF2A3A5A,
};
static const uint32_t bg1_pal[16] = {
    0x00000000, 0xFF6A5A3A, 0xFF5A4A2A, 0xFF2A7A2A,
    0xFF4ABA4A, 0xFFA06030, 0xFF807060, 0xFF909090,
    0xFF6090C0, 0xFFD0D040, 0xFF604020, 0xFFE06080,
    [12 ... 15] = 0xFF000000,
};
static const uint32_t bg3_pal[4] = {
    0x00000000, 0xE0182848, 0xFF4080C0, 0xFFA0C0E0,
};

/* ================================================================
 *  TILEMAPS
 * ================================================================ */

#define MW 64
#define MH 64
static uint16_t bg1_map[MH * MW];
static uint16_t bg2_map[MH * MW];
static uint16_t bg3_map[MH * MW]; /* only top rows used */

static void build_maps(void)
{
    /* BG2: sky + mountains */
    for (int y=0;y<MH;y++) for (int x=0;x<MW;x++) {
        uint16_t t = 0;
        if (y==22 && (x%12<6)) t = SNES_ENTRY(5,0,0,0,0);
        else if (y>=23 && y<=25 && (x%12<6)) t = SNES_ENTRY(4,0,0,0,0);
        if (y==26) { int m=x%16; t=(m<8)?SNES_ENTRY(2,0,0,0,0):SNES_ENTRY(3,0,0,0,0); }
        else if (y>26&&y<32) { int m=x%16,h=y-26; if(m>=(7-h)&&m<=(8+h)) t=SNES_ENTRY(1,0,0,0,0); }
        else if (y>=32) t = SNES_ENTRY(1,0,0,0,0);
        if (y>=6&&y<=10&&t==0&&((x+y*7)%20==0)) t=SNES_ENTRY(6,0,1,0,0);
        bg2_map[y*MW+x] = t;
    }

    /* BG1: city on ground */
    for (int y=0;y<MH;y++) for (int x=0;x<MW;x++) {
        uint16_t t = 0;
        if (y==31) t = SNES_ENTRY(2,0,0,0,0);
        else if (y>=32) t = SNES_ENTRY(1,0,0,0,0);
        int bx=x%10, btype=(x/10)%4;
        int bh = (btype==0)?6:(btype==1)?8:(btype==2)?5:7;
        int btop = 31 - bh;
        if (bx>=1 && bx<=6 && y>=btop && y<31) {
            if (y==btop) t = SNES_ENTRY(4,0,1,0,0);
            else if ((y-btop)==2 && bx>=2 && bx<=5 && !(bx&1)) t = SNES_ENTRY(5,0,1,0,0);
            else if ((y-btop)==4 && bx>=2 && bx<=5 && !(bx&1)) t = SNES_ENTRY(5,0,1,0,0);
            else if (y==30 && bx==3) t = SNES_ENTRY(6,0,1,0,0);
            else t = SNES_ENTRY(3,0,1,0,0);
        }
        if (y==30 && t==0 && (x%5==0)) t = SNES_ENTRY(7,0,0,0,0);
        bg1_map[y*MW+x] = t;
    }

    /* BG3: status bar top 3 rows */
    for (int y=0;y<MH;y++) for (int x=0;x<MW;x++) {
        uint16_t t = 0;
        if (y<3) t = SNES_ENTRY(1,0,1,0,0);
        else if (y==3) t = SNES_ENTRY(2,0,1,0,0);
        bg3_map[y*MW+x] = t;
    }
}

/* ================================================================
 *  MAIN
 * ================================================================ */

void main(void)
{
    uart_puts("\n\n=== Jupiter SDK — SNES Mode 1 Demo ===\n");
    uart_puts("3 BG layers, parallax scroll, per-tile priority.\n\n");

    timer_init();
    mmu_init();
    init_tiles();
    build_maps();

    snes_bg_t bg1 = { .tiles=bg1_tiledata, .map=bg1_map, .palette=bg1_pal,
                       .map_w=MW, .map_h=MH, .bpp=4, .enabled=1 };
    snes_bg_t bg2 = { .tiles=bg2_tiledata, .map=bg2_map, .palette=bg2_pal,
                       .map_w=MW, .map_h=MH, .bpp=4, .enabled=1 };
    snes_bg_t bg3 = { .tiles=bg3_tiledata, .map=bg3_map, .palette=bg3_pal,
                       .map_w=MW, .map_h=MH, .bpp=2, .enabled=1 };

    video_init();
    uart_puts("Display active. Scrolling...\n\n");

    uint32_t back_fb = FB1_ADDR, front_fb = FB0_ADDR;
    uint32_t *ovl = (uint32_t *)OVL_ADDR;
    uint32_t frame = 0;
    uint32_t backdrop = 0xFF1A2A4A;

    while (1) {
        uint32_t t0 = timer_read();

        bg1.scroll_x = (int32_t)frame;
        bg2.scroll_x = (int32_t)(frame / 2);
        bg1.scroll_y = 120;
        bg2.scroll_y = 100;

        /* Single-pass Mode 1 compositor: one iteration, all layers resolved per pixel */
        snes_mode1_render((uint32_t *)back_fb, ovl, LCD_W, LCD_H,
                          backdrop, &bg1, &bg2, &bg3, 0);

        dcache_clean_fb(back_fb);
        dcache_clean_fb(OVL_ADDR);
        uint32_t t1 = timer_read();

        video_swap(back_fb);
        uint32_t tmp = back_fb; back_fb = front_fb; front_fb = tmp;

        /* Frame pacing */
        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667) ;

        if ((frame % 120) == 0) {
            uart_puts("f="); uart_putdec(frame);
            uart_puts(" render="); uart_putdec(ticks_to_us(timer_elapsed(t0, t1)));
            uart_puts("us\n");
        }
        frame++;
    }
}
