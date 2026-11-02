#ifndef JUPITER_PINE64_CEDAR_IPCM_H
#define JUPITER_PINE64_CEDAR_IPCM_H

#include <stddef.h>
#include <stdint.h>

/* A single Annex B Baseline IDR picture, using lossless I_PCM macroblocks.
 * The NV12 input must include padding to a complete 16x16 macroblock grid.
 * Returns the stream length, or a negative value for invalid input / capacity.
 * The slice header is 36 RBSP bits from the NAL header, matching cedar.c. */
int pine64_h264_ipcm(const uint8_t *y, const uint8_t *uv, uint32_t stride,
                     uint32_t width, uint32_t height, int qp,
                     uint8_t *output, size_t capacity);

#endif
