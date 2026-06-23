/*
 * Jupiter SDK — AV Demo (SNES visuals + 3-console VGM audio)
 *
 * Act 1: Jukebox — three VGM tracks play sequentially (Genesis, NES, GB)
 *        while SNES background modes cycle underneath.
 * Act 2: Triggered — each visual mode change seeks into a different song
 *        at a non-zero position, proving audio-video sync.
 *
 * Build: make GAME=examples/av_demo/main.c
 */
#include "jupiter.h"
#include "snes.h"
#include "../../libvgm/vgm_player.h"
#include "step_on_beat.h"
#include "the_moon.h"
#include "pokemon_gym.h"

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
#define MAX_4BPP 20
static uint8_t s_tiles_4bpp[MAX_4BPP * 32];
#define MAX_2BPP 8
static uint8_t s_tiles_2bpp[MAX_2BPP * 16];
/* Mode 3 demo renders a 32×28 tile (256×224 px) "smooth-shaded sphere
 * on starfield" — each tile is unique (the whole region is one bitmap
 * rather than a tilemap), so we need 32×28 + a few overhead = ~900
 * unique 8bpp tiles. 1024 gives headroom. */
#define MAX_8BPP 1024
static uint8_t s_tiles_8bpp[MAX_8BPP * 64];
#define MODE3_TW   32u
#define MODE3_TH   28u
#define MODE3_FIRST_TILE  8u   /* leave tiles 0..7 for legacy modes */

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

    /* P2: scene tiles — designed for canonical SNES mode showcases.
     * Tile 0 is transparent in SNES BG entries.
     *
     * 4bpp tile palette indexing (uses pal_city / pal_forest):
     *   0=transparent  1=sky/light  2=highlight  3=mid  4=dark  5=accent
     *   6=outline  7=window  8=brick light  9=brick dark
     *   10=grass bright 11=grass dark 12=tree dark 13=trunk
     *   14=hill light 15=hill shadow */

    /* 4bpp #1: solid color 1 (sky fill) */
    { uint8_t d[64]; for(int i=0;i<64;i++) d[i]=1; set4(s_tiles_4bpp,1,d); }
    /* 4bpp #2: checker — for distinct backdrops in Mode 0 layers */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=((x+y)&1)?1:2; set4(s_tiles_4bpp,2,d); }
    /* 4bpp #3: brick wall (light-mortar bricks for ground) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        int brick_row=(y/4)&1; int mortar=(y%4==3)||(x==(brick_row?3:7));
        d[y*8+x]=mortar?9:8; } set4(s_tiles_4bpp,3,d); }
    /* 4bpp #4: ground (grass + dirt — top row green, rest brown) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=(y<2)?10:(y<3?11:((x+y)&3?4:5)); set4(s_tiles_4bpp,4,d); }
    /* 4bpp #5: triangle UL (used as sloped pieces or castle towers) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=(x>=(7-y))?6:0; set4(s_tiles_4bpp,5,d); }
    /* 4bpp #6: triangle UR (mirror — slope the other way) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=(x<=y)?6:0; set4(s_tiles_4bpp,6,d); }
    /* 4bpp #7: window grid (FF6-style status window frame, 4 colors) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        int edge=(y==0||y==7||x==0||x==7); int cross=(x==3||y==3);
        d[y*8+x]=edge?7:(cross?6:8); } set4(s_tiles_4bpp,7,d); }
    /* 4bpp #8: stones (bottom row dark, alternating) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=(y==0||y==7)?9:((y&1)?8:11); set4(s_tiles_4bpp,8,d); }
    /* 4bpp #9: cobblestone (top-row bricks) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=(y<2)?12:((y==2)?9:((y&1)?8:11)); set4(s_tiles_4bpp,9,d); }
    /* 4bpp #10: stripes — alternating bright/dark cols (for Mode 0 layer 3) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=(x&1)?2:1; set4(s_tiles_4bpp,10,d); }
    /* 4bpp #11: cloud puff (white blob on transparent — Mode 1 BG3) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        int cx=x-3,cy=y-3; int r=cx*cx+cy*cy;
        d[y*8+x]=(r<=4)?2:(r<=7)?1:0; } set4(s_tiles_4bpp,11,d); }
    /* 4bpp #12: hill top — rounded green curve (Mode 1 BG2) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        int cx=x-4; int top=4+(cx*cx)/8;
        d[y*8+x]=(y<top)?0:((y==top)?10:14); } set4(s_tiles_4bpp,12,d); }
    /* 4bpp #13: tree (triangular green canopy with trunk) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        int dx=(x<4?(3-x):(x-4));
        if(y>=5&&(x==3||x==4)) d[y*8+x]=13;       /* trunk */
        else if(y<5&&dx<=(4-y)) d[y*8+x]=(y<2)?10:12;  /* canopy */
        else d[y*8+x]=0; } set4(s_tiles_4bpp,13,d); }
    /* 4bpp #14: castle wall (dark stone) — for Mode 3 silhouette */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        int mortar=(y%4==3)||(x%4==3);
        d[y*8+x]=mortar?6:9; } set4(s_tiles_4bpp,14,d); }
    /* 4bpp #15: castle crenellation (battlement top) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        int notch=((x/2)&1)==0 && y<3;
        d[y*8+x]=notch?0:9; } set4(s_tiles_4bpp,15,d); }
    /* 4bpp #16-19: solid-color tiles for arbitrary palette indices.
     * Useful when we bind a custom palette (e.g. pal_road) and want
     * to fill regions with a specific color without writing a new
     * patterned tile each time. Index i+15 = solid palette index i+1. */
    { uint8_t d[64]; for(int i=0;i<64;i++) d[i]=2; set4(s_tiles_4bpp,16,d); }
    { uint8_t d[64]; for(int i=0;i<64;i++) d[i]=3; set4(s_tiles_4bpp,17,d); }
    { uint8_t d[64]; for(int i=0;i<64;i++) d[i]=4; set4(s_tiles_4bpp,18,d); }
    { uint8_t d[64]; for(int i=0;i<64;i++) d[i]=5; set4(s_tiles_4bpp,19,d); }

    /* 2bpp tiles (4 colors total — used for Mode 0 layers and Mode 1 BG3) */
    /* 2bpp #1: solid color 1 */
    { uint8_t d[64]; for(int i=0;i<64;i++) d[i]=1; set2(s_tiles_2bpp,1,d); }
    /* 2bpp #2: sparse dots (sky/stars) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=((x*3+y*7)%17==0)?3:1; set2(s_tiles_2bpp,2,d); }
    /* 2bpp #3: horizon line (last row dark, rest light) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=(y==7)?2:1; set2(s_tiles_2bpp,3,d); }
    /* 2bpp #4: cloud (white blob, 4-color limit) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        int cx=x-3,cy=y-3; int r=cx*cx+cy*cy;
        d[y*8+x]=(r<=4)?3:(r<=7)?2:1; } set2(s_tiles_2bpp,4,d); }
    /* 2bpp #5: sun disc (radial yellow blob) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        int cx=x-3,cy=y-3; int r=cx*cx+cy*cy;
        d[y*8+x]=(r<=4)?3:(r<=9)?2:1; } set2(s_tiles_2bpp,5,d); }
    /* 2bpp #6: vertical stripe pattern (Mode 0 backdrop) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=((x>>1)&1)?2:1; set2(s_tiles_2bpp,6,d); }
    /* 2bpp #7: mountain silhouette (triangular dark mass on light) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        int dx=(x<4?(3-x):(x-4)); d[y*8+x]=(y>=dx)?2:1; } set2(s_tiles_2bpp,7,d); }

    /* 8bpp tiles — for Mode 3/4 (256-color BG1).
     * Indices fill pal_sunset (built in init_pal_256). */
    /* 8bpp #1: rainbow gradient (kept from original, useful as backup) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=(uint8_t)(x*16+y*8+1); set8(s_tiles_8bpp,1,d); }
    /* 8bpp #2,3,4: solid bands of indices 40/80/120 */
    { uint8_t d[64]; for(int i=0;i<64;i++) d[i]=40; set8(s_tiles_8bpp,2,d); }
    { uint8_t d[64]; for(int i=0;i<64;i++) d[i]=80; set8(s_tiles_8bpp,3,d); }
    { uint8_t d[64]; for(int i=0;i<64;i++) d[i]=120; set8(s_tiles_8bpp,4,d); }
    /* 8bpp #5: radial sun disc (for sunset center) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        int cx=x-4,cy=y-4; int r2=cx*cx+cy*cy;
        d[y*8+x]=(uint8_t)(255-r2*4); } set8(s_tiles_8bpp,5,d); }
    /* 8bpp #6: SMOOTH vertical gradient — 1 row = 1 index step. */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x]=(uint8_t)y; set8(s_tiles_8bpp,6,d); }
    /* Tiles 8..(8 + MODE3_TW*MODE3_TH - 1) reserved for the Mode 3/4
     * sphere bitmap, written below by gen_mode3_sphere(). */
}

