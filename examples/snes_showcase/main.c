/*
 * Jupiter SDK — SNES Grand Showcase (Combined)
 *
 * Phase 1: Parallax checker tests (modes 0-5). Big on top, small on bottom.
 * Phase 2: Scene showcase (modes 0-7). Distinct visual per mode.
 *   Mode 0: 4-layer parallax clouds
 *   Mode 1: City + mountains + status bar
 *   Mode 2: Waving bridge (per-tile offset)
 *   Mode 3: 256-color gradient landscape
 *   Mode 4: 256-color + offset wave
 *   Mode 5: Hi-res dual layer stripes
 *   Mode 6: Checkerboard + dream wobble
 *   Mode 7: Affine rotating floor
 *
 * VI1 PIP label auto-sizes per slot.
 * Build: make GAME=examples/snes_showcase/main.c
 */
#include "jupiter.h"
#include "snes.h"

#define FRAMES_PER_SLOT 420
#define P1_SLOTS 6
#define P2_SLOTS 8
#define TOTAL_SLOTS (P1_SLOTS + P2_SLOTS)

/* ================================================================
 *  TILES — both phases
 * ================================================================ */

/* Phase 1: simple solid tiles */
static uint8_t p1_tiles_2bpp[2 * 16];
static uint8_t p1_tiles_4bpp[2 * 32];
static uint8_t p1_tiles_8bpp[2 * 64];

/* Phase 2: scene tiles */
#define MAX_4BPP 16
static uint8_t s_tiles_4bpp[MAX_4BPP * 32];
#define MAX_2BPP 8
static uint8_t s_tiles_2bpp[MAX_2BPP * 16];
#define MAX_8BPP 8
static uint8_t s_tiles_8bpp[MAX_8BPP * 64];

static void set4(uint8_t *b, int t, const uint8_t d[64])
{ uint8_t *p=b+t*32; for(int r=0;r<8;r++) for(int c=0;c<4;c++)
    p[r*4+c]=(d[r*8+c*2]<<4)|d[r*8+c*2+1]; }
static void set2(uint8_t *b, int t, const uint8_t d[64])
{ uint8_t *p=b+t*16; for(int r=0;r<8;r++) for(int c=0;c<2;c++)
    p[r*2+c]=(d[r*8+c*4]<<6)|(d[r*8+c*4+1]<<4)|(d[r*8+c*4+2]<<2)|d[r*8+c*4+3]; }
static void set8(uint8_t *b, int t, const uint8_t d[64])
{ uint8_t *p=b+t*64; for(int i=0;i<64;i++) p[i]=d[i]; }

static void init_tiles(void)
{
    /* P1: solid tiles */
    for(int i=0;i<16;i++) p1_tiles_2bpp[16+i]=0x55;
    for(int i=0;i<32;i++) p1_tiles_4bpp[32+i]=0x11;
    { uint8_t *p=p1_tiles_8bpp+64; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        p[y*8+x]=(x==0||x==7||y==0||y==7)?1:(uint8_t)(2+(y-1)*36+(x-1)*6); }

    /* P2: scene tiles */
    { uint8_t d[64]; for(int i=0;i<64;i++) { d[i]=1; } set4(s_tiles_4bpp,1,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) { d[y*8+x]=((x+y)&1)?1:2; } set4(s_tiles_4bpp,2,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        if(y==3||y==7) d[y*8+x]=3; else if(y<3&&x==7) d[y*8+x]=3;
        else if(y>3&&x==3) d[y*8+x]=3; else d[y*8+x]=1;
      } set4(s_tiles_4bpp,3,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) { d[y*8+x]=y<3?4:(((x+y)&3)==0?5:6); } set4(s_tiles_4bpp,4,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) { d[y*8+x]=(x>=(7-y))?1:0; } set4(s_tiles_4bpp,5,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) { d[y*8+x]=(x<=y)?1:0; } set4(s_tiles_4bpp,6,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) { d[y*8+x]=(y==0||y==7||x==0||x==7||x==3||y==3)?7:8; } set4(s_tiles_4bpp,7,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) { d[y*8+x]=(y==0||y==7)?9:((y&1)?10:11); } set4(s_tiles_4bpp,8,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) { d[y*8+x]=(y<2)?12:((y==2)?9:((y&1)?10:11)); } set4(s_tiles_4bpp,9,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) { d[y*8+x]=(x&1)?13:14; } set4(s_tiles_4bpp,10,d); }
    { uint8_t d[64]; for(int i=0;i<64;i++) { d[i]=1; } set2(s_tiles_2bpp,1,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) { d[y*8+x]=((x+y)&3)==0?2:1; } set2(s_tiles_2bpp,2,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) { d[y*8+x]=(y==7)?2:1; } set2(s_tiles_2bpp,3,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) { d[y*8+x]=(uint8_t)(x*16+y*8+1); } set8(s_tiles_8bpp,1,d); }
    { uint8_t d[64]; for(int i=0;i<64;i++) { d[i]=40; } set8(s_tiles_8bpp,2,d); }
    { uint8_t d[64]; for(int i=0;i<64;i++) { d[i]=80; } set8(s_tiles_8bpp,3,d); }
    { uint8_t d[64]; for(int i=0;i<64;i++) { d[i]=120; } set8(s_tiles_8bpp,4,d); }
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) { int cx=x-4,cy=y-4; d[y*8+x]=(uint8_t)(cx*cx+cy*cy+1); } set8(s_tiles_8bpp,5,d); }
}

