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

#define BG2_TILES 18
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

/* Battlement dither tile: 4px-wide teeth between band colors a (top) and b */
static void set_teeth_4bpp(uint8_t *base, int tidx, uint8_t a, uint8_t b)
{
    uint8_t d[64];
    for (int y=0;y<8;y++) for (int x=0;x<8;x++)
        d[y*8+x] = (y < ((x&4)?2:6)) ? a : b;
    set_4bpp_tile(base, tidx, d);
}

static void init_tiles(void)
{
    /* BG2: dusk sky + mountains
     * 0=transparent 1=near mountain 4=distant mountain
     * sky bands 6..11 (indigo→violet→plum→rose→amber→glow) */
    fill_tile_4bpp(bg2_tiledata, 1, 1);  /* solid near mountain */
    fill_tile_4bpp(bg2_tiledata, 4, 4);  /* distant mountain */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=(y>=4)?1:11;
      set_4bpp_tile(bg2_tiledata,2,d); }        /* near step: half mtn / glow */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=(y>=4)?4:9;
      set_4bpp_tile(bg2_tiledata,3,d); }        /* far step: half mtn / rose */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        int cx=x-4,cy=y-3; d[y*8+x]=(cx*cx+cy*cy*2<12)?9:8;
      } set_4bpp_tile(bg2_tiledata,6,d); }      /* dusk-lit cloud puff on plum */
    for (int i=0;i<6;i++)
        fill_tile_4bpp(bg2_tiledata, 7+i, (uint8_t)(6+i)); /* 7..12: solid bands */
    set_teeth_4bpp(bg2_tiledata, 13, 6, 7);
    set_teeth_4bpp(bg2_tiledata, 14, 7, 8);
    set_teeth_4bpp(bg2_tiledata, 15, 8, 9);
    set_teeth_4bpp(bg2_tiledata, 16, 9, 10);
    set_teeth_4bpp(bg2_tiledata, 17, 10, 11);

    /* BG1: dusk city */
    /* 0=transparent 1=pavement 2=pavement-speckle 3=grass 4=grass-lit
     * 5=wall 6=wall-shadow 7=roof-rim 8=window-lit 9=glow-bright
     * 10=door-dark 11=bush-accent 12=window-dark 13=window-dim */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        int bx=x>>1, by=y>>1;
        d[y*8+x]=((bx==0&&by==1)||(bx==2&&by==3))?2:1;
      } set_4bpp_tile(bg1_tiledata,1,d); }      /* pavement, sparse 2x2 dots */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        int bx=x>>1, by=y>>1;
        d[y*8+x]=y<2?4:y<3?3:(((bx==0&&by==1)||(bx==2&&by==3))?2:1);
      } set_4bpp_tile(bg1_tiledata,2,d); }      /* grass strip on pavement */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        if(y==3||y==7) d[y*8+x]=6;
        else if(y<3&&x==7) d[y*8+x]=6;
        else if(y>3&&x==3) d[y*8+x]=6;
        else d[y*8+x]=5;
      } set_4bpp_tile(bg1_tiledata,3,d); }      /* silhouette wall */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=y<1?7:y<2?6:((y==3||y==7)?6:((y>3&&x==3)?6:5));
      set_4bpp_tile(bg1_tiledata,4,d); }        /* roofline with thin rim */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=(y==0||y==7||x==0||x==7||x==3||y==3)?6:8;
      set_4bpp_tile(bg1_tiledata,5,d); }        /* window: all panes lit */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=(x==0||x==7||y==0)?6:(x==5&&y==4)?9:13;
      set_4bpp_tile(bg1_tiledata,6,d); }        /* glowing doorway */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=y>=5?1:((x+y)&1)?((y<2)?11:3):3;
      set_4bpp_tile(bg1_tiledata,7,d); }        /* dusk bush */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=(y==0||y==7||x==0||x==7||x==3||y==3)?6:
                  ((x<3&&y<3)?8:12);
      set_4bpp_tile(bg1_tiledata,8,d); }        /* window: one pane lit */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=(y==0||y==7||x==0||x==7||x==3||y==3)?6:
                  ((x>3&&y>3)?13:12);
      set_4bpp_tile(bg1_tiledata,9,d); }        /* window: dim corner */

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
    0x00000000,     /* 0: transparent */
    0xFF241C3E,     /* 1: near mountain silhouette */
    0xFF241C3E,     /* 2: (spare, matches 1) */
    0xFF241C3E,     /* 3: (spare, matches 1) */
    0xFF3A2C58,     /* 4: distant mountain */
    0xFF3A2C58,     /* 5: (spare, matches 4) */
    0xFF2A2248,     /* 6: sky indigo (top) */
    0xFF4A3268,     /* 7: sky violet */
    0xFF7A3E6A,     /* 8: sky plum */
    0xFFB05868,     /* 9: sky rose */
    0xFFE08850,     /* 10: sky amber */
    0xFFF8B868,     /* 11: horizon glow */
    [12 ... 15] = 0xFF2A2248,
};
static const uint32_t bg1_pal[16] = {
    0x00000000,     /* 0: transparent */
    0xFF20243A,     /* 1: pavement */
    0xFF2A2F4A,     /* 2: pavement speckle */
    0xFF28463A,     /* 3: grass dusk */
    0xFF3A6A4E,     /* 4: grass lit */
    0xFF2A2A44,     /* 5: wall silhouette */
    0xFF202034,     /* 6: wall shadow / frames */
    0xFF5A5480,     /* 7: roofline rim */
    0xFFF8C868,     /* 8: window lit amber */
    0xFFFFE8A0,     /* 9: glow bright */
    0xFF1A1A2A,     /* 10: door dark */
    0xFF4A8A5A,     /* 11: bush accent mint */
    0xFF32304E,     /* 12: window dark */
    0xFFC08040,     /* 13: window dim ember */
    [14 ... 15] = 0xFF000000,
};
static const uint32_t bg3_pal[4] = {
    0x00000000, 0xE0141C2C, 0xFF48A090, 0xFFB0E0D8,
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
    /* BG2: dusk gradient sky + mountain silhouettes */
    for (int y=0;y<MH;y++) for (int x=0;x<MW;x++) {
        uint16_t t;
        if (y<=16)      t = 0;                       /* transparent: status bar
                                                      * (fb layer) shows here,
                                                      * backdrop = indigo */
        else if (y==17) t = SNES_ENTRY(13,0,0,0,0);  /* teeth indigo/violet */
        else if (y==18) t = SNES_ENTRY(8,0,0,0,0);   /* violet */
        else if (y==19) t = SNES_ENTRY(14,0,0,0,0);  /* teeth violet/plum */
        else if (y==20) t = ((x*7+3)%19<2) ? SNES_ENTRY(6,0,0,(x&1),0)
                                           : SNES_ENTRY(9,0,0,0,0); /* plum + puffs */
        else if (y==21) t = SNES_ENTRY(15,0,0,0,0);  /* teeth plum/rose */
        else if (y>=22 && y<=25) {
            /* distant range: chunky stepped pyramids over rose→amber bands */
            int m=x%12, h=y-22;
            if (m>=(4-h) && m<=(5+h))
                t = (h==0) ? SNES_ENTRY(3,0,0,0,0)   /* half-step peak */
                           : SNES_ENTRY(4,0,0,0,0);
            else t = (y==22) ? SNES_ENTRY(10,0,0,0,0)   /* rose */
                 : (y==23) ? SNES_ENTRY(16,0,0,0,0)     /* teeth rose/amber */
                 : (y==24) ? SNES_ENTRY(11,0,0,0,0)     /* amber */
                 :           SNES_ENTRY(17,0,0,0,0);    /* teeth amber/glow */
        }
        else if (y==26) { int m=x%16;
            t = (m==7||m==8) ? SNES_ENTRY(2,0,0,0,0)    /* half-step peak */
                             : SNES_ENTRY(12,0,0,0,0); /* glow */ }
        else if (y<32) { int m=x%16,h=y-26;
            t = (m>=(7-h)&&m<=(8+h)) ? SNES_ENTRY(1,0,0,0,0)
                                     : SNES_ENTRY(12,0,0,0,0); /* glow gaps */ }
        else            t = SNES_ENTRY(1,0,0,0,0);
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
            int wrow = (y-btop)==2 || (y-btop)==4 || (y-btop)==6;
            if (y==btop) t = SNES_ENTRY(4,0,1,0,0);
            else if (wrow && bx>=2 && bx<=5 && !(bx&1)) {
                int h = (x*7 + y*13) % 5;   /* lit / half-lit / dim mix */
                t = SNES_ENTRY((h<3)?5:(h==3)?8:9, 0,1,0,0);
            }
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
    uint32_t backdrop = 0xFF2A2248;

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
