#ifndef PINE64_ASSETS_H
#define PINE64_ASSETS_H
#include <stdint.h>
/* Read from FAT data partition (0:), with exact read and length checks.
 * Returned allocation belongs to the caller; NULL means unavailable. */
uint8_t *pine64_load_asset(const char *path, uint32_t min_size,
                          uint32_t max_size, uint32_t *length);
#endif