/* ================================================================
 *  PALETTES
 * ================================================================ */

/* P1 palettes */
static const uint32_t p1_r2[4]={0,0xFFDD4444,0xFFFF6666,0xFFBB2222};
static const uint32_t p1_b2[4]={0,0xFF4444DD,0xFF6666FF,0xFF2222BB};
static const uint32_t p1_g2[4]={0,0xFF44DD44,0xFF66FF66,0xFF22BB22};
static const uint32_t p1_y2[4]={0,0xFFDDDD44,0xFFFFFF66,0xFFBBBB22};
static const uint32_t p1_r4[16]={0,0xFFDD4444,[2 ... 15]=0xFF800000};
static const uint32_t p1_b4[16]={0,0xFF4444DD,[2 ... 15]=0xFF000080};

/* P2 scene palettes */
static const uint32_t pal_city[16]={0,0xFFA06030,0xFF806040,0xFF606060,0xFF40A040,0xFF305020,0xFF508030,0xFF909090,0xFF6090C0,0xFF4A3020,0xFF8A6A40,0xFFA08050,0xFFAA8844,0xFFE0E0F0,0xFF2040A0,0xFFFFFFFF};
static const uint32_t pal_sky[16]={0,0xFF5A7AAA,0xFF3A5A8A,0xFF7A6A5A,0xFF40A040,0xFF305020,0xFF508030,[7 ... 15]=0xFF2A4A7A};
static const uint32_t pal_earth[16]={0,0xFF6A5A4A,0xFF5A4A3A,0xFF4A3A2A,0xFF40A030,0xFF306020,0xFF4A7A3A,[7 ... 15]=0xFF3A2A1A};
static const uint32_t s_pal2[4]={0,0xE0203050,0xFF4080C0,0xFFA0C0E0};
static const uint32_t s_pal2b[4]={0,0xFF405060,0xFF607080,0xFF8090A0};

