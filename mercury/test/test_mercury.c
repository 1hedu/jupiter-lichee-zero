/*
 * Host-side verification for mercury_raster.c + mercury_displaylist.c
 *
 * 1. Renders the self-test cube scene through the display-list path,
 *    writes a PNG so the output can be eyeballed.
 * 2. Clip-equivalence: random triangles with coords in [-5000,5000],
 *    fixed rasterizer (y-clipped walk) vs an int64 reference that walks
 *    every row with bounds-checked plotting. Must be pixel-identical.
 * 3. Hostile display lists: oversized texture claims, truncated
 *    commands, garbage streams — run under ASan, must not crash.
 *
 * A pthread stands in for core 1, servicing j32x_swap_apply() so
 * CMD_SCENE_END's blocking j32x_swap() completes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <zlib.h>
#include "jupiter32x.h"

/* ---------- fake core 1 ---------- */
static volatile int applier_run = 1;
static void *applier_thread(void *arg)
{
    (void)arg;
    while (applier_run)
        j32x_swap_apply();
    return NULL;
}

/* ---------- tiny PNG writer (RGB, zlib) ---------- */
static void put32(FILE *f, uint32_t v)
{
    fputc(v >> 24, f); fputc(v >> 16, f); fputc(v >> 8, f); fputc(v, f);
}
static void png_chunk(FILE *f, const char *tag, const uint8_t *data, uint32_t len)
{
    put32(f, len);
    fwrite(tag, 1, 4, f);
    if (len) fwrite(data, 1, len, f);
    uint32_t crc = crc32(0, (const uint8_t *)tag, 4);
    if (len) crc = crc32(crc, data, len);
    put32(f, crc);
}
static void write_png(const char *path, const uint8_t *fb, int w, int h)
{
    /* R3G3B2 -> RGB rows with filter byte */
    uint8_t *raw = malloc((size_t)h * (w * 3 + 1));
    for (int y = 0; y < h; y++) {
        uint8_t *row = raw + (size_t)y * (w * 3 + 1);
        *row++ = 0;
        for (int x = 0; x < w; x++) {
            uint8_t p = fb[y * w + x];
            uint8_t r = p & 0xE0, g = (p & 0x1C) << 3, b = (p & 0x03) << 6;
            r |= (r >> 3) | (r >> 6);
            g |= (g >> 3) | (g >> 6);
            b |= (b >> 2) | (b >> 4) | (b >> 6);
            *row++ = r; *row++ = g; *row++ = b;
        }
    }
    uLongf zlen = compressBound((uLong)h * (w * 3 + 1));
    uint8_t *z = malloc(zlen);
    compress(z, &zlen, raw, (uLong)h * (w * 3 + 1));

    FILE *f = fopen(path, "wb");
    static const uint8_t sig[8] = {137, 'P', 'N', 'G', 13, 10, 26, 10};
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13];
    ihdr[0] = w >> 24; ihdr[1] = w >> 16; ihdr[2] = w >> 8; ihdr[3] = w;
    ihdr[4] = h >> 24; ihdr[5] = h >> 16; ihdr[6] = h >> 8; ihdr[7] = h;
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    png_chunk(f, "IHDR", ihdr, 13);
    png_chunk(f, "IDAT", z, (uint32_t)zlen);
    png_chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(raw); free(z);
}

/* ---------- int64 reference rasterizer (no y-clip, walks all rows) ---------- */
static uint8_t ref_fb[J32X_FB_SIZE];

static void ref_hline(int x0, int x1, int y, uint8_t color, int w, int h)
{
    if (y < 0 || y >= h) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (x0 < 0) x0 = 0;
    if (x1 >= w) x1 = w - 1;
    if (x0 > x1) return;
    memset(&ref_fb[y * w + x0], color, x1 - x0 + 1);
}

static int16_t rclamp(int32_t v)
{
    if (v < J32X_COORD_MIN) return J32X_COORD_MIN;
    if (v > J32X_COORD_MAX) return J32X_COORD_MAX;
    return (int16_t)v;
}

