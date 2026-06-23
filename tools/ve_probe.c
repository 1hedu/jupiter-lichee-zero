/* ve_probe.c — compile on board: gcc -std=c99 -o ve_probe ve_probe.c */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

int main(void) {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open /dev/mem"); return 1; }

    /* Map CCU: 0x01C20000 — same as regdump */
    volatile uint32_t *ccu = mmap(NULL, 0x1000, PROT_READ|PROT_WRITE,
        MAP_SHARED, fd, 0x01C20000);
    if (ccu == MAP_FAILED) { perror("mmap ccu"); return 1; }

    printf("=== CCU PLLs ===\n");
    printf("PLL_CPU    +0x000 = 0x%08X\n", ccu[0x000/4]);
    printf("PLL_AUDIO  +0x008 = 0x%08X\n", ccu[0x008/4]);
    printf("PLL_VIDEO  +0x010 = 0x%08X\n", ccu[0x010/4]);
    printf("PLL_DDR0   +0x020 = 0x%08X\n", ccu[0x020/4]);
    printf("PLL_PERIPH +0x028 = 0x%08X\n", ccu[0x028/4]);
    printf("CCU+0x058  +0x058 = 0x%08X\n", ccu[0x058/4]);

    printf("\n=== CCU Bus Gates + Clocks ===\n");
    printf("BUS_GATE0  +0x060 = 0x%08X\n", ccu[0x060/4]);
    printf("BUS_GATE1  +0x064 = 0x%08X\n", ccu[0x064/4]);
    printf("BUS_GATE2  +0x068 = 0x%08X\n", ccu[0x068/4]);
    printf("DRAM_GATE  +0x100 = 0x%08X\n", ccu[0x100/4]);
    printf("VE_CLK     +0x13C = 0x%08X\n", ccu[0x13C/4]);
    printf("BUS_RST0   +0x2C0 = 0x%08X\n", ccu[0x2C0/4]);
    printf("BUS_RST1   +0x2C4 = 0x%08X\n", ccu[0x2C4/4]);

    printf("\n=== CCU raw 0x000-0x070 ===\n");
    unsigned o;
    for (o = 0; o <= 0x70; o += 4)
        printf("+0x%03X = 0x%08X\n", o, ccu[o/4]);

    printf("\n=== CCU raw 0x0F0-0x160 ===\n");
    for (o = 0xF0; o <= 0x160; o += 4)
        printf("+0x%03X = 0x%08X\n", o, ccu[o/4]);

    /* SRAM controller */
    volatile uint32_t *sram = mmap(NULL, 0x1000, PROT_READ|PROT_WRITE,
        MAP_SHARED, fd, 0x01C00000);
    if (sram == MAP_FAILED) {
        perror("mmap sram");
    } else {
        printf("\n=== SRAM Controller ===\n");
        printf("SRAM+0x00 = 0x%08X\n", sram[0x00/4]);
        printf("SRAM+0x04 = 0x%08X\n", sram[0x04/4]);
        munmap((void*)sram, 0x1000);
    }

    munmap((void*)ccu, 0x1000);
    close(fd);
    printf("\nDone.\n");
    return 0;
}