/* Shared 256-color */
static uint32_t pal_256[256];
static void init_pal_256(void)
{
    pal_256[0]=0; pal_256[1]=0xFFFFFFFF;
    for(int i=2;i<256;i++){
        int h=(i-2)*360/254,hi=h/60,f=h%60;
        int v=220,p=40,q=v-(v-p)*f/60,t=p+(v-p)*f/60;
        int r,g,b;
        switch(hi%6){
            case 0:r=v;g=t;b=p;break; case 1:r=q;g=v;b=p;break;
            case 2:r=p;g=v;b=t;break; case 3:r=p;g=q;b=v;break;
            case 4:r=t;g=p;b=v;break; default:r=v;g=p;b=q;break;
        }
        pal_256[i]=0xFF000000|((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)b;
    }
}

/* ================================================================
 *  MAPS + SINE + MODE 7
 * ================================================================ */

#define MW 48
#define MH 48
static uint16_t map_a[MH*MW],map_b[MH*MW],map_c[MH*MW],map_d[MH*MW];

static void make_checker(uint16_t *map, int block)
{ for(int y=0;y<MH;y++) for(int x=0;x<MW;x++){
    int bx=x/block,by=y/block;
    map[y*MW+x]=((bx+by)&1)?SNES_ENTRY(1,0,0,0,0):0;
}}

#define MAX_COLS 64
static int16_t offset_table[MAX_COLS];
#define SIN_N 64
static int16_t sin_tbl[SIN_N];
static void init_sin(void)
{ static const int8_t q[16]={0,1,2,3,4,5,5,6,6,6,6,6,5,5,4,3};
  for(int i=0;i<16;i++){sin_tbl[i]=q[i];sin_tbl[32-i]=q[i];sin_tbl[32+i]=-q[i];if(64-i<64)sin_tbl[64-i]=-q[i];}
  sin_tbl[0]=0; }

static uint8_t m7_map[MW*MH]; static uint32_t m7_lut[256];
static int32_t m7_sin[256],m7_cos[256];
static void init_m7(void)
{
    static const int16_t qtr[65]={0,101,201,302,402,502,601,700,799,897,995,1092,1189,1285,1380,1474,1567,1660,1751,1842,1931,2019,2106,2191,2276,2359,2440,2520,2598,2675,2751,2824,2896,2967,3035,3102,3166,3229,3290,3349,3406,3461,3513,3564,3612,3659,3703,3745,3784,3822,3857,3889,3920,3948,3973,3996,4017,4036,4052,4065,4076,4085,4091,4095,4096};
    for(int i=0;i<256;i++){int idx=i&63,quad=(i>>6)&3;int32_t v;
        if(quad==0)v=qtr[idx];else if(quad==1)v=qtr[64-idx];
        else if(quad==2)v=-qtr[idx];else v=-qtr[64-idx];
        m7_sin[i]=v;m7_cos[i]=m7_sin[(i+64)&255];}
    for(int y=0;y<MH;y++) for(int x=0;x<MW;x++){
        int cx=x-MH/2,cy=y-MH/2;
        m7_map[y*MW+x]=(cx*cx+cy*cy<16)?2:((x+y)&1)?1:0;}
    m7_lut[0]=0xFF2A5A2A;m7_lut[1]=0xFF3A7A3A;m7_lut[2]=0xFFA06030;
}

/* ================================================================
 *  BG STATE + PHASE SETUP
 * ================================================================ */

static snes_bg_t bg[4];
static snes_tile_offset_t ofs;

static void setup_p1(int mode)
{
    for(int i=0;i<4;i++){bg[i].enabled=0;bg[i].scroll_x=0;bg[i].scroll_y=0;}
    switch(mode){
    case 0: make_checker(map_a,4);make_checker(map_b,3);make_checker(map_c,2);make_checker(map_d,1);
        bg[0]=(snes_bg_t){p1_tiles_2bpp,map_a,p1_r2,0,0,MW,MH,2,1};
        bg[1]=(snes_bg_t){p1_tiles_2bpp,map_b,p1_b2,0,0,MW,MH,2,1};
        bg[2]=(snes_bg_t){p1_tiles_2bpp,map_c,p1_g2,0,0,MW,MH,2,1};
        bg[3]=(snes_bg_t){p1_tiles_2bpp,map_d,p1_y2,0,0,MW,MH,2,1}; break;
    case 1: make_checker(map_a,3);make_checker(map_b,2);make_checker(map_c,1);
        bg[0]=(snes_bg_t){p1_tiles_4bpp,map_a,p1_r4,0,0,MW,MH,4,1};
        bg[1]=(snes_bg_t){p1_tiles_4bpp,map_b,p1_b4,0,0,MW,MH,4,1};
        bg[2]=(snes_bg_t){p1_tiles_2bpp,map_c,p1_g2,0,0,MW,MH,2,1}; break;
    case 2: make_checker(map_a,2);make_checker(map_b,1);
        bg[0]=(snes_bg_t){p1_tiles_4bpp,map_a,p1_r4,0,0,MW,MH,4,1};
        bg[1]=(snes_bg_t){p1_tiles_4bpp,map_b,p1_b4,0,0,MW,MH,4,1};
        ofs.col_offset=offset_table;ofs.vertical=1; break;
    case 3: make_checker(map_a,3);make_checker(map_b,2);
        bg[0]=(snes_bg_t){p1_tiles_8bpp,map_a,pal_256,0,0,MW,MH,8,1};
        bg[1]=(snes_bg_t){p1_tiles_4bpp,map_b,p1_b4,0,0,MW,MH,4,1}; break;
    case 4: make_checker(map_a,3);make_checker(map_b,2);
        bg[0]=(snes_bg_t){p1_tiles_8bpp,map_a,pal_256,0,0,MW,MH,8,1};
        bg[1]=(snes_bg_t){p1_tiles_2bpp,map_b,p1_g2,0,0,MW,MH,2,1};
        ofs.col_offset=offset_table;ofs.vertical=1; break;
    case 5: make_checker(map_a,2);make_checker(map_b,1);
        bg[0]=(snes_bg_t){p1_tiles_4bpp,map_a,p1_r4,0,0,MW,MH,4,1};
        bg[1]=(snes_bg_t){p1_tiles_2bpp,map_b,p1_b2,0,0,MW,MH,2,1}; break;
    }
}

static void setup_p2(int mode)
{
    for(int i=0;i<4;i++){bg[i].enabled=0;bg[i].scroll_x=0;bg[i].scroll_y=0;}
    switch(mode){
    case 0: /* 4-layer parallax hills */
        /* BG4 (furthest): low distant hills */
        for(int y=0;y<MH;y++) for(int x=0;x<MW;x++){
            int h=26+((x*3+5)%5);
            map_d[y*MW+x]=(y>=h)?SNES_ENTRY(1,0,0,0,0):0;}
        /* BG3: medium hills */
        for(int y=0;y<MH;y++) for(int x=0;x<MW;x++){
            int h=22+((x*7+3)%7);
            map_c[y*MW+x]=(y>=h)?SNES_ENTRY(1,0,0,0,0):0;}
        /* BG2: taller foreground hills */
        for(int y=0;y<MH;y++) for(int x=0;x<MW;x++){
            int h=18+((x*5+1)%9);
            map_b[y*MW+x]=(y>=h)?SNES_ENTRY(2,0,0,0,0):0;}
        /* BG1 (nearest): ground with detail */
        for(int y=0;y<MH;y++) for(int x=0;x<MW;x++){
            if(y>=26) map_a[y*MW+x]=SNES_ENTRY(2,0,0,0,0);
            else if(y==25) map_a[y*MW+x]=SNES_ENTRY(1,0,0,0,0);
            else map_a[y*MW+x]=0;}
        bg[0]=(snes_bg_t){s_tiles_2bpp,map_a,s_pal2,0,0,MW,MH,2,1};
        bg[1]=(snes_bg_t){s_tiles_2bpp,map_b,s_pal2,0,0,MW,MH,2,1};
        bg[2]=(snes_bg_t){s_tiles_2bpp,map_c,s_pal2b,0,0,MW,MH,2,1};
        bg[3]=(snes_bg_t){s_tiles_2bpp,map_d,s_pal2b,0,0,MW,MH,2,1}; break;
    case 1: /* City parallax */
        for(int y=0;y<MH;y++) for(int x=0;x<MW;x++){
            uint16_t t=0; int bx=x%8,bh=4+(x/8)%4,bt=24-bh;
            if(y==24) t=SNES_ENTRY(4,0,0,0,0);
            else if(y>24) t=SNES_ENTRY(1,0,0,0,0);
            else if(bx>=1&&bx<=5&&y>=bt&&y<24){
                if(y==bt)t=SNES_ENTRY(1,0,1,0,0);
                else if((y-bt)==2&&bx==3)t=SNES_ENTRY(7,0,1,0,0);
                else t=SNES_ENTRY(3,0,1,0,0);}
            map_a[y*MW+x]=t;
            t=0; if(y==20){int m=x%12;t=(m<6)?SNES_ENTRY(5,0,0,0,0):SNES_ENTRY(6,0,0,0,0);}
            else if(y>20&&y<24) t=SNES_ENTRY(1,0,0,0,0);
            map_b[y*MW+x]=t;
            map_c[y*MW+x]=(y<3)?SNES_ENTRY(1,0,1,0,0):(y==3)?SNES_ENTRY(3,0,1,0,0):0;
        }
        bg[0]=(snes_bg_t){s_tiles_4bpp,map_a,pal_city,0,0,MW,MH,4,1};
        bg[1]=(snes_bg_t){s_tiles_4bpp,map_b,pal_sky,0,0,MW,MH,4,1};
        bg[2]=(snes_bg_t){s_tiles_2bpp,map_c,s_pal2,0,0,MW,MH,2,1}; break;
    case 2: /* Waving bridge */
        for(int y=0;y<MH;y++) for(int x=0;x<MW;x++){
            uint16_t t1=0;
            if(x>=6&&x<=26&&(y==14||y==15)) t1=SNES_ENTRY(8,0,0,0,0);
            map_a[y*MW+x]=t1;
            uint16_t t2=0;
            if(y<12) t2=SNES_ENTRY(1,0,0,0,0);
            else if(y>14) t2=SNES_ENTRY(1,0,0,0,0);
            map_b[y*MW+x]=t2;
        }
        bg[0]=(snes_bg_t){s_tiles_4bpp,map_a,pal_city,0,0,MW,MH,4,1};
        bg[1]=(snes_bg_t){s_tiles_4bpp,map_b,pal_sky,0,0,MW,MH,4,1};
        ofs.col_offset=offset_table;ofs.vertical=1; break;
    case 3: /* 256-color gradient */
        for(int y=0;y<MH;y++) for(int x=0;x<MW;x++){
            int tile=1+((x+y)%4);
            map_a[y*MW+x]=SNES_ENTRY(tile,0,0,0,0);
            map_b[y*MW+x]=(y>=20)?SNES_ENTRY(4,0,1,0,0):0;
        }
        bg[0]=(snes_bg_t){s_tiles_8bpp,map_a,pal_256,0,0,MW,MH,8,1};
        bg[1]=(snes_bg_t){s_tiles_4bpp,map_b,pal_earth,0,0,MW,MH,4,1}; break;
    case 4: /* 256-color + offset wave */
        for(int y=0;y<MH;y++) for(int x=0;x<MW;x++){
            map_a[y*MW+x]=SNES_ENTRY(5,0,0,0,0);
            map_b[y*MW+x]=(y>=22)?SNES_ENTRY(2,0,1,0,0):0;
        }
        bg[0]=(snes_bg_t){s_tiles_8bpp,map_a,pal_256,0,0,MW,MH,8,1};
        bg[1]=(snes_bg_t){s_tiles_2bpp,map_b,s_pal2,0,0,MW,MH,2,1};
        ofs.col_offset=offset_table;ofs.vertical=1; break;
    case 5: /* Hi-res: color bands + overlay window */
        for(int y=0;y<MH;y++) for(int x=0;x<MW;x++){
            /* BG1: row-dependent tiles — each band uses different tile */
            int band=(y%6);
            int tile=(band<2)?3:(band<4)?4:2; /* brick/grass/checker */
            map_a[y*MW+x]=SNES_ENTRY(tile,0,0,0,0);
            /* BG2: border frame (tiles around edge, empty center) */
            if(y<2||y>=30||x<2||x>=30)
                map_b[y*MW+x]=SNES_ENTRY(1,0,1,0,0); /* high pri border */
            else
                map_b[y*MW+x]=0;
        }
        bg[0]=(snes_bg_t){s_tiles_4bpp,map_a,pal_city,0,0,MW,MH,4,1};
        bg[1]=(snes_bg_t){s_tiles_2bpp,map_b,s_pal2,0,0,MW,MH,2,1}; break;
    case 6: /* Horizontal stripes + wave distortion */
        for(int y=0;y<MH;y++) for(int x=0;x<MW;x++){
            /* Alternating rows: brick/grass creates visible stripes.
             * Vertical per-column offset makes stripes undulate. */
            int band=(y>>1)&3;
            int tile=(band==0)?3:(band==1)?4:(band==2)?1:2;
            map_a[y*MW+x]=SNES_ENTRY(tile,0,0,0,0);
        }
        bg[0]=(snes_bg_t){s_tiles_4bpp,map_a,pal_city,0,0,MW,MH,4,1};
        ofs.col_offset=offset_table;ofs.vertical=1; break;
    case 7: break;
    }
}

/* ================================================================
 *  SLOT TABLE
 * ================================================================ */

static const struct { uint8_t mode; uint8_t phase; const char *label; } slots[TOTAL_SLOTS] = {
    {0,0,"MODE 0 2+2+2+2"},{1,0,"MODE 1 4+4+2"},{2,0,"MODE 2 4+4 OFFSET"},
    {3,0,"MODE 3 8+4"},{4,0,"MODE 4 8+2 OFFSET"},{5,0,"MODE 5 4+2"},
    {0,1,"MODE 0 DEMO"},{1,1,"MODE 1 DEMO"},{2,1,"MODE 2 DEMO"},
    {3,1,"MODE 3 DEMO"},{4,1,"MODE 4 DEMO"},{5,1,"MODE 5 DEMO"},
    {6,1,"MODE 6 DEMO"},{7,1,"MODE 7 DEMO"},
};

/* ================================================================
 *  FONT + VI1
 * ================================================================ */

#define NG 22
static const uint8_t font[NG][7]={
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F},{0x0E,0x11,0x01,0x06,0x01,0x11,0x0E},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},{0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
    {0x11,0x1B,0x15,0x11,0x11,0x11,0x11},{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
    {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C},{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x04,0x0E,0x04,0x00,0x00},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},{0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E},
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},{0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},{0x11,0x19,0x15,0x13,0x11,0x11,0x11},
};
static int fi(char c){if(c>='0'&&c<='9')return c-'0';
    switch(c){case'M':return 10;case'O':return 11;case'D':return 12;case'E':return 13;
    case'+':return 15;case'F':return 16;case'S':return 17;case'T':return 18;
    case'A':return 19;case'I':return 20;case'N':return 21;}return 14;}
