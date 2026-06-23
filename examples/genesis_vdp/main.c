/*
 * Jupiter SDK — Genesis VDP Demo
 *
 * Five things the Genesis VDP can do natively:
 *   Plane A (front): city with buildings + ground, scrolls at 1×
 *   Plane B (back):  distant mountains + sky with per-line hscroll
 *                    (heat haze / water ripple effect)
 *   Window:          fixed HUD banner at the top of the screen
 *   Sprites:         animated character bouncing around the city
 *   Authentic:       rendered at native 320×224 with pillarbox
 *
 * All tiles are 4bpp. 4 palettes × 16 colors in CRAM. Plane A renders
 * to the UI0 hardware overlay, Plane B to VI0, and the DE2 blender
 * composites them for free.
 *
 * Build: make GAME=examples/genesis_vdp/main.c
 */
#include "jupiter.h"
#include "genesis.h"
#include "../../libvgm/vgm_player.h"
#include "fighting_back.h"

/* ================================================================
 *  TILE DATA — 4bpp, 32 bytes per tile
 * ================================================================ */

#define TILE_COUNT 32
static uint8_t tiledata[TILE_COUNT * 32];

/* Pack 64 nibble pixels (8x8) into 32 bytes of 4bpp data. */
static void set_tile(int tidx, const uint8_t px[64])
{
    uint8_t *d = tiledata + tidx * 32;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 4; c++)
            d[r*4+c] = (px[r*8+c*2] << 4) | px[r*8+c*2+1];
}

static void fill_tile(int tidx, uint8_t idx)
{
    uint8_t px[64]; for (int i=0;i<64;i++) px[i]=idx;
    set_tile(tidx, px);
}