static void ref_tri(j32x_vertex_t v0, j32x_vertex_t v1, j32x_vertex_t v2,
                    uint8_t color, int w, int h)
{
    v0.x = rclamp(v0.x); v0.y = rclamp(v0.y);
    v1.x = rclamp(v1.x); v1.y = rclamp(v1.y);
    v2.x = rclamp(v2.x); v2.y = rclamp(v2.y);

    j32x_vertex_t tmp;
    if (v0.y > v1.y) { tmp = v0; v0 = v1; v1 = tmp; }
    if (v0.y > v2.y) { tmp = v0; v0 = v2; v2 = tmp; }
    if (v1.y > v2.y) { tmp = v1; v1 = v2; v2 = tmp; }

    int64_t y0 = v0.y, y1 = v1.y, y2 = v2.y;
    if (y0 == y2) return;
    if (y2 <= 0 || y0 >= h) return;

    int64_t dy_long = y2 - y0, dy_top = y1 - y0, dy_bot = y2 - y1;
    /* same truncating division as the fixed code */
    int64_t dx_long = (int64_t)(v2.x - v0.x) * 65536 / dy_long;
    int64_t x_long = (int64_t)v0.x * 65536;

    if (dy_top > 0) {
        int64_t dx_short = (int64_t)(v1.x - v0.x) * 65536 / dy_top;
        int64_t x_short = (int64_t)v0.x * 65536;
        for (int64_t y = y0; y < y1; y++) {
            ref_hline((int)(x_long >> 16), (int)(x_short >> 16), (int)y,
                      color, w, h);
            x_long += dx_long;
            x_short += dx_short;
        }
    }
    if (dy_bot > 0) {
        int64_t dx_short = (int64_t)(v2.x - v1.x) * 65536 / dy_bot;
        int64_t x_short = (int64_t)v1.x * 65536;
        for (int64_t y = y1; y < y2; y++) {
            ref_hline((int)(x_long >> 16), (int)(x_short >> 16), (int)y,
                      color, w, h);
            x_long += dx_long;
            x_short += dx_short;
        }
    }
}

/* ---------- deterministic PRNG ---------- */
static uint32_t rng_state = 0x1234567;
static uint32_t rnd(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state >> 8;
}

/* ---------- cube scene via display list (mirrors mercury_main.c) ---------- */
static const int16_t cube_verts[8][3] = {
    {-64, -64, -64}, { 64, -64, -64}, { 64,  64, -64}, {-64,  64, -64},
    {-64, -64,  64}, { 64, -64,  64}, { 64,  64,  64}, {-64,  64,  64},
};
static const uint8_t cube_tris[12][3] = {
    {0,1,2}, {0,2,3}, {4,6,5}, {4,7,6}, {0,4,5}, {0,5,1},
    {2,6,7}, {2,7,3}, {0,3,7}, {0,7,4}, {1,5,6}, {1,6,2},
};
static const uint8_t face_colors[6] = {
    RGB332(0xFF,0x00,0x00), RGB332(0x00,0xFF,0x00), RGB332(0x00,0x00,0xFF),
    RGB332(0xFF,0xFF,0x00), RGB332(0xFF,0x00,0xFF), RGB332(0x00,0xFF,0xFF),
};
static const int8_t sin_tab[256] = {
    0,3,6,9,12,16,19,22,25,28,31,34,37,40,43,46,
    49,51,54,57,60,63,65,68,71,73,76,78,81,83,85,88,
    90,92,94,96,98,100,102,104,106,107,109,111,112,113,115,116,
    117,118,120,121,122,122,123,124,125,125,126,126,126,127,127,127,
    127,127,127,127,126,126,126,125,125,124,123,122,122,121,120,118,
    117,116,115,113,112,111,109,107,106,104,102,100,98,96,94,92,
    90,88,85,83,81,78,76,73,71,68,65,63,60,57,54,51,
    49,46,43,40,37,34,31,28,25,22,19,16,12,9,6,3,
    0,-3,-6,-9,-12,-16,-19,-22,-25,-28,-31,-34,-37,-40,-43,-46,
    -49,-51,-54,-57,-60,-63,-65,-68,-71,-73,-76,-78,-81,-83,-85,-88,
    -90,-92,-94,-96,-98,-100,-102,-104,-106,-107,-109,-111,-112,-113,-115,-116,
    -117,-118,-120,-121,-122,-122,-123,-124,-125,-125,-126,-126,-126,-127,-127,-127,
    -127,-127,-127,-127,-126,-126,-126,-125,-125,-124,-123,-122,-122,-121,-120,-118,
    -117,-116,-115,-113,-112,-111,-109,-107,-106,-104,-102,-100,-98,-96,-94,-92,
    -90,-88,-85,-83,-81,-78,-76,-73,-71,-68,-65,-63,-60,-57,-54,-51,
    -49,-46,-43,-40,-37,-34,-31,-28,-25,-22,-19,-16,-12,-9,-6,-3,
};
static int isin(uint8_t a) { return sin_tab[a]; }
static int icos(uint8_t a) { return sin_tab[(a + 64) & 0xFF]; }