static int slen(const char*s){int n=0;while(*s++){n++;}return n;}
#define VI1_H 24
static void vi1_label(const char*str){
    int len=slen(str);uint32_t w=(uint32_t)(len*12+12);if(w<80)w=80;
    video_vi1_init(8,8,w,VI1_H);
    uint32_t*vb=(uint32_t*)SPR_ADDR;
    for(uint32_t i=0;i<w*VI1_H;i++)vb[i]=0xFF102040;
    for(uint32_t x=0;x<w;x++)vb[(VI1_H-1)*w+x]=0xFF4080C0;
    int cx=6;while(*str){const uint8_t*g=font[fi(*str)];
        for(int r=0;r<7;r++){uint8_t row=g[r];for(int c=0;c<5;c++){
            if(row&(0x10>>c)){int sx=cx+c*2,sy=4+r*2;
                if(sx+1<(int)w&&sy+1<VI1_H){vb[sy*w+sx]=0xFFFFFFFF;vb[sy*w+sx+1]=0xFFFFFFFF;
                vb[(sy+1)*w+sx]=0xFFFFFFFF;vb[(sy+1)*w+sx+1]=0xFFFFFFFF;}}}}cx+=12;str++;}
    dcache_clean_range(SPR_ADDR,w*VI1_H*4);}

