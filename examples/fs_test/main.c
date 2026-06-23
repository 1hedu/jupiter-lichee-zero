/*
 * Jupiter SDK — FatFs sanity check.
 *
 * Mounts the 2nd primary partition of the SD card (the 3.57 GB FAT16
 * the user created in Disk Management; "0:" via FF_MULTI_PARTITION's
 * VolToPart[0] = {0, 2}), lists the root directory, writes
 * "0:/hello.txt", reads it back, byte-compares.
 *
 * Build: make GAME=examples/fs_test/main.c
 *
 * Expected on first run (Windows-side has only whatever files you
 * dropped on E:): UART shows the listing, then "wrote N bytes",
 * then "ROUND-TRIP OK".
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"
#include "sdmmc.h"
#include "ff.h"
#include <string.h>

#define D(s) uart_puts("[fs] " s "\n")

static FATFS g_fs;
static FIL   g_fp;

static const char k_msg[] =
    "Hello from Jupiter / FatFs round-trip. "
    "If you can read this on Windows after the test ran, "
    "the SD-side write committed all the way through.\n";

static const char *fr_str(FRESULT r) {
    switch (r) {
        case FR_OK: return "OK";
        case FR_DISK_ERR: return "DISK_ERR";
        case FR_INT_ERR: return "INT_ERR";
        case FR_NOT_READY: return "NOT_READY";
        case FR_NO_FILE: return "NO_FILE";
        case FR_NO_PATH: return "NO_PATH";
        case FR_INVALID_NAME: return "INVALID_NAME";
        case FR_DENIED: return "DENIED";
        case FR_EXIST: return "EXIST";
        case FR_INVALID_OBJECT: return "INVALID_OBJECT";
        case FR_WRITE_PROTECTED: return "WRITE_PROTECTED";
        case FR_INVALID_DRIVE: return "INVALID_DRIVE";
        case FR_NOT_ENABLED: return "NOT_ENABLED";
        case FR_NO_FILESYSTEM: return "NO_FILESYSTEM";
        case FR_TIMEOUT: return "TIMEOUT";
        case FR_LOCKED: return "LOCKED";
        case FR_NOT_ENOUGH_CORE: return "NOT_ENOUGH_CORE";
        case FR_TOO_MANY_OPEN_FILES: return "TOO_MANY_OPEN_FILES";
        case FR_INVALID_PARAMETER: return "INVALID_PARAMETER";
        default: return "?";
    }
}

static void show_fr(const char *what, FRESULT r) {
    uart_puts("[fs] "); uart_puts(what);
    uart_puts(" rc="); uart_puts(fr_str(r));
    uart_puts("\n");
}

static int list_root(void)
{
    DIR dir;
    FILINFO fi;
    FRESULT r = f_opendir(&dir, "0:/");
    if (r != FR_OK) { show_fr("opendir", r); return -1; }
    D("root listing:");
    int n = 0;
    while (1) {
        r = f_readdir(&dir, &fi);
        if (r != FR_OK) { show_fr("readdir", r); break; }
        if (fi.fname[0] == 0) break;
        uart_puts("  ");
        uart_puts((fi.fattrib & AM_DIR) ? "<DIR> " : "      ");
        uart_puts(fi.fname);
        uart_puts(" (");
        uart_putdec((uint32_t)fi.fsize);
        uart_puts(" B)\n");
        n++;
    }
    f_closedir(&dir);
    uart_puts("[fs] root entries: "); uart_putdec((uint32_t)n); uart_puts("\n");
    return 0;
}

void main(void)
{
    timer_init();
    mmu_init();
    irq_init();
    pmu_init();

    uart_puts("\n\n=== Jupiter SDK FatFs sanity test ===\n");

    D("step 1: sdmmc_init");
    if (sdmmc_init() != 0) {
        D("sdmmc_init failed");
        for (;;) { }
    }

    D("step 2: f_mount 0:");
    FRESULT r = f_mount(&g_fs, "0:", 1);
    show_fr("f_mount", r);
    if (r != FR_OK) {
        D("mount failed -- check that the 2nd primary partition exists,");
        D("is formatted FAT16/32, and shows up on Windows as E:");
        for (;;) { }
    }

    DWORD free_clusters = 0;
    FATFS *fs_p = NULL;
    r = f_getfree("0:", &free_clusters, &fs_p);
    if (r == FR_OK) {
        uint32_t free_kb = (uint32_t)free_clusters * fs_p->csize / 2u;
        uart_puts("[fs] free space ~"); uart_putdec(free_kb);
        uart_puts(" KiB (cluster size = ");
        uart_putdec((uint32_t)fs_p->csize * 512u);
        uart_puts(" B, FAT type=");
        uart_putdec((uint32_t)fs_p->fs_type);
        uart_puts(")\n");
    } else {
        show_fr("f_getfree", r);
    }

    D("step 3: list root before write");
    list_root();

    D("step 4: write 0:/hello.txt");
    r = f_open(&g_fp, "0:/hello.txt", FA_CREATE_ALWAYS | FA_WRITE);
    show_fr("f_open write", r);
    if (r != FR_OK) for (;;) { }

    UINT written = 0;
    r = f_write(&g_fp, k_msg, (UINT)strlen(k_msg), &written);
    show_fr("f_write", r);
    uart_puts("[fs] wrote "); uart_putdec(written); uart_puts(" bytes\n");
    f_close(&g_fp);

    D("step 5: read it back");
    r = f_open(&g_fp, "0:/hello.txt", FA_READ);
    show_fr("f_open read", r);
    if (r != FR_OK) for (;;) { }

    char buf[256];
    UINT got = 0;
    r = f_read(&g_fp, buf, sizeof(buf) - 1, &got);
    show_fr("f_read", r);
    f_close(&g_fp);
    buf[got] = 0;
    uart_puts("[fs] read back "); uart_putdec(got); uart_puts(" bytes:\n");
    uart_puts("---8<---\n");
    uart_puts(buf);
    uart_puts("---8<---\n");

    if (got == strlen(k_msg) && memcmp(buf, k_msg, got) == 0) {
        D("ROUND-TRIP OK");
    } else {
        D("ROUND-TRIP MISMATCH");
    }

    D("step 6: list root after write");
    list_root();

    D("done. Power off, eject the SD card, plug into Windows: hello.txt should be on E:");
    for (;;) { }
}