static void init_tiles(void)
{
    /* Tile 0 is always blank/transparent — skip it. */

    /* --- Plane B (mountain/sky) tiles, palette 1 --- */
    /* 1: distant mountain solid */
    fill_tile(1, 1);
    /* 2: mountain slope left  */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x] = (x >= (7-y)) ? 1 : 0;
      set_tile(2, d); }
    /* 3: mountain slope right */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x] = (x <= y) ? 1 : 0;
      set_tile(3, d); }
    /* 4: snow cap */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x] = (y < 3) ? 2 : 1;
      set_tile(4, d); }
    /* 5: cloud */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        int cx=x-4, cy=y-3;
        d[y*8+x] = (cx*cx + cy*cy*2 < 10) ? 3 : 0;
      } set_tile(5, d); }
    /* 6: sky gradient band */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x] = (y < 4) ? 4 : 5;
      set_tile(6, d); }

    /* --- Plane A (city) tiles, palette 0 --- */
    /* 8: dirt ground */
    fill_tile(8, 1);
    /* 9: grass on dirt */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x] = (y < 2) ? 2 : ((y==2 && ((x+y)&1)) ? 2 : 1);
      set_tile(9, d); }
    /* 10: brick wall */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        if (y==3 || y==7) d[y*8+x] = 3;
        else if (y<3 && x==7) d[y*8+x] = 3;
        else if (y>3 && x==3) d[y*8+x] = 3;
        else d[y*8+x] = 4;
      } set_tile(10, d); }
    /* 11: brick cap (roof line) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x] = (y < 2) ? 5 : (y == 2) ? 3 : 4;
      set_tile(11, d); }
    /* 12: window (lit) */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x] = (x==0||x==7||y==0||y==7) ? 3 :
                   (x>=2&&x<=5&&y>=2&&y<=5) ? 6 : 4;
      set_tile(12, d); }
    /* 13: door */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x] = (x==0||x==7) ? 3 :
                   (y==0) ? 3 :
                   (x>=2&&x<=5) ? 7 : 4;
      set_tile(13, d); }
    /* 14: streetlamp post */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x] = (x==3||x==4) ? 3 : (y<2 && x>=2 && x<=5) ? 6 : 0;
      set_tile(14, d); }

    /* --- Window/HUD tiles, palette 2 --- */
    /* 16: HUD background */
    fill_tile(16, 1);
    /* 17: HUD top border */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x] = (y == 7) ? 3 : (y < 2) ? 2 : 1;
      set_tile(17, d); }
    /* 18: HUD bottom border */
    { uint8_t d[64]; for(int y=0;y<8;y++) for(int x=0;x<8;x++)
        d[y*8+x] = (y == 0) ? 3 : (y > 5) ? 2 : 1;
      set_tile(18, d); }

    /* --- Sprite tiles: 16×16 character, 4 tiles column-major (palette 0) ---
     * Layout within the sprite (pixel coordinates):
     *   (0..7,0..7)  = tile 20   (0..7,8..15) = tile 21
     *   (8..15,0..7) = tile 22   (8..15,8..15)= tile 23
     * Uses palette 0 (Plane A's palette): 1=brown, 6=yellow (highlight),
     * 3=dark (outline), 7=red (body). */
    {
        /* Full 16×16 character bitmap */
        static const uint8_t sprite[16*16] = {
            0,0,0,3,3,3,3,0,0,3,3,3,3,0,0,0,
            0,0,3,6,6,6,6,3,3,6,6,6,6,3,0,0,
            0,3,6,6,6,6,6,6,6,6,6,6,6,6,3,0,
            0,3,6,3,6,6,3,6,6,3,6,6,3,6,3,0,
            0,3,6,6,6,6,6,6,6,6,6,6,6,6,3,0,
            0,3,6,6,3,3,3,3,3,3,3,3,6,6,3,0,
            0,0,3,6,6,6,6,6,6,6,6,6,6,3,0,0,
            0,0,0,3,3,3,3,3,3,3,3,3,3,0,0,0,
            0,0,0,7,7,7,7,7,7,7,7,7,7,0,0,0,
            0,0,7,7,1,1,7,7,7,7,1,1,7,7,0,0,
            0,0,7,7,1,1,7,7,7,7,1,1,7,7,0,0,
            0,0,7,7,7,7,7,7,7,7,7,7,7,7,0,0,
            0,0,7,7,7,7,7,7,7,7,7,7,7,7,0,0,
            0,0,0,7,7,7,0,0,0,0,7,7,7,0,0,0,
            0,0,0,3,3,3,0,0,0,0,3,3,3,0,0,0,
            0,0,3,3,3,0,0,0,0,0,0,3,3,3,0,0,
        };
        /* Unpack into 4 8×8 tiles in column-major order:
         * tile 20 = left column top, 21 = left column bottom,
         * tile 22 = right column top, 23 = right column bottom. */
        for (int tx = 0; tx < 2; tx++) {
            for (int ty = 0; ty < 2; ty++) {
                uint8_t t[64];
                for (int y = 0; y < 8; y++)
                    for (int x = 0; x < 8; x++)
                        t[y*8+x] = sprite[(ty*8 + y) * 16 + (tx*8 + x)];
                set_tile(20 + tx*2 + ty, t);
            }
        }
    }
}

/* ================================================================
 *  PALETTES — 4 × 16 = 64 entries (Genesis CRAM)
 * ================================================================ */

static const uint32_t cram[64] = {
    /* Palette 0 (Plane A: city) */
    0x00000000, 0xFF6A4A2A, 0xFF5ABA4A, 0xFF202028,
    0xFF7A6050, 0xFFB09A7A, 0xFFF0D060, 0xFF8A3820,
    0xFFA07040, 0xFF403020, 0xFF606060, 0xFF8A8A8A,
    0xFFC0C0C0, 0xFF2A2A3A, 0xFF1A1A28, 0xFFFFFFFF,

    /* Palette 1 (Plane B: mountains/sky) */
    0x00000000, 0xFF3A4A6A, 0xFFE8F0FA, 0xFFFFFFFF,
    0xFF5A7AAA, 0xFF7A9ACA, 0xFF2A3A5A, 0xFF1A2A4A,
    0xFF405878, 0xFF3A5068, 0xFF202838, 0xFF101828,
    0xFF0A0F1A, 0xFF4A6080, 0xFF5A7090, 0xFF6A80A0,

    /* Palette 2 (Window: HUD) */
    0x00000000, 0xFF1A2A48, 0xFF4080C0, 0xFFA0D0F0,
    0xFF2A3A5A, 0xFF3A4A6A, 0xFF5A6A8A, 0xFFFFFFFF,
    0xFF0A1A38, 0xFF0A1A38, 0xFF0A1A38, 0xFF0A1A38,
    0xFF0A1A38, 0xFF0A1A38, 0xFF0A1A38, 0xFF0A1A38,

    /* Palette 3 (unused) */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
};