/* Procedural Mode 3 image: a smooth-shaded sphere on a starfield with
 * a vertical gradient sky. Pure 256-color SNES showcase content —
 * each pixel picks an index from pal_sphere (defined further down)
 * where adjacent indices differ by single shading steps. 16-color
 * modes would have to dither this heavily; 256-color renders it
 * cleanly. Each 8×8 region of the 256×224 image becomes its own
 * unique 8bpp tile (no tile reuse — it's a bitmap). */
static void gen_mode3_sphere(void)
{
    const int IW = 256, IH = 224;
    const int CX = IW / 2;
    const int CY = IH / 2;
    const int R  = 64;          /* sphere radius */
    const int R2 = R * R;
    /* Light direction (toward upper-left) */
    const int LX = -6, LY = -8, LZ = 12;
    const int LMAG2 = LX*LX + LY*LY + LZ*LZ;

    for (uint32_t ty = 0; ty < MODE3_TH; ty++) {
        for (uint32_t tx = 0; tx < MODE3_TW; tx++) {
            uint8_t d[64];
            for (int py = 0; py < 8; py++) {
                for (int px = 0; px < 8; px++) {
                    int x = (int)(tx * 8 + px);
                    int y = (int)(ty * 8 + py);
                    int dx = x - CX;
                    int dy = y - CY;
                    int r2 = dx*dx + dy*dy;
                    uint8_t color;
                    if (r2 < R2) {
                        /* Inside sphere — compute normal-dot-light for
                         * smooth shading. z = sqrt(R^2 - dx^2 - dy^2).
                         * Use a quick integer sqrt approximation. */
                        int zsq = R2 - r2;
                        int z = 0;
                        while ((z+1)*(z+1) <= zsq) z++;
                        /* Dot product of (dx,dy,z) with light direction. */
                        int dot = dx*LX + dy*LY + z*LZ;
                        if (dot < 0) dot = 0;
                        /* Normalize roughly: dot ranges 0..R*sqrt(LMAG2). */
                        int shade = (dot * 200) / (R * 12);  /* 12 ~= sqrt(LMAG2) */
                        if (shade > 200) shade = 200;
                        /* Map shade [0..200] to palette indices [160..255]
                         * (warm sun ramp in pal_sphere). */
                        color = (uint8_t)(160 + (shade * 95) / 200);
                    } else if (r2 < R2 + 600) {
                        /* Outer glow halo */
                        int glow = (R2 + 600 - r2) / 6;   /* 0..100 */
                        color = (uint8_t)(120 + glow / 2);
                    } else {
                        /* Background sky gradient + sparse stars */
                        color = (uint8_t)((y * 31) / IH);   /* 0..30 deep blue */
                        if (((x * 7 + y * 13) % 191) == 0) color = 250;  /* star */
                        else if (((x * 11 + y * 17) % 263) == 0) color = 200;
                    }
                    d[py * 8 + px] = color;
                }
            }
            int tile_idx = (int)MODE3_FIRST_TILE + (int)(ty * MODE3_TW + tx);
            set8(s_tiles_8bpp, tile_idx, d);
        }
    }
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
/* Canonical scene palettes.
 *
 *   pal_city (Mode 1 BG1+BG2): SMW-style colorful platformer — sky
 *     blue, grass greens, brown brick ground, gray windows, white.
 *   pal_castle (Mode 3 BG2):   black silhouette with gray highlights
 *     for sunset castle.
 *   sp2_*  (2bpp 4-color):     themed 4-color sets for Mode 0 layers
 *     (each layer gets a distinct palette so 4 simultaneous BGs read
 *     as 4 different planes, not one muddy mess).
 */
static const uint32_t pal_city[16]={
    0,
    0xFF6A8AC0,  /* 1 sky light */
    0xFFFFFFFF,  /* 2 white (cloud highlight) */
    0xFF4A6AB0,  /* 3 sky mid */
    0xFF8B5A2B,  /* 4 dirt */
    0xFF5A3A1A,  /* 5 dirt dark */
    0xFF202020,  /* 6 outline / window */
    0xFF80B0E0,  /* 7 window frame */
    0xFFC0805A,  /* 8 brick light */
    0xFF704020,  /* 9 brick dark */
    0xFF60D060,  /* 10 grass bright */
    0xFF308030,  /* 11 grass dark */
    0xFF1A4A1A,  /* 12 tree dark / dark green */
    0xFF5A3A20,  /* 13 trunk brown */
    0xFF80C880,  /* 14 hill light */
    0xFF408040,  /* 15 hill shadow */
};
static const uint32_t pal_castle[16]={
    0,
    0xFF0A0A1A,  /* 1 black silhouette */
    0xFF1A1A2A,  /* 2 */
    0xFF2A2A3A,  /* 3 */
    0xFF101020,  /* 4 */
    0xFF050510,  /* 5 */
    0xFF000000,  /* 6 absolute black */
    0xFF80808A,  /* 7 (unused here) */
    0xFF202030,  /* 8 wall light */
    0xFF101020,  /* 9 wall dark */
    0xFF60606A,0xFF40404A,0xFF202028,0xFF181820,0xFF101018,0xFF080810,
};
/* Forest palette unused for now but defined so Mode 1 can swap.
 * Mode 1 currently uses pal_city. */
/* Mode 0 4-layer palettes: each BG gets distinct 4-color set. */
static const uint32_t sp2_bg4[4]={0,0xFF101830,0xFF2030A0,0xFF6090E0};  /* deep sky */
static const uint32_t sp2_bg3[4]={0,0xFF301030,0xFF902080,0xFFE060A0};  /* magenta mountains */
static const uint32_t sp2_bg2[4]={0,0xFF0A300A,0xFF308030,0xFF60D060};  /* green trees */
static const uint32_t sp2_bg1[4]={0,0xFF302010,0xFF806040,0xFFC8A878};  /* tan foreground */
/* Mode 6 road palette: asphalt + lane markings.
 *   0 = transparent  1 = dark asphalt  2 = mid asphalt  3 = light asphalt
 *   4 = yellow lane divider  5 = white lane edge  6 = dust/shoulder
 *   7+ = unused (padded). */
static const uint32_t pal_road[16]={
    0,
    0xFF202028,  /* 1 dark asphalt */
    0xFF303038,  /* 2 mid asphalt */
    0xFF484850,  /* 3 light asphalt */
    0xFFE0C040,  /* 4 yellow lane */
    0xFFE0E0E0,  /* 5 white edge */
    0xFF604030,  /* 6 dust shoulder */
    0xFF202028,0xFF202028,0xFF202028,0xFF202028,0xFF202028,
    0xFF202028,0xFF202028,0xFF202028,0xFF202028,
};
/* Mode 5 hi-res text palette: high contrast white-on-dark.
 * Defined as 16 entries because Mode 5's BG1 is 4bpp (the lib reads
 * indices 0..15 from the palette). The hi-res font is rendered with
 * just 4 distinct values, so 4-15 are duplicated. */
static const uint32_t sp2_hires[16]={
    0,
    0xFF000810,                     /* 1 dark backdrop */
    0xFF80C8FF,                     /* 2 cyan accent */
    0xFFFFFFFF,                     /* 3 white text */
    /* 4-15: padded with dark/light alternation to avoid garbage if tiles
     * accidentally use higher indices. */
    0xFF000810,0xFF000810,0xFF000810,0xFF000810,
    0xFF000810,0xFF000810,0xFF000810,0xFF000810,
    0xFF000810,0xFF000810,0xFF000810,0xFF000810,
};
/* Generic 4-color for legacy tiles (unused now but referenced by P1). */
static const uint32_t s_pal2[4]={0,0xFF203050,0xFF4080C0,0xFFA0C0E0};
static const uint32_t s_pal2b[4]={0,0xFF405060,0xFF607080,0xFF8090A0};
/* Legacy 16-color palettes still referenced by older P2 cases that
 * we'll be overwriting below. Keep around until those cases are
 * rebuilt to use pal_city / pal_castle. */
static const uint32_t pal_sky[16]={0,0xFF5A7AAA,0xFF3A5A8A,0xFF7A6A5A,0xFF40A040,0xFF305020,0xFF508030,[7 ... 15]=0xFF2A4A7A};
static const uint32_t pal_earth[16]={0,0xFF6A5A4A,0xFF5A4A3A,0xFF4A3A2A,0xFF40A030,0xFF306020,0xFF4A7A3A,[7 ... 15]=0xFF3A2A1A};

/* Shared 256-color rainbow (legacy — used by Mode 4 unless overridden) */
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

/* Mode 3 palette: tuned for a smooth-shaded sphere on starfield.
 *   0           = transparent
 *   1-31        = deep night blue (sky gradient top→bottom)
 *   32-119      = blue/purple sky transitions (unused for now, reserved)
 *   120-159     = orange halo around the sphere
 *   160-255     = warm sun ramp (dark red → orange → yellow → white)
 *
 * 256 distinct entries give the sphere ~95 distinct shading values —
 * banding becomes invisible to the eye. A 16-color mode rendering
 * this with even the best dither would look obviously paletted. */
static uint32_t pal_sunset[256];
static void init_pal_sunset(void)
{
    pal_sunset[0] = 0;
    /* 1..31: deep night blue gradient (sky top → mid) */
    for (int i = 1; i < 32; i++) {
        int t = i;                                       /* 1..31 */
        int r = 4  + t * 2;       /* 6..66  */
        int g = 0  + t * 2;       /* 2..62  */
        int b = 18 + t * 4;       /* 22..142 */
        pal_sunset[i] = 0xFF000000 | (r<<16) | (g<<8) | b;
    }
    /* 32..119: reserved deep purples (transit region; unused by our
     * sphere image but defined so adjacent indices are smooth). */
    for (int i = 32; i < 120; i++) {
        int t = i - 32;
        int r = 66  + t;          int g = 62  - t / 2;   int b = 142 - t;
        if (r > 255) r = 255; if (g < 0) g = 0; if (b < 0) b = 0;
        pal_sunset[i] = 0xFF000000 | (r<<16) | (g<<8) | b;
    }
    /* 120..159: dim halo around sphere — dark red/orange glow. */
    for (int i = 120; i < 160; i++) {
        int t = i - 120;                                 /* 0..39 */
        int r = 80  + t * 3;       /* 80..197 */
        int g = 20  + t;           /* 20..59  */
        int b = 10  + t / 4;       /* 10..19  */
        if (r > 255) r = 255;
        pal_sunset[i] = 0xFF000000 | (r<<16) | (g<<8) | b;
    }
    /* 160..255: smooth sphere shading ramp — dark red → orange →
     * bright yellow → near-white at the specular highlight. */
    for (int i = 160; i < 256; i++) {
        int t = i - 160;                                 /* 0..95 */
        int r = 120 + t * 140 / 95;       /* 120..260 → clamp */
        int g = 30  + t * 220 / 95;       /* 30..250 */
        int b = 10  + t * 230 / 95;       /* 10..240 */
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;
        pal_sunset[i] = 0xFF000000 | (r<<16) | (g<<8) | b;
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
    case 0: /* MODE 0 canonical: 4 simultaneous BGs, 4 colors each.
             * Final Fantasy battle-screen look: 4 visibly distinct
             * depth planes, each in its own 4-color palette so it's
             * obvious you're getting 4 BGs (not just 1 or 2).
             *   BG4 (back):  deep blue sky + stars (slowest scroll)
             *   BG3:         magenta mountain ridges
             *   BG2:         green forest treeline
             *   BG1 (front): tan/brown ground texture */
        for(int y=0;y<MH;y++) for(int x=0;x<MW;x++){
            /* BG4: sky + sparse stars (top 18 rows) */
            map_d[y*MW+x] = (y<14) ? SNES_ENTRY(2,0,0,0,0) :
                            (y<18) ? SNES_ENTRY(6,0,0,0,0) : 0;
            /* BG3: distant mountain silhouette (rows 14-22) */
            map_c[y*MW+x] = (y>=14 && y<22) ? SNES_ENTRY(7,0,0,0,0) : 0;
            /* BG2: forest treeline (rows 19-25, sparse) */
            map_b[y*MW+x] = (y>=19 && y<25 && (x%3==0)) ? SNES_ENTRY(4,0,0,0,0) :
                            (y>=19 && y<25)            ? SNES_ENTRY(1,0,0,0,0) : 0;
            /* BG1: foreground ground (rows 24+) */
            map_a[y*MW+x] = (y>=24) ? SNES_ENTRY(3,0,0,0,0) : 0;
        }
        bg[3]=(snes_bg_t){s_tiles_2bpp,map_d,sp2_bg4,0,0,MW,MH,2,1};
        bg[2]=(snes_bg_t){s_tiles_2bpp,map_c,sp2_bg3,0,0,MW,MH,2,1};
        bg[1]=(snes_bg_t){s_tiles_2bpp,map_b,sp2_bg2,0,0,MW,MH,2,1};
        bg[0]=(snes_bg_t){s_tiles_2bpp,map_a,sp2_bg1,0,0,MW,MH,2,1}; break;
    case 1: /* MODE 1 canonical: SMW-style 3-layer parallax.
             *   BG3 (back, 4-col): sky + clouds + sun (barely scrolls)
             *   BG2 (16-col):     green hills with trees (medium scroll)
             *   BG1 (16-col):     brick/dirt ground (full scroll)
             * Speed differentiation set in render(). */
        for(int y=0;y<MH;y++) for(int x=0;x<MW;x++){
            /* BG1 ground: bottom 4 rows, brick + grass top edge */
            int top_row = (y==25);
            int ground  = (y>=25);
            map_a[y*MW+x] = ground ? SNES_ENTRY(top_row?4:3, 0, 0, 0, 0) : 0;
            /* BG2 mid: hills with trees scattered on top */
            int hill_top  = 20 + ((x*3)%5);             /* uneven hill skyline */
            int on_hill   = (y >= hill_top);
            int has_tree  = (y == hill_top-2) && ((x%7)==3);
            map_b[y*MW+x] = has_tree ? SNES_ENTRY(13,0,0,0,0) :
                            on_hill  ? SNES_ENTRY((y==hill_top)?12:1,0,0,0,0) : 0;
            /* BG3 sky: sparse clouds + sun. 2bpp = 4 colors. */
            int cloud_row = (y==3 || y==4);
            int cloud_pos = ((x%9)==2);
            int sun_pos   = (y==2 && (x==6 || x==7));
            map_c[y*MW+x] = sun_pos ? SNES_ENTRY(5,0,0,0,0) :
                            (cloud_row && cloud_pos) ? SNES_ENTRY(4,0,0,0,0) :
                            (y<6) ? SNES_ENTRY(1,0,0,0,0) : 0;
        }
        bg[0]=(snes_bg_t){s_tiles_4bpp,map_a,pal_city,0,0,MW,MH,4,1};
        bg[1]=(snes_bg_t){s_tiles_4bpp,map_b,pal_city,0,0,MW,MH,4,1};
        bg[2]=(snes_bg_t){s_tiles_2bpp,map_c,sp2_bg4,0,0,MW,MH,2,1}; break;
    case 2: /* MODE 2 canonical: Battlemaniacs / Panel de Pon style —
             * VERTICAL POSTS (fence / columns) where column-offset
             * makes each post bob up/down independently. The structured
             * vertical shapes make the per-column offset visible as
             * a wave of bobbing pillars, not a floating squiggle. */
        for(int y=0;y<MH;y++) for(int x=0;x<MW;x++){
            /* Posts every 3 columns, 12 rows tall, sitting on a
             * ground line at row 22. Each post is 1 tile wide so
             * each gets its own column offset. */
            int is_post  = (x % 3 == 1);
            int post_top = 10, post_bot = 22;
            int in_post  = is_post && (y >= post_top && y < post_bot);
            int on_ground = (y == 22);
            int below     = (y > 22);
            if (in_post)       map_a[y*MW+x] = SNES_ENTRY(3,0,0,0,0);   /* brick post */
            else if (on_ground)map_a[y*MW+x] = SNES_ENTRY(4,0,0,0,0);   /* grass top */
            else if (below)    map_a[y*MW+x] = SNES_ENTRY(3,0,0,0,0);   /* dirt */
            else               map_a[y*MW+x] = 0;
            /* BG2: solid sky everywhere — gives posts contrast */
            map_b[y*MW+x] = SNES_ENTRY(1,0,0,0,0);
        }
        bg[0]=(snes_bg_t){s_tiles_4bpp,map_a,pal_city,0,0,MW,MH,4,1};
        bg[1]=(snes_bg_t){s_tiles_4bpp,map_b,pal_city,0,0,MW,MH,4,1};
        ofs.col_offset=offset_table; ofs.vertical=1; break;
    case 3: /* MODE 3 canonical: 256-color SMOOTH-SHADED SPHERE on a
             * starfield. Generated procedurally at boot — every 8×8
             * region of the visible 256×224 area is a unique 8bpp
             * tile. The sphere uses ~95 distinct shading values
             * (palette indices 160..255) — a 16-color mode would
             * have to dither badly to imitate it. */
        for(int y=0;y<MH;y++) for(int x=0;x<MW;x++){
            if ((uint32_t)y < MODE3_TH && (uint32_t)x < MODE3_TW) {
                int idx = (int)MODE3_FIRST_TILE + y * (int)MODE3_TW + x;
                map_a[y*MW+x] = SNES_ENTRY(idx, 0, 0, 0, 0);
            } else {
                map_a[y*MW+x] = 0;
            }
            map_b[y*MW+x] = 0;
        }
        bg[0]=(snes_bg_t){s_tiles_8bpp,map_a,pal_sunset,0,0,MW,MH,8,1};
        bg[1]=(snes_bg_t){s_tiles_4bpp,map_b,pal_castle,0,0,MW,MH,4,1}; break;
    case 4: /* MODE 4: same sphere bitmap as Mode 3 + per-tile column
             * offset → vertical shimmer. The sphere becomes wavy like
             * a reflection on disturbed water. Same image data,
             * column-offset is what differentiates the demo. */
        for(int y=0;y<MH;y++) for(int x=0;x<MW;x++){
            if ((uint32_t)y < MODE3_TH && (uint32_t)x < MODE3_TW) {
                int idx = (int)MODE3_FIRST_TILE + y * (int)MODE3_TW + x;
                map_a[y*MW+x] = SNES_ENTRY(idx, 0, 0, 0, 0);
            } else {
                map_a[y*MW+x] = 0;
            }
            map_b[y*MW+x] = 0;
        }
        bg[0]=(snes_bg_t){s_tiles_8bpp,map_a,pal_sunset,0,0,MW,MH,8,1};
        bg[1]=(snes_bg_t){s_tiles_2bpp,map_b,s_pal2,0,0,MW,MH,2,1};
        ofs.col_offset=offset_table; ofs.vertical=1; break;
    case 5: /* ORIGINAL — window-frame + color bands (kept; was good) */
        for(int y=0;y<MH;y++) for(int x=0;x<MW;x++){
            int is_frame = (y<2||y>=30||x<2||x>=30);
            map_a[y*MW+x]=is_frame?SNES_ENTRY(1,0,0,0,0):0;
            int band=(y%6);
            int tile=(band<2)?3:(band<4)?4:2;
            map_b[y*MW+x]=SNES_ENTRY(tile,0,0,0,0);
        }
        bg[0]=(snes_bg_t){s_tiles_2bpp,map_a,s_pal2,0,0,MW,MH,2,1};
        bg[1]=(snes_bg_t){s_tiles_4bpp,map_b,pal_city,0,0,MW,MH,4,1}; break;
    case 6: /* MODE 6 canonical: hi-res road (RPM Racing). Asphalt
             * bands with yellow lane divider every 4 rows + white
             * edges. Column offset bends the road into a snaking
             * pseudo-3D motion. pal_road = real road colors. */
        for(int y=0;y<MH;y++) for(int x=0;x<MW;x++){
            int t;
            if (x == 0 || x == MW-1)    t = 19;  /* white edge tile (idx 5) */
            else if ((y % 4) == 2)       t = 18;  /* yellow lane divider (idx 4) */
            else if ((y % 2) == 0)       t = 16;  /* mid asphalt (idx 2) */
            else                          t = 17;  /* light asphalt (idx 3) */
            map_a[y*MW+x] = SNES_ENTRY(t, 0, 0, 0, 0);
        }
        bg[0]=(snes_bg_t){s_tiles_4bpp,map_a,pal_road,0,0,MW,MH,4,1};
        ofs.col_offset=offset_table; ofs.vertical=1; break;
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

#define AUTH 1  /* 1 = render at native 256x224 with pillarbox */

static void render(int mode, int phase, uint32_t *fb, uint32_t *ovl, uint32_t mf)
{
    uint32_t bd = phase==0 ? 0xFF1A1A2A : 0xFF2A4A7A;
    switch(mode){
    case 0:  /* Mode 0: 4-deep parallax (background = slow, foreground = fast).
              * Speed ratio 1:2:4:8 makes each plane visibly distinct. */
        if (phase == 1) {
            bg[3].scroll_x = (int32_t)(mf / 8);  /* back sky/stars */
            bg[2].scroll_x = (int32_t)(mf / 4);  /* mountains */
            bg[1].scroll_x = (int32_t)(mf / 2);  /* trees */
            bg[0].scroll_x = (int32_t)mf;        /* ground */
        } else {
            bg[0].scroll_x=(int32_t)mf; bg[1].scroll_x=(int32_t)(mf*3/4);
            bg[2].scroll_x=(int32_t)(mf/2);
        }
        snes_mode0_render(fb,ovl,LCD_W,LCD_H,bd,&bg[0],&bg[1],&bg[2],&bg[3],AUTH); break;
    case 1:  /* Mode 1: classic 3-plane SMW parallax.
              * BG3 (clouds) barely moves; BG2 (hills) slow; BG1 (ground) full. */
        if (phase == 1) {
            bg[2].scroll_x = (int32_t)(mf / 6);   /* sky/clouds — barely moves */
            bg[1].scroll_x = (int32_t)(mf / 3);   /* hills — medium */
            bg[0].scroll_x = (int32_t)mf;          /* ground — full */
            bg[0].scroll_y = 0; bg[1].scroll_y = 0; bg[2].scroll_y = 0;
        } else {
            bg[0].scroll_x=(int32_t)mf;bg[1].scroll_x=(int32_t)(mf/2);
            bg[0].scroll_y=80;bg[1].scroll_y=60;
        }
        snes_mode1_render(fb,ovl,LCD_W,LCD_H,bd,&bg[0],&bg[1],&bg[2],AUTH); break;
    case 2: if(phase==1){bg[0].scroll_y=40;bg[1].scroll_y=40;}
        else{bg[0].scroll_x=(int32_t)mf;bg[1].scroll_x=(int32_t)(mf/2);}
        for(int c=0;c<MAX_COLS;c++){int ph=(int)(mf+c*2)&(SIN_N-1);offset_table[c]=sin_tbl[ph];}
        snes_mode2_render(fb,ovl,LCD_W,LCD_H,bd,&bg[0],&bg[1],&ofs,AUTH); break;
    case 3: bg[0].scroll_x=(int32_t)mf;bg[1].scroll_x=(int32_t)(mf/2);bg[0].scroll_y=0;
        snes_mode3_render(fb,ovl,LCD_W,LCD_H,phase==1?0xFF102030:bd,&bg[0],&bg[1],AUTH); break;
    case 4: bg[0].scroll_x=(int32_t)mf;bg[1].scroll_x=(int32_t)(mf/2);
        for(int c=0;c<MAX_COLS;c++){int ph=(int)(mf*2+c*3)&(SIN_N-1);offset_table[c]=sin_tbl[ph];}
        snes_mode4_render(fb,ovl,LCD_W,LCD_H,phase==1?0xFF081828:bd,&bg[0],&bg[1],&ofs,AUTH); break;
    case 5: bg[0].scroll_x=(int32_t)mf;bg[1].scroll_x=(int32_t)(mf/2);
        snes_mode5_render(fb,ovl,LCD_W,LCD_H,bd,&bg[0],&bg[1],AUTH); break;
    case 6: for(int c=0;c<MAX_COLS;c++){int ph=(int)(mf*3+c*4)&(SIN_N-1);offset_table[c]=sin_tbl[ph];}
        snes_mode6_render(fb,ovl,LCD_W,LCD_H,bd,&bg[0],&ofs,AUTH); break;
    case 7: {
        /* Mode 7: centered rect with pillarbox (authentic) or full-screen */
        uint32_t rw = AUTH ? SNES_NATIVE_W : LCD_W;
        uint32_t rh = AUTH ? SNES_NATIVE_H : LCD_H;
        uint32_t x0 = (LCD_W - rw) / 2;
        uint32_t y0 = (LCD_H - rh) / 2;
        /* Clear overlay rect */
        for (uint32_t py = y0; py < y0 + rh; py++) {
            uint32_t *row = ovl + py * LCD_W + x0;
            for (uint32_t px = 0; px < rw; px++) row[px] = 0;
        }
        uint8_t angle=(uint8_t)mf; int32_t ca=m7_cos[angle],sa=m7_sin[angle];
        int32_t cx=MW/2*4096,cy=MH/2*4096;
        /* Sky gradient (upper half of rect) */
        for (uint32_t ly = 0; ly < rh/2; ly++) {
            uint32_t *row = fb + (y0+ly) * LCD_W + x0;
            uint32_t g = 0x60 + ly;
            uint32_t sky = 0xFF000000 | (g/3<<16) | (g/2<<8) | g;
            for (uint32_t px = 0; px < rw; px++) row[px] = sky;
        }
        /* Affine ground (lower half of rect) */
        for (uint32_t ly = rh/2; ly < rh; ly++) {
            uint32_t *row = fb + (y0+ly) * LCD_W + x0;
            int32_t d = (int32_t)(ly - rh/2 + 1);
            int32_t scale = 4096*8/d;
            int32_t du = (ca*scale)>>12, dv = (sa*scale)>>12;
            int32_t u0v = cx - (int32_t)(rw/2)*du - (int32_t)(rh/4)*dv;
            int32_t v0v = cy - (int32_t)(rw/2)*dv + (int32_t)(rh/4)*du;
            iso_scanline(row, m7_map, m7_lut, u0v, v0v, du, dv, rw/2);
            if (ly + 1 < rh) memcpy_neon(row + LCD_W, row, rw * 4);
        }
        break; }
    }
}

/* ================================================================
 *  AUDIO PLUMBING
 * ================================================================ */

#define MIX_BUF_SIZE 4096  /* matches audio.c's actual buffer */
#define MIX_BUF_MASK (MIX_BUF_SIZE - 1)
extern int16_t  mix_buf[MIX_BUF_SIZE];
extern volatile uint32_t mix_wr;
extern volatile uint32_t mix_rd;
extern volatile uint32_t audio_underruns;
extern volatile uint32_t audio_transitions;

static vgm_player_t vgm;
static int16_t stereo[2048];

#include "hstimer.h"

/* ISR-driven VGM producer. Self-paces to mix_buf consumption so
 * NES/GB chips (heavier per-sample than SN76489/YM2612) stay clean
 * even when video render eats more CPU than expected. Tested clean
 * in Act 1 (jukebox: songs play to end). Act 2 (mode-change-driven
 * track reloads every 7 s) has a known issue — Mode 0 BG eventually
 * scrambles + occasional buzz — that we accept as a limit. */
#define AUDIO_TICK_SCANLINES   64u                   /* ~270 Hz */
#define AUDIO_TICK_TARGET      ((MIX_BUF_SIZE * 3) / 4)
#define AUDIO_TICK_MAX         600u
static volatile int audio_isr_armed   = 0;
static volatile int audio_isr_running = 0;

static void vgm_fill_isr(uint32_t n)
{
    uint32_t done = 0;
    while (done < n) {
        uint32_t chunk = n - done;
        if (chunk > 128) chunk = 128;
        vgm_render(&vgm, stereo + done * 2, chunk);
        for (uint32_t i = 0; i < chunk; i++) {
            if ((mix_wr - mix_rd) >= (MIX_BUF_SIZE - 2)) continue;
            mix_buf[mix_wr & MIX_BUF_MASK] = stereo[(done + i) * 2];     mix_wr++;
            mix_buf[mix_wr & MIX_BUF_MASK] = stereo[(done + i) * 2 + 1]; mix_wr++;
        }
        done += chunk;
    }
}

/* Wrapper kept for the bring-up prefill before ISR is armed. */
static void vgm_fill(uint32_t n) { vgm_fill_isr(n); }

static void vgm_audio_tick(void)
{
    if (!audio_isr_armed) return;
    audio_isr_running = 1;
    __sync_synchronize();
    if (audio_isr_armed) {
        uint32_t depth = mix_wr - mix_rd;
        if (depth < AUDIO_TICK_TARGET) {
            uint32_t deficit_frames = (AUDIO_TICK_TARGET - depth) / 2;
            if (deficit_frames > AUDIO_TICK_MAX) deficit_frames = AUDIO_TICK_MAX;
            if (deficit_frames > 0) vgm_fill_isr(deficit_frames);
        }
    }
    __sync_synchronize();
    audio_isr_running = 0;
}

/* Fast-forward: render VGM to advance chip state without producing audio */
static void vgm_seek(uint32_t samples)
{
    int16_t dummy[256];
    while (samples > 0) {
        uint32_t chunk = samples > 128 ? 128 : samples;
        vgm_render(&vgm, dummy, chunk);
        samples -= chunk;
    }
}

/* Track catalog */
typedef struct {
    const char *name;
    const uint8_t *data;
    uint32_t len;
    uint32_t seek_to;  /* samples to fast-forward for Act 2 */
} track_t;

static const track_t tracks[] = {
    { "OutRun - Step on Beat",   vgm_step_on_beat, vgm_step_on_beat_len, 48000 * 5 },
    { "DuckTales - The Moon",    vgm_the_moon,     vgm_the_moon_len,     48000 * 8 },
    { "Pokemon - Gym Battle",    vgm_pokemon_gym,  vgm_pokemon_gym_len,  65536 * 3 },
};
#define NUM_TRACKS 3

static uint32_t spf = 800;

static uint32_t current_rate = 0;

static void load_track(int idx)
{
    /* Disarm ISR + spin-wait until any in-flight render exits BEFORE
     * vgm_unload frees the chip pointers. Without the spin, use-after-
     * free corrupts whatever the freed chunk gets reused for. */
    int was_armed = audio_isr_armed;
    audio_isr_armed = 0;
    __sync_synchronize();
    while (audio_isr_running) { __asm__ volatile("nop"); }

    vgm_stop(&vgm);
    vgm_unload(&vgm);
    vgm_load(&vgm, tracks[idx].data, tracks[idx].len, 0);

    uint32_t new_rate = vgm.sample_rate;
    if (new_rate != current_rate) {
        /* Rate change: audio_set_rate stops DMA and reconfigures PLL */
        audio_set_rate(new_rate);
        current_rate = new_rate;
        vgm_play(&vgm);
        spf = new_rate / 60;
        vgm_fill(spf);
        vgm_fill(spf);
        audio_start();
    } else {
        vgm_play(&vgm);
        spf = new_rate / 60;
    }

    __sync_synchronize();
    audio_isr_armed = was_armed;

    uart_puts("  >> "); uart_puts(tracks[idx].name);
    uart_puts(" @ "); uart_putdec(new_rate); uart_puts("Hz");
    uart_puts(" spf="); uart_putdec(spf); uart_puts("\n");
}

/* ================================================================
 *  MAIN
 * ================================================================ */

void main(void)
{
    uart_puts("\n\n=== Jupiter AV Demo ===\n");
    uart_puts("Act 1: VGM jukebox over SNES modes\n");
    uart_puts("Act 2: Triggered songs on mode changes\n\n");

    /* Init everything */
    timer_init();
    mmu_init();
    irq_init();           /* GIC setup (before audio_start enables DMA IRQ) */
    init_tiles();
    gen_mode3_sphere();      /* fills s_tiles_8bpp[MODE3_FIRST_TILE..] */
    init_pal_256();
    init_pal_sunset();
    init_sin();
    init_m7();
    video_init();
    audio_init();

    /* Authentic mode: pre-clear both framebuffers + overlay to black so
     * the pillarbox stays untouched for the lifetime of the demo. */
    memset32_neon(FB0_ADDR, 0, LCD_FB_BYTES);
    memset32_neon(FB1_ADDR, 0, LCD_FB_BYTES);
    memset32_neon(OVL_ADDR, 0, LCD_FB_BYTES);
    memset32_neon(OVL1_ADDR, 0, LCD_FB_BYTES);
    dcache_clean_fb(FB0_ADDR);
    dcache_clean_fb(FB1_ADDR);
    dcache_clean_fb(OVL_ADDR);
    dcache_clean_fb(OVL1_ADDR);

    /* Load first track + pre-fill */
    int cur_track = 0;
    vgm_load(&vgm, tracks[0].data, tracks[0].len, 0);
    audio_set_rate(vgm.sample_rate);
    current_rate = vgm.sample_rate;
    vgm_play(&vgm);
    spf = vgm.sample_rate / 60;
    vgm_fill(spf);
    vgm_fill(spf);
    audio_start();

    /* Arm ISR-driven producer (heavy NES/GB chips need cadence-
     * driven production for menu-binary icache pressure). */
    hstimer_init();
    audio_isr_armed = 1;
    hstimer_set_repeating(0, AUDIO_TICK_SCANLINES, vgm_audio_tick);

    irq_global_enable();  /* Unmask IRQs — DMA ISR now services audio */

    uart_puts("  >> "); uart_puts(tracks[0].name);
    uart_puts(" @ "); uart_putdec(vgm.sample_rate); uart_puts("Hz");
    uart_puts(" spf="); uart_putdec(spf); uart_puts("\n");

    vi1_label(slots[0].label);
    uint32_t back = FB1_ADDR, front = FB0_ADDR, frame = 0;
    uint32_t ovl_back = OVL1_ADDR, ovl_front = OVL_ADDR;
    int cur_slot = -1;
    int act = 1;  /* 1 = jukebox, 2 = triggered */

    while (1) {
        uint32_t t0 = timer_read();

        /* ---- Visual: SNES showcase ---- */
        int slot = (int)((frame / FRAMES_PER_SLOT) % TOTAL_SLOTS);
        uint32_t mf = frame % FRAMES_PER_SLOT;
        int mode = slots[slot].mode, phase = slots[slot].phase;

        if (slot != cur_slot) {
            if (phase == 0) setup_p1(mode); else setup_p2(mode);
            cur_slot = slot;
            vi1_label(slots[slot].label);
            uart_puts(phase == 0 ? "[CHK] " : "[SCN] ");
            uart_puts(slots[slot].label); uart_puts("\n");

            /* ---- Act 2: trigger a song seek on each mode change ---- */
            if (act == 2) {
                int tidx = slot % NUM_TRACKS;
                load_track(tidx);
                uart_puts("  [seek "); uart_putdec(tracks[tidx].seek_to / (vgm.sample_rate ? vgm.sample_rate : 1));
                uart_puts("s]\n");
                vgm_seek(tracks[tidx].seek_to);
            }
        }

        render(mode, phase, (uint32_t *)back, (uint32_t *)ovl_back, mf);
        dcache_clean_fb(back);
        dcache_clean_fb(ovl_back);
        uint32_t t_render = timer_read();

        /* ---- Audio production runs in hstimer ISR ---- */
        uint32_t t_audio = timer_read();

        /* ---- Act 1: advance jukebox when a song ends ---- */
        if (act == 1 && vgm.cur_loop >= 1) {
            cur_track++;
            if (cur_track >= NUM_TRACKS) {
                /* All songs played — switch to Act 2 */
                act = 2;
                uart_puts("\n=== ACT 2: Triggered mode ===\n");
                int tidx = cur_slot % NUM_TRACKS;
                load_track(tidx);
                vgm_seek(tracks[tidx].seek_to);
            } else {
                load_track(cur_track);
            }
        }

        /* ---- Swap + pace ---- */
        VI_TOP_LADDR0(0) = back;
        UI_ADDR(0) = ovl_back;
        video_commit();
        { uint32_t tmp = back; back = front; front = tmp; }
        { uint32_t tmp = ovl_back; ovl_back = ovl_front; ovl_front = tmp; }

        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667)
            ;  /* Pace to 60fps — DMA ISR services audio */

        if ((frame % 60) == 0) {
            uint32_t us_render = ticks_to_us(timer_elapsed(t0, t_render));
            uint32_t us_audio  = ticks_to_us(timer_elapsed(t_render, t_audio));
            uint32_t us_total  = ticks_to_us(timer_elapsed(t0, timer_read()));
            uint32_t fill = mix_wr - mix_rd;
            uart_puts("f="); uart_putdec(frame);
            uart_puts(" act="); uart_putdec(act);
            uart_puts(" s="); uart_putdec((uint32_t)slot);
            uart_puts("/"); uart_puts(slots[slot].label);
            uart_puts(" vid="); uart_putdec(us_render);
            uart_puts("us aud="); uart_putdec(us_audio);
            uart_puts("us tot="); uart_putdec(us_total);
            uart_puts("us fill="); uart_putdec(fill);
            uart_puts(" under="); uart_putdec(audio_underruns);
            uart_puts(" trk="); uart_putdec((uint32_t)cur_track);
            uart_puts(" loop="); uart_putdec(vgm.cur_loop);
            uart_puts(" ticks="); uart_putdec(vgm.total_ticks);
            uart_puts("\n");
        }
        frame++;
    }
}
