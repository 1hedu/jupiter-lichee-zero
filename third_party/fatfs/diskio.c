/*-----------------------------------------------------------------------*/
/* Jupiter SDK glue for FatFs.                                            */
/*-----------------------------------------------------------------------*/
/* Drive 0 = the V3s SDMMC0 controller via lib/sdmmc.c.
 * FF_MULTI_PARTITION = 1; the logical-drive → partition table lives in
 * VolToPart[] below. Logical drive 0 is the 2nd primary partition (the
 * 3.57 GB FAT16 created in Disk Management); the first primary is the
 * 128 MB boot partition we never touch from here. */

#include "ff.h"
#include "diskio.h"
#include "sdmmc.h"
#include "jupiter.h"

#include <string.h>
#include <stdint.h>

PARTITION VolToPart[FF_VOLUMES] = {
    { 0, 2 }   /* "0:" -> physical drive 0, partition #2 (1-indexed) */
};

static DSTATUS g_status = STA_NOINIT;

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;
    return g_status;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;
    if (!sdmmc_card()->initialised) {
        if (sdmmc_init() != 0) {
            uart_puts("[diskio] sdmmc_init failed\n");
            return STA_NOINIT;
        }
    }
    g_status = 0;
    return g_status;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0 || (g_status & STA_NOINIT)) return RES_NOTRDY;
    if (((uintptr_t)buff & 3u) == 0) {
        if (sdmmc_read_blocks((uint32_t)sector, (uint32_t)count, buff) != 0)
            return RES_ERROR;
    } else {
        /* sdmmc FIFO PIO needs 4-byte alignment; bounce when the caller
         * hands us an odd buffer. FatFs's window buffer is aligned, so
         * this only triggers for f_read into user buffers. */
        static uint8_t bounce[512] __attribute__((aligned(4)));
        for (UINT i = 0; i < count; i++) {
            if (sdmmc_read_blocks((uint32_t)(sector + i), 1, bounce) != 0)
                return RES_ERROR;
            memcpy(buff + i * 512u, bounce, 512u);
        }
    }
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0 || (g_status & STA_NOINIT)) return RES_NOTRDY;
    if (((uintptr_t)buff & 3u) == 0) {
        if (sdmmc_write_blocks((uint32_t)sector, (uint32_t)count, buff) != 0)
            return RES_ERROR;
    } else {
        static uint8_t bounce[512] __attribute__((aligned(4)));
        for (UINT i = 0; i < count; i++) {
            memcpy(bounce, buff + i * 512u, 512u);
            if (sdmmc_write_blocks((uint32_t)(sector + i), 1, bounce) != 0)
                return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != 0) return RES_PARERR;
    switch (cmd) {
        case CTRL_SYNC:
            return RES_OK;
        case GET_SECTOR_COUNT: {
            const sdmmc_card_t *c = sdmmc_card();
            if (!c->initialised) return RES_NOTRDY;
            *(LBA_t *)buff = (LBA_t)c->num_blocks;
            return RES_OK;
        }
        case GET_SECTOR_SIZE:
            *(WORD *)buff = 512;
            return RES_OK;
        case GET_BLOCK_SIZE:
            /* Erase block size in sectors. We don't introspect the
             * card; 1 is safe (only consulted by f_mkfs). */
            *(DWORD *)buff = 1;
            return RES_OK;
        default:
            return RES_PARERR;
    }
}