static void rotate_project(const int16_t in[3], uint8_t ax, uint8_t ay,
                           int16_t *sx, int16_t *sy, int32_t *z_out)
{
    int32_t x = in[0], y = in[1], z = in[2];
    int32_t ca = icos(ay), sa = isin(ay);
    int32_t nx = (x * ca - z * sa) >> 7;
    int32_t nz = (x * sa + z * ca) >> 7;
    x = nx; z = nz;
    ca = icos(ax); sa = isin(ax);
    int32_t ny = (y * ca - z * sa) >> 7;
    nz = (y * sa + z * ca) >> 7;
    y = ny; z = nz;
    z += 256;
    if (z < 16) z = 16;
    *sx = (int16_t)(J32X_WIDTH / 2  + (x * 200) / z);
    *sy = (int16_t)(J32X_HEIGHT / 2 + (y * 200) / z);
    *z_out = z;
}

static uint32_t dl_put_tri(uint8_t *dl, uint32_t pos, uint8_t color,
                           int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                           int16_t x2, int16_t y2)
{
    j32x_cmd_tri_t c;
    c.cmd = CMD_TRI; c.color = color;
    c.v[0].x = x0; c.v[0].y = y0;
    c.v[1].x = x1; c.v[1].y = y1;
    c.v[2].x = x2; c.v[2].y = y2;
    memcpy(dl + pos, &c, sizeof(c));
    return pos + sizeof(c);
}

static int build_cube_dl(uint8_t *dl, uint8_t angle)
{
    uint32_t pos = 0;
    dl[pos++] = CMD_CLEAR;
    dl[pos++] = RGB332(0x00, 0x00, 0x40);

    int16_t pv[8][2];
    int32_t pz[8];
    for (int i = 0; i < 8; i++)
        rotate_project(cube_verts[i], angle / 2, angle, &pv[i][0], &pv[i][1], &pz[i]);

    int32_t face_z[6];
    int order[6] = {0,1,2,3,4,5};
    for (int f = 0; f < 6; f++) {
        int t0 = f*2;
        face_z[f] = (pz[cube_tris[t0][0]] + pz[cube_tris[t0][1]] +
                     pz[cube_tris[t0][2]] + pz[cube_tris[t0+1][2]]) / 4;
    }
    for (int i = 0; i < 5; i++)
        for (int j = i+1; j < 6; j++)
            if (face_z[order[i]] < face_z[order[j]]) {
                int t = order[i]; order[i] = order[j]; order[j] = t;
            }
    for (int fi = 0; fi < 6; fi++) {
        int f = order[fi];
        for (int t = 0; t < 2; t++) {
            int ti = f*2 + t;
            pos = dl_put_tri(dl, pos, face_colors[f],
                pv[cube_tris[ti][0]][0], pv[cube_tris[ti][0]][1],
                pv[cube_tris[ti][1]][0], pv[cube_tris[ti][1]][1],
                pv[cube_tris[ti][2]][0], pv[cube_tris[ti][2]][1]);
        }
    }
    dl[pos++] = CMD_SCENE_END;
    return (int)pos;
}