/* ================================================================
 *  RENDER
 * ================================================================ */

static void render(int mode, int phase, uint32_t *fb, uint32_t *ovl, uint32_t mf)
{
    uint32_t bd = phase==0 ? 0xFF1A1A2A : 0xFF2A4A7A;
    switch(mode){
    case 0: bg[0].scroll_x=(int32_t)mf;bg[1].scroll_x=(int32_t)(mf*3/4);
        bg[2].scroll_x=(int32_t)(mf/2);bg[3].scroll_x=(int32_t)(mf/4);
        snes_mode0_render(fb,ovl,LCD_W,LCD_H,bd,&bg[0],&bg[1],&bg[2],&bg[3], 0); break;
    case 1: bg[0].scroll_x=(int32_t)mf;bg[1].scroll_x=(int32_t)(mf/2);
        if(phase==1){bg[0].scroll_y=80;bg[1].scroll_y=60;}
        snes_mode1_render(fb,ovl,LCD_W,LCD_H,bd,&bg[0],&bg[1],&bg[2], 0); break;
    case 2: if(phase==1){bg[0].scroll_y=40;bg[1].scroll_y=40;}
        else{bg[0].scroll_x=(int32_t)mf;bg[1].scroll_x=(int32_t)(mf/2);}
        for(int c=0;c<MAX_COLS;c++){int ph=(int)(mf+c*2)&(SIN_N-1);offset_table[c]=sin_tbl[ph];}
        snes_mode2_render(fb,ovl,LCD_W,LCD_H,bd,&bg[0],&bg[1],&ofs, 0); break;
    case 3: bg[0].scroll_x=(int32_t)mf;bg[1].scroll_x=(int32_t)(mf/2);if(phase==1){bg[0].scroll_y=(int32_t)(mf/3);}
        snes_mode3_render(fb,ovl,LCD_W,LCD_H,phase==1?0xFF102030:bd,&bg[0],&bg[1], 0); break;
    case 4: bg[0].scroll_x=(int32_t)mf;bg[1].scroll_x=(int32_t)(mf/2);
        for(int c=0;c<MAX_COLS;c++){int ph=(int)(mf*2+c*3)&(SIN_N-1);offset_table[c]=sin_tbl[ph];}
        snes_mode4_render(fb,ovl,LCD_W,LCD_H,phase==1?0xFF081828:bd,&bg[0],&bg[1],&ofs, 0); break;
    case 5: bg[0].scroll_x=(int32_t)mf;bg[1].scroll_x=(int32_t)(mf/2);
        snes_mode5_render(fb,ovl,LCD_W,LCD_H,bd,&bg[0],&bg[1], 0); break;
    case 6: for(int c=0;c<MAX_COLS;c++){int ph=(int)(mf*3+c*4)&(SIN_N-1);offset_table[c]=sin_tbl[ph];}
        snes_mode6_render(fb,ovl,LCD_W,LCD_H,bd,&bg[0],&ofs, 0); break;
    case 7:{memset32_neon(OVL_ADDR,0,LCD_FB_BYTES);
        uint8_t angle=(uint8_t)mf;int32_t ca=m7_cos[angle],sa=m7_sin[angle];
        int32_t cx=MW/2*4096,cy=MH/2*4096;
        for(uint32_t py=0;py<LCD_H/2;py++){uint32_t*row=fb+py*LCD_W;uint32_t g=0x60+py;
            uint32_t sky=0xFF000000|(g/3<<16)|(g/2<<8)|g;
            for(uint32_t px=0;px<LCD_W;px++)row[px]=sky;}
        for(uint32_t py=LCD_H/2;py<LCD_H;py++){uint32_t*row=fb+py*LCD_W;
            int32_t d=(int32_t)(py-LCD_H/2+1),scale=4096*8/d;
            int32_t du=(ca*scale)>>12,dv=(sa*scale)>>12;
            int32_t u0=cx-(int32_t)(LCD_W/2)*du-(int32_t)(LCD_H/4)*dv;
            int32_t v0=cy-(int32_t)(LCD_W/2)*dv+(int32_t)(LCD_H/4)*du;
            iso_scanline(row,m7_map,m7_lut,u0,v0,du,dv,LCD_W/2);
            if(py+1<LCD_H)memcpy_neon(row+LCD_W,row,LCD_W*4);}break;}
    }
}

