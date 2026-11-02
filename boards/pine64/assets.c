#include "jupiter.h"
#include "assets.h"
#include "ff.h"
#include <stdlib.h>

static FATFS data_fs;
static int mounted;

uint8_t *pine64_load_asset(const char *path, uint32_t min_size,
                          uint32_t max_size, uint32_t *length)
{
    FIL file;
    uint8_t *data = NULL;
    *length = 0;
    if (!mounted) {
        FRESULT result = f_mount(&data_fs, "0:", 1);
        if (result != FR_OK) {
            uart_puts("[asset] data partition mount failed: ");
            uart_putdec(result); uart_puts("\n");
            return NULL;
        }
        mounted = 1;
    }
    FRESULT result = f_open(&file, path, FA_READ);
    if (result != FR_OK) {
        uart_puts("[asset] cannot open "); uart_puts(path);
        uart_puts(" (FatFs "); uart_putdec(result); uart_puts(")\n");
        return NULL;
    }
    FSIZE_t size = f_size(&file);
    if (size < min_size || size > max_size) {
        uart_puts("[asset] invalid size for "); uart_puts(path);
        uart_puts(": "); uart_putdec((uint32_t)size); uart_puts("\n");
        goto done;
    }
    data = malloc((size_t)size);
    if (!data) {
        uart_puts("[asset] allocation failed\n");
        goto done;
    }
    for (uint32_t offset = 0; offset < (uint32_t)size;) {
        UINT want = (uint32_t)size - offset;
        UINT got = 0;
        if (want > 32768u) want = 32768u;
        result = f_read(&file, data + offset, want, &got);
        if (result != FR_OK || got != want) {
            uart_puts("[asset] incomplete read: "); uart_puts(path);
            uart_puts("\n");
            free(data); data = NULL;
            goto done;
        }
        offset += got;
        pine64_service();
    }
    *length = (uint32_t)size;
    uart_puts("[asset] "); uart_puts(path); uart_puts(": ");
    uart_putdec(*length); uart_puts(" bytes\n");
done:
    f_close(&file);
    return data;
}