/* ================================================================
 *  NAMETABLES
 * ================================================================ */

#define MW 64
#define MH 64
static uint16_t plane_a_map[MH * MW];
static uint16_t plane_b_map[MH * MW];

#define WIN_W 40    /* 40 tiles wide = 320 pixels */
#define WIN_H 3     /* 3 tiles tall  = 24 pixels  */
static uint16_t window_map[WIN_H * WIN_W];

static void build_maps(void)
{
    /* --- Plane B: sky + distant mountains (palette 1) --- */
    for (int y=0;y<MH;y++) for (int x=0;x<MW;x++) {
        uint16_t t = 0;
        /* Sky gradient band at the top */
        if (y < 4) t = GEN_ENTRY(6, 1, 0, 0);
        /* Scattered clouds in the upper sky */
        else if (y >= 5 && y <= 8 && ((x * 7 + y * 3) % 17 == 0))
            t = GEN_ENTRY(5, 1, 0, 0);
        /* Mountain range: heights vary with column */
        else {
            int h = 14 + ((x * 5 + 3) % 8);
            int half = h / 2;
            if (y == h - half && (x & 3) == 0) t = GEN_ENTRY(4, 1, 0, 0);
            else if (y > h - half && y < h) {
                int slope = (x & 7);
                if (slope < 3)      t = GEN_ENTRY(2, 1, 0, 0);
                else if (slope > 4) t = GEN_ENTRY(3, 1, 0, 0);
                else                t = GEN_ENTRY(1, 1, 0, 0);
            }
            else if (y >= h) t = GEN_ENTRY(1, 1, 0, 0);
        }
        plane_b_map[y * MW + x] = t;
    }

    /* --- Plane A: city foreground (palette 0) --- */
    for (int y=0;y<MH;y++) for (int x=0;x<MW;x++) {
        uint16_t t = 0;
        /* Ground at the bottom */
        if (y == 30)      t = GEN_ENTRY(9,  0, 0, 0);
        else if (y > 30)  t = GEN_ENTRY(8,  0, 0, 0);
        /* Buildings: each 10-column block is one building */
        else {
            int bx = x % 10;
            int btype = (x / 10) % 4;
            int bh = (btype == 0) ? 7 : (btype == 1) ? 9 :
                     (btype == 2) ? 5 : 8;
            int btop = 30 - bh;
            if (bx >= 1 && bx <= 6 && y >= btop && y < 30) {
                if (y == btop) t = GEN_ENTRY(11, 0, 0, 0);  /* roof cap */
                else if ((y-btop)==2 && (bx==2 || bx==4))
                    t = GEN_ENTRY(12, 0, 0, 0);             /* window */
                else if ((y-btop)==4 && (bx==2 || bx==4))
                    t = GEN_ENTRY(12, 0, 0, 0);             /* window */
                else if (y == 29 && bx == 3)
                    t = GEN_ENTRY(13, 0, 0, 0);             /* door */
                else t = GEN_ENTRY(10, 0, 0, 0);             /* wall */
            }
            /* Streetlamps between buildings */
            else if (y == 29 && (x % 10 == 8))
                t = GEN_ENTRY(14, 0, 0, 0);
        }
        plane_a_map[y * MW + x] = t;
    }

    /* --- Window: HUD banner, 40×3 tiles --- */
    for (int y=0;y<WIN_H;y++) for (int x=0;x<WIN_W;x++) {
        uint16_t t;
        if (y == 0)      t = GEN_ENTRY(17, 2, 0, 0); /* top border */
        else if (y == WIN_H-1) t = GEN_ENTRY(18, 2, 0, 0); /* bottom border */
        else             t = GEN_ENTRY(16, 2, 0, 0);
        window_map[y * WIN_W + x] = t;
    }
}