/* ================================================================
 *  MAIN
 * ================================================================ */

void main(void)
{
    uart_puts("\n\n=== SNES Grand Showcase ===\n");
    uart_puts("Phase 1: Checkers (0-5) | Phase 2: Scenes (0-7)\n\n");
    timer_init();mmu_init();init_tiles();init_pal_256();init_sin();init_m7();
    video_init();vi1_label(slots[0].label);
    uint32_t back=FB1_ADDR,front=FB0_ADDR,frame=0;int cur=-1;
    uint32_t *ovl=(uint32_t*)OVL_ADDR;
    while(1){
        uint32_t t0=timer_read();
        int slot=(int)((frame/FRAMES_PER_SLOT)%TOTAL_SLOTS);
        uint32_t mf=frame%FRAMES_PER_SLOT;
        int mode=slots[slot].mode,phase=slots[slot].phase;
        if(slot!=cur){
            if(phase==0)setup_p1(mode);else setup_p2(mode);
            cur=slot;vi1_label(slots[slot].label);
            uart_puts(phase==0?"[CHK] ":"[SCN] ");
            uart_puts(slots[slot].label);uart_puts("\n");}
        render(mode,phase,(uint32_t*)back,ovl,mf);
        dcache_clean_fb(back);dcache_clean_fb(OVL_ADDR);uint32_t t1=timer_read();
        video_swap(back);uint32_t tmp=back;back=front;front=tmp;
        while(ticks_to_us(timer_elapsed(t0,timer_read()))<16667);
        if((frame%120)==0){uart_puts("f=");uart_putdec(frame);
            uart_puts(" s=");uart_putdec(slot);uart_puts(" ");
            uart_putdec(ticks_to_us(timer_elapsed(t0,t1)));uart_puts("us\n");}
        frame++;}
}
