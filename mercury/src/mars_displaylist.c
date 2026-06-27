/*
 * Jupiter Mars — Display list executor
 *
 * Parses the command buffer received from V3s over SPI
 * and dispatches rendering commands to the rasterizer.
 */

#include <string.h>
#include "jupiter32x.h"

void j32x_exec_displaylist(const uint8_t *buf, uint32_t len)
{
    uint32_t pos = 0;

    while (pos < len) {
        uint8_t cmd = buf[pos];

        switch (cmd) {

        case CMD_NOP:
            pos++;
            break;

        case CMD_CLEAR: {
            if (pos + sizeof(j32x_cmd_clear_t) > len) return;
            const j32x_cmd_clear_t *c = (const j32x_cmd_clear_t *)&buf[pos];
            j32x_raster_clear(c->color);
            pos += sizeof(j32x_cmd_clear_t);
            break;
        }

        case CMD_TRI: {
            if (pos + sizeof(j32x_cmd_tri_t) > len) return;
            const j32x_cmd_tri_t *c = (const j32x_cmd_tri_t *)&buf[pos];
            j32x_raster_tri(c->v[0], c->v[1], c->v[2], c->color);
            pos += sizeof(j32x_cmd_tri_t);
            break;
        }

        case CMD_QUAD: {
            if (pos + sizeof(j32x_cmd_quad_t) > len) return;
            const j32x_cmd_quad_t *c = (const j32x_cmd_quad_t *)&buf[pos];
            /* Two triangles: 0-1-2 and 0-2-3 */
            j32x_raster_tri(c->v[0], c->v[1], c->v[2], c->color);
            j32x_raster_tri(c->v[0], c->v[2], c->v[3], c->color);
            pos += sizeof(j32x_cmd_quad_t);
            break;
        }

        case CMD_LINE: {
            if (pos + sizeof(j32x_cmd_line_t) > len) return;
            const j32x_cmd_line_t *c = (const j32x_cmd_line_t *)&buf[pos];
            j32x_raster_line(c->v[0], c->v[1], c->color);
            pos += sizeof(j32x_cmd_line_t);
            break;
        }

        case CMD_TEXTURE: {
            if (pos + 4 > len) return;
            uint8_t slot = buf[pos + 1];
            uint16_t size = buf[pos + 2] | (buf[pos + 3] << 8);
            pos += 4;
            if (slot < J32X_TEX_SLOTS && pos + size <= len) {
                memcpy(g_state.tex[slot].data, &buf[pos], size);
                g_state.tex[slot].valid = true;
            }
            pos += size;
            break;
        }

        case CMD_SET_RES: {
            if (pos + sizeof(j32x_cmd_set_res_t) > len) return;
            const j32x_cmd_set_res_t *c = (const j32x_cmd_set_res_t *)&buf[pos];
            if (c->w <= J32X_WIDTH && c->h <= J32X_HEIGHT && c->w > 0 && c->h > 0) {
                g_state.width = c->w;
                g_state.height = c->h;
            }
            pos += sizeof(j32x_cmd_set_res_t);
            break;
        }

        case CMD_SCENE_END:
            j32x_swap();
            return;

        default:
            /* Unknown command — skip byte and hope for the best */
            pos++;
            break;
        }
    }
}
