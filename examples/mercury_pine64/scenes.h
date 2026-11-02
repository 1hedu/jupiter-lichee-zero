#ifndef PINE64_MERCURY_DEMO_SCENES_H
#define PINE64_MERCURY_DEMO_SCENES_H
#include <stdint.h>
#include <stddef.h>

/* Exact commands consumed by mercury/src/mercury_displaylist.c at
 * 4fb5904e. This builds display lists only; it does not render a local
 * substitute for the Pico's TV output. */
enum { MC_CLEAR = 0x01, MC_TRI = 0x02, MC_QUAD = 0x04, MC_LINE = 0x05,
       MC_SET_RES = 0x20, MC_SCENE_END = 0xff };
#define MC_RGB332(r,g,b) (((r) & 0xe0) | (((g) >> 3) & 0x1c) | ((b) >> 6))
static void mercury_scene_vertex(uint8_t *p, int x, int y)
{
    uint16_t ux = (uint16_t)(int16_t)x, uy = (uint16_t)(int16_t)y;
    p[0] = (uint8_t)ux; p[1] = (uint8_t)(ux >> 8);
    p[2] = (uint8_t)uy; p[3] = (uint8_t)(uy >> 8);
}
static size_t mercury_scene_begin(uint8_t *out, uint8_t color)
{
    /* Keep the existing Pico scanout at its default 320x224 resolution. */
    out[0] = MC_SET_RES; out[1] = 0x40; out[2] = 1;
    out[3] = 0xe0; out[4] = 0;
    out[5] = MC_CLEAR; out[6] = color;
    return 7;
}
static size_t mercury_scene_bars(uint8_t *out, size_t capacity)
{
    if (!out || capacity < 176u) return 0;
    static const uint8_t colors[8] = {0xe0,0x1c,0x03,0xfc,0xe3,0x1f,0xff,0x00};
    size_t pos = mercury_scene_begin(out, 0);
    for (unsigned bar = 0; bar < 8; ++bar) {
        out[pos++] = MC_QUAD; out[pos++] = colors[bar];
        mercury_scene_vertex(out + pos, bar * 40, 0); pos += 4;
        mercury_scene_vertex(out + pos, bar * 40 + 39, 0); pos += 4;
        mercury_scene_vertex(out + pos, bar * 40 + 39, 224); pos += 4;
        mercury_scene_vertex(out + pos, bar * 40, 224); pos += 4;
    }
    out[pos++] = MC_TRI; out[pos++] = 0xff;
    mercury_scene_vertex(out + pos, 80, 190); pos += 4;
    mercury_scene_vertex(out + pos, 160, 35); pos += 4;
    mercury_scene_vertex(out + pos, 240, 190); pos += 4;
    out[pos++] = MC_LINE; out[pos++] = 0;
    mercury_scene_vertex(out + pos, 0, 205); pos += 4;
    mercury_scene_vertex(out + pos, 319, 205); pos += 4;
    out[pos++] = MC_SCENE_END;
    return pos;
}
/* The branch's dusk-palette cube: 8.8 parabolic sine, Y/X rotation,
 * perspective and the same painter's face order. Distinct colours make
 * a working command link visible against the autonomous primary cube. */
static int mercury_scene_sine(unsigned a)
{
    unsigned i = a & 127u;
    int v = (int)(4u * i * (128u - i) * 256u / (128u * 128u));
    return (a & 128u) ? -v : v;
}
static size_t mercury_scene_cube(uint8_t *out, size_t capacity, uint8_t angle)
{
    if (!out || capacity < 176u) return 0;
    static const int16_t vertices[8][3] = {
        {-64,-64,-64}, {64,-64,-64}, {64,64,-64}, {-64,64,-64},
        {-64,-64,64}, {64,-64,64}, {64,64,64}, {-64,64,64}
    };
    static const uint8_t tris[12][3] = {
        {0,1,2},{0,2,3},{4,6,5},{4,7,6},{0,4,5},{0,5,1},
        {2,6,7},{2,7,3},{0,3,7},{0,7,4},{1,5,6},{1,6,2}
    };
    static const uint8_t colors[6] = {
        MC_RGB332(0xf0,0x7a,0x5a), MC_RGB332(0x23,0x25,0x58),
        MC_RGB332(0x8a,0x4a,0x86), MC_RGB332(0xf2,0xc1,0x4e),
        MC_RGB332(0xc7,0x5b,0x79), MC_RGB332(0x48,0xa0,0x90)
    };
    int16_t projected[8][2];
    int32_t depth_z[8], face_z[6];
    unsigned order[6] = {0,1,2,3,4,5};
    int ca = mercury_scene_sine(angle + 64u), sa = mercury_scene_sine(angle);
    int cb = mercury_scene_sine(angle / 2u + 64u), sb = mercury_scene_sine(angle / 2u);
    for (unsigned i = 0; i < 8; ++i) {
        int x = vertices[i][0], y = vertices[i][1], z = vertices[i][2];
        int nx = (x * ca - z * sa) >> 8;
        int nz = (x * sa + z * ca) >> 8;
        int ny = (y * cb - nz * sb) >> 8;
        z = ((y * sb + nz * cb) >> 8) + 256;
        if (z < 16) z = 16;
        projected[i][0] = (int16_t)(160 + nx * 200 / z);
        projected[i][1] = (int16_t)(112 + ny * 200 / z);
        depth_z[i] = z;
    }
    for (unsigned f = 0; f < 6; ++f) {
        unsigned t = f * 2;
        face_z[f] = (depth_z[tris[t][0]] + depth_z[tris[t][1]] +
                     depth_z[tris[t][2]] + depth_z[tris[t + 1][2]]) / 4;
    }
    for (unsigned i = 0; i < 5; ++i)
        for (unsigned j = i + 1; j < 6; ++j)
            if (face_z[order[i]] < face_z[order[j]]) {
                unsigned tmp = order[i]; order[i] = order[j]; order[j] = tmp;
            }
    size_t pos = mercury_scene_begin(out, MC_RGB332(0x0b,0x10,0x26));
    for (unsigned f = 0; f < 6; ++f)
        for (unsigned t = 0; t < 2; ++t) {
            unsigned ti = order[f] * 2u + t;
            out[pos++] = MC_TRI; out[pos++] = colors[order[f]];
            for (unsigned v = 0; v < 3; ++v) {
                unsigned vi = tris[ti][v];
                mercury_scene_vertex(out + pos, projected[vi][0], projected[vi][1]);
                pos += 4;
            }
        }
    out[pos++] = MC_SCENE_END;
    return pos;
}
#endif