/* ================================================================
 *  MAIN
 * ================================================================ */

/* ================================================================
 *  AUDIO (VGM jukebox, matches av_demo pattern)
 * ================================================================ */

#define MIX_BUF_SIZE 4096
#define MIX_BUF_MASK (MIX_BUF_SIZE - 1)
extern int16_t  mix_buf[MIX_BUF_SIZE];
extern volatile uint32_t mix_wr;
extern volatile uint32_t mix_rd;

static vgm_player_t vgm;
static int16_t stereo[2048];

static void vgm_fill(uint32_t n)
{
    uint32_t done = 0;
    while (done < n) {
        uint32_t chunk = n - done;
        if (chunk > 128) chunk = 128;
        vgm_render(&vgm, stereo + done * 2, chunk);
        for (uint32_t i = 0; i < chunk; i++) {
            if ((mix_wr - mix_rd) >= (MIX_BUF_SIZE - 2))
                continue;
            mix_buf[mix_wr & MIX_BUF_MASK] = stereo[(done + i) * 2];
            mix_wr++;
            mix_buf[mix_wr & MIX_BUF_MASK] = stereo[(done + i) * 2 + 1];
            mix_wr++;
        }
        done += chunk;
    }
}

/* Tiny sine table, 64 entries, amplitude ±6. Used for line-hscroll waves
 * on Plane B and sprite bob. */
#define SIN_N 64
static int16_t sin_tbl[SIN_N];
static void init_sin(void)
{
    static const int8_t q[17] = {0,1,2,2,3,4,4,5,5,5,6,6,6,6,6,6,6};
    for (int i = 0; i <= 16; i++) {
        sin_tbl[i]          =  q[i];
        sin_tbl[32 - i]     =  q[i];
        sin_tbl[32 + i]     = -q[i];
        sin_tbl[(64 - i) & (SIN_N - 1)] = -q[i];
    }
    sin_tbl[0] = 0;
}

/* Line-hscroll table for Plane B (heat-haze effect). One entry per scanline. */
static int16_t b_line_hscroll[GEN_NATIVE_H];

