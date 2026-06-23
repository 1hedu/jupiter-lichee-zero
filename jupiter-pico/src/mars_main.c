/*
 * Jupiter Mars — Main entry point
 *
 * Core 0: Receives display lists from V3s over SPI, executes them
 * Core 1: Continuously scans out the front framebuffer as parallel CSI
 *
 * The Pico is a stateless polygon GPU. The V3s sends scene commands,
 * the Pico rasterizes and streams pixels back through CSI.
 * Like the 32X was to the Genesis — Mars to Jupiter.
 */

#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "jupiter32x.h"

/* Display list receive buffer */
static uint8_t dl_recv_buf[4096];

/* ============================================================
 * Core 1 entry: DVI scanout
 * ============================================================ */
static void core1_entry(void)
{
    j32x_scanout_start();
    j32x_scanout_loop();  /* never returns */
}

/* ============================================================
 * Self-test: render a spinning wireframe cube
 * Used when no SPI master is connected.
 * ============================================================ */

/* 8 vertices of a unit cube, 8.8 fixed-point */
static const int16_t cube_verts[8][3] = {
    {-64, -64, -64}, { 64, -64, -64}, { 64,  64, -64}, {-64,  64, -64},
    {-64, -64,  64}, { 64, -64,  64}, { 64,  64,  64}, {-64,  64,  64},
};

/* 12 triangles (2 per face) */
static const uint8_t cube_tris[12][3] = {
    {0,1,2}, {0,2,3},   /* front */
    {4,6,5}, {4,7,6},   /* back */
    {0,4,5}, {0,5,1},   /* bottom */
    {2,6,7}, {2,7,3},   /* top */
    {0,3,7}, {0,7,4},   /* left */
    {1,5,6}, {1,6,2},   /* right */
};

static const uint8_t face_colors[6] = {
    RGB332(0xFF, 0x00, 0x00),  /* front  — red */
    RGB332(0x00, 0xFF, 0x00),  /* back   — green */
    RGB332(0x00, 0x00, 0xFF),  /* bottom — blue */
    RGB332(0xFF, 0xFF, 0x00),  /* top    — yellow */
    RGB332(0xFF, 0x00, 0xFF),  /* left   — magenta */
    RGB332(0x00, 0xFF, 0xFF),  /* right  — cyan */
};

/* Simple sine table, 256 entries, amplitude 127 */
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

static inline int isin(uint8_t a) { return sin_tab[a]; }
static inline int icos(uint8_t a) { return sin_tab[(a + 64) & 0xFF]; }

static void rotate_project(const int16_t in[3], uint8_t ax, uint8_t ay,
                            int16_t *sx, int16_t *sy, int32_t *z_out)
{
    int32_t x = in[0], y = in[1], z = in[2];

    /* Rotate around Y */
    int32_t ca = icos(ay), sa = isin(ay);
    int32_t nx = (x * ca - z * sa) >> 7;
    int32_t nz = (x * sa + z * ca) >> 7;
    x = nx; z = nz;

    /* Rotate around X */
    ca = icos(ax); sa = isin(ax);
    int32_t ny = (y * ca - z * sa) >> 7;
    nz = (y * sa + z * ca) >> 7;
    y = ny; z = nz;

    /* Perspective projection */
    z += 256;  /* push away from camera */
    if (z < 16) z = 16;
    *sx = (int16_t)(J32X_WIDTH / 2  + (x * 200) / z);
    *sy = (int16_t)(J32X_HEIGHT / 2 + (y * 200) / z);
    *z_out = z;
}

static void self_test_frame(uint8_t angle)
{
    j32x_raster_clear(COL_BLACK);

    int16_t projected[8][2];
    int32_t proj_z[8];

    for (int i = 0; i < 8; i++) {
        rotate_project(cube_verts[i], angle / 2, angle,
                       &projected[i][0], &projected[i][1], &proj_z[i]);
    }

    /* Painter's algorithm: sort faces by average Z (back to front) */
    int32_t face_z[6];
    int face_order[6] = {0, 1, 2, 3, 4, 5};
    for (int f = 0; f < 6; f++) {
        int t0 = f * 2, t1 = f * 2 + 1;
        int i0 = cube_tris[t0][0], i1 = cube_tris[t0][1], i2 = cube_tris[t0][2];
        int i3 = cube_tris[t1][2];
        face_z[f] = (proj_z[i0] + proj_z[i1] + proj_z[i2] + proj_z[i3]) / 4;
    }

    /* Simple bubble sort on 6 elements */
    for (int i = 0; i < 5; i++)
        for (int j = i + 1; j < 6; j++)
            if (face_z[face_order[i]] < face_z[face_order[j]]) {
                int t = face_order[i];
                face_order[i] = face_order[j];
                face_order[j] = t;
            }

    /* Draw faces back to front */
    for (int fi = 0; fi < 6; fi++) {
        int f = face_order[fi];
        uint8_t col = face_colors[f];

        for (int t = 0; t < 2; t++) {
            int ti = f * 2 + t;
            j32x_vertex_t va = {projected[cube_tris[ti][0]][0],
                                projected[cube_tris[ti][0]][1]};
            j32x_vertex_t vb = {projected[cube_tris[ti][1]][0],
                                projected[cube_tris[ti][1]][1]};
            j32x_vertex_t vc = {projected[cube_tris[ti][2]][0],
                                projected[cube_tris[ti][2]][1]};
            j32x_raster_tri(va, vb, vc, col);
        }
    }

    j32x_swap();
}

/* ============================================================
 * Core 0 main: SPI receive loop with self-test fallback
 * ============================================================ */
int main(void)
{
    stdio_init_all();

    /* Init state */
    memset(&g_state, 0, sizeof(g_state));
    g_state.front = 0;
    g_state.back = 1;
    g_state.width = J32X_WIDTH;
    g_state.height = J32X_HEIGHT;

    /* Init SPI slave */
    j32x_spi_init();

    /* Launch Core 1: CSI scanout */
    multicore_launch_core1(core1_entry);

    /* Give Core 1 a moment to start PIO */
    sleep_ms(10);

    uint8_t angle = 0;
    bool got_spi = false;  /* have we ever received a display list? */

    while (1) {
        /* Try to receive a display list from V3s */
        int len = j32x_spi_recv_displaylist(dl_recv_buf, sizeof(dl_recv_buf));

        if (len > 0) {
            /* Got a display list — execute it */
            j32x_exec_displaylist(dl_recv_buf, (uint32_t)len);
            got_spi = true;
        } else if (!got_spi) {
            /* No SPI master ever connected — run self-test cube */
            self_test_frame(angle++);
            sleep_ms(16);  /* ~60fps */
        }
        /* If got_spi but no data this poll: just loop back and check again.
         * The V3s sends a new display list each frame at ~60fps. */
    }

    return 0;
}