int main(void)
{
    memset(&g_state, 0, sizeof(g_state));
    g_state.front = 0;
    g_state.back = 1;
    g_state.width = J32X_WIDTH;
    g_state.height = J32X_HEIGHT;

    pthread_t th;
    pthread_create(&th, NULL, applier_thread, NULL);

    /* ---- Test 1: cube frame through the display-list path ---- */
    static uint8_t dl[4096];
    int len = build_cube_dl(dl, 40);
    j32x_exec_displaylist(dl, (uint32_t)len);
    write_png("mercury_selftest.png", g_state.fb[g_state.front],
              J32X_WIDTH, J32X_HEIGHT);
    printf("test 1: cube display list (%d bytes) -> mercury_selftest.png\n", len);

    /* ---- Test 2: clip equivalence, random hostile triangles ---- */
    int w = J32X_WIDTH, h = J32X_HEIGHT;
    int fails = 0;
    for (int iter = 0; iter < 20000; iter++) {
        j32x_vertex_t a, b, c;
        /* mix of near-screen and far-offscreen coords, incl. ±32767 */
        int span = (iter % 3 == 0) ? 65536 : (iter % 3 == 1) ? 10000 : 700;
        a.x = (int16_t)((int)(rnd() % span) - span/2);
        a.y = (int16_t)((int)(rnd() % span) - span/2);
        b.x = (int16_t)((int)(rnd() % span) - span/2);
        b.y = (int16_t)((int)(rnd() % span) - span/2);
        c.x = (int16_t)((int)(rnd() % span) - span/2);
        c.y = (int16_t)((int)(rnd() % span) - span/2);

        memset(g_state.fb[g_state.back], 0, (size_t)w * h);
        memset(ref_fb, 0, (size_t)w * h);
        j32x_raster_tri(a, b, c, 0xE3);
        ref_tri(a, b, c, 0xE3, w, h);
        if (memcmp(g_state.fb[g_state.back], ref_fb, (size_t)w * h) != 0) {
            if (fails < 5)
                fprintf(stderr,
                    "MISMATCH tri (%d,%d)(%d,%d)(%d,%d)\n",
                    a.x, a.y, b.x, b.y, c.x, c.y);
            fails++;
        }
    }
    printf("test 2: clip equivalence, 20000 random triangles, %d mismatches\n",
           fails);

    /* ---- Test 3: hostile display lists ---- */
    static uint8_t evil[4096];

    /* 3a: texture claiming 0xFFFF bytes with 4 bytes of payload */
    evil[0] = CMD_TEXTURE; evil[1] = 0; evil[2] = 0xFF; evil[3] = 0xFF;
    j32x_exec_displaylist(evil, 8);
    /* 3b: texture size just over the slot */
    evil[0] = CMD_TEXTURE; evil[1] = 7;
    evil[2] = (J32X_TEX_MAX + 1) & 0xFF; evil[3] = (J32X_TEX_MAX + 1) >> 8;
    j32x_exec_displaylist(evil, sizeof(evil));
    if (g_state.tex[7].valid) { fprintf(stderr, "oversize texture accepted!\n"); fails++; }
    /* 3c: texture bad slot */
    evil[0] = CMD_TEXTURE; evil[1] = 200; evil[2] = 16; evil[3] = 0;
    j32x_exec_displaylist(evil, 64);
    /* 3d: truncated TRI (header claims more than buffer) */
    evil[0] = CMD_TRI;
    j32x_exec_displaylist(evil, 3);
    /* 3e: SET_RES to 0 and to huge */
    j32x_cmd_set_res_t sr = { CMD_SET_RES, 0, 0 };
    memcpy(evil, &sr, sizeof(sr));
    j32x_exec_displaylist(evil, sizeof(sr));
    sr.w = 0xFFFF; sr.h = 0xFFFF;
    memcpy(evil, &sr, sizeof(sr));
    j32x_exec_displaylist(evil, sizeof(sr));
    if (g_state.width != J32X_WIDTH || g_state.height != J32X_HEIGHT) {
        fprintf(stderr, "bad SET_RES accepted (%ux%u)!\n",
                g_state.width, g_state.height);
        fails++;
    }
    /* 3f: pure garbage streams (no SCENE_END deadlock risk: applier runs) */
    for (int iter = 0; iter < 20000; iter++) {
        uint32_t n = rnd() % sizeof(evil);
        for (uint32_t i = 0; i < n; i++) evil[i] = (uint8_t)rnd();
        j32x_exec_displaylist(evil, n);
    }
    /* restore sane res in case fuzz hit a valid SET_RES */
    g_state.width = J32X_WIDTH; g_state.height = J32X_HEIGHT;
    printf("test 3: hostile display lists survived (ASan clean if silent)\n");

    /* ---- Test 4: SET_RES small then draw full-range ---- */
    sr.cmd = CMD_SET_RES; sr.w = 160; sr.h = 144;
    memcpy(evil, &sr, sizeof(sr));
    uint32_t pos = sizeof(sr);
    evil[pos++] = CMD_CLEAR; evil[pos++] = 0x1C;
    pos = dl_put_tri(evil, pos, 0xFF, -3000, -3000, 3000, 100, -100, 3000);
    evil[pos++] = CMD_SCENE_END;
    j32x_exec_displaylist(evil, pos);
    printf("test 4: SET_RES 160x144 + oversized tri ok\n");

    applier_run = 0;
    pthread_join(th, NULL);

    if (fails) { printf("FAILED: %d\n", fails); return 1; }
    printf("ALL PASS\n");
    return 0;
}