void main(void)
{
    uart_puts("\n\n=== Jupiter SDK — Genesis VDP Demo ===\n");
    uart_puts("Plane A (city) + Plane B (haze mountains) + Window + Sprites\n");
    uart_puts("Audio: Thunder Force IV - Fighting Back (Stage 1A)\n\n");

    timer_init();
    mmu_init();
    irq_init();           /* GIC setup (before audio_start enables DMA IRQ) */
    init_tiles();
    build_maps();
    init_sin();

    genesis_plane_t plane_a = {
        .tiles = tiledata, .map = plane_a_map, .cram = cram,
        .map_w = MW, .map_h = MH, .enabled = 1,
    };
    genesis_plane_t plane_b = {
        .tiles = tiledata, .map = plane_b_map, .cram = cram,
        .line_hscroll = b_line_hscroll,
        .map_w = MW, .map_h = MH, .enabled = 1,
    };
    genesis_window_t window = {
        .map = window_map,
        .x = 0, .y = 0, .w = WIN_W * 8, .h = WIN_H * 8,
        .map_w = WIN_W, .map_h = WIN_H,
    };

    /* One 16×16 sprite (4 tiles starting at index 20, column-major) */
    genesis_sprite_t sprites[1] = {
        {
            .x = 80, .y = 140,
            .tile = 20, .w = 2, .h = 2,
            .pal = 0, .fliph = 0, .flipv = 0,
            .enabled = 1,
        },
    };

    video_init();
    audio_init();

    /* Load track, set DAC rate, prime the mix buffer, kick DMA */
    vgm_load(&vgm, vgm_fighting_back, vgm_fighting_back_len, 0);
    audio_set_rate(vgm.sample_rate);
    vgm_play(&vgm);
    uint32_t spf = vgm.sample_rate / 60;
    vgm_fill(spf);
    vgm_fill(spf);
    audio_start();
    irq_global_enable();  /* Unmask IRQs — DMA ISR services the audio buffer */

    uart_puts("Display active. 320x224 authentic, sprites + line-hscroll.\n");
    uart_puts("Audio @ "); uart_putdec(vgm.sample_rate);
    uart_puts("Hz, spf="); uart_putdec(spf); uart_puts("\n\n");

    uint32_t back_fb  = FB1_ADDR,  front_fb  = FB0_ADDR;
    uint32_t back_ovl = OVL1_ADDR, front_ovl = OVL_ADDR;
    uint32_t frame = 0;
    uint32_t backdrop = 0xFF1A2A4A;

    /* Authentic mode: pre-clear all four buffers to black so the pillarbox
     * stays untouched for the lifetime of the demo. Both VI0 and UI0 are
     * double-buffered to avoid tearing (important for moving sprites). */
    memset32_neon(FB0_ADDR,  0, LCD_FB_BYTES);
    memset32_neon(FB1_ADDR,  0, LCD_FB_BYTES);
    memset32_neon(OVL_ADDR,  0, LCD_FB_BYTES);
    memset32_neon(OVL1_ADDR, 0, LCD_FB_BYTES);
    dcache_clean_fb(FB0_ADDR);
    dcache_clean_fb(FB1_ADDR);
    dcache_clean_fb(OVL_ADDR);
    dcache_clean_fb(OVL1_ADDR);

    while (1) {
        uint32_t t0 = timer_read();

        /* Parallax: Plane A scrolls at 1×, Plane B at 1/2× */
        plane_a.scroll_x = (int32_t)frame;
        plane_b.scroll_x = (int32_t)(frame / 2);
        plane_a.scroll_y = 128;
        plane_b.scroll_y = 96;

        /* Heat-haze / water-ripple wave on Plane B: per-scanline hscroll,
         * wave amplitude grows toward the bottom of the frame. */
        for (int y = 0; y < GEN_NATIVE_H; y++) {
            int phase = (frame + y * 2) & (SIN_N - 1);
            int amp = 1 + y / 32;  /* 1..7 depending on row */
            b_line_hscroll[y] = sin_tbl[phase] * amp;
        }

        /* Animate the sprite: bob up/down and skip across the screen */
        int bob = sin_tbl[(frame * 2) & (SIN_N - 1)];
        sprites[0].x = 40 + (int16_t)((frame / 2) % (GEN_NATIVE_W - 16));
        sprites[0].y = 140 + bob;
        sprites[0].fliph = ((frame / 60) & 1);

        genesis_render((uint32_t *)back_fb, (uint32_t *)back_ovl,
                       LCD_W, LCD_H, backdrop,
                       &plane_a, &plane_b, &window,
                       sprites, 1, 1);

        dcache_clean_fb(back_fb);
        dcache_clean_fb(back_ovl);
        uint32_t t1 = timer_read();

        /* Fill one frame's worth of audio samples */
        vgm_fill(spf);
        /* Loop the song if it ends */
        if (vgm.cur_loop >= 1) {
            vgm_stop(&vgm);
            vgm_load(&vgm, vgm_fighting_back, vgm_fighting_back_len, 0);
            vgm_play(&vgm);
        }

        /* Atomic swap of both VI0 and UI0 */
        VI_TOP_LADDR0(0) = back_fb;
        UI_ADDR(0) = back_ovl;
        video_commit();
        { uint32_t tmp = back_fb;  back_fb  = front_fb;  front_fb  = tmp; }
        { uint32_t tmp = back_ovl; back_ovl = front_ovl; front_ovl = tmp; }

        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667) ;

        if ((frame % 120) == 0) {
            uart_puts("f="); uart_putdec(frame);
            uart_puts(" render="); uart_putdec(ticks_to_us(timer_elapsed(t0, t1)));
            uart_puts("us\n");
        }
        frame++;
    }
}
