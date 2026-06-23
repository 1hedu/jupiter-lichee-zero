/*
 * ve_full_test.c — Comprehensive V3s CedarVE test from Linux userspace.
 * Enables VE clocks, tests all engines, tries DMA, dumps everything.
 *
 * Compile on Lichee Pi: gcc -std=c99 -O2 -o ve_full_test ve_full_test.c
 * Run: ./ve_full_test
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define PAGE_SIZE 4096

static int mem_fd;

/* Map a physical page and return pointer to specific offset */
static volatile uint32_t *map_page(uint32_t phys_base)
{
    void *p = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED,
                   mem_fd, phys_base & ~(PAGE_SIZE-1));
    if (p == MAP_FAILED) { perror("mmap"); return NULL; }
    return (volatile uint32_t *)p;
}

/* Read/write physical registers through mmap */
static volatile uint32_t *ccu;    /* 0x01C20000 */
static volatile uint32_t *sram;   /* 0x01C00000 */
static volatile uint32_t *ve;     /* 0x01C0E000 */
static volatile uint32_t *dram;   /* 0x01C62000 */
static volatile uint32_t *dram2;  /* 0x01C63000 */
static volatile uint8_t  *ve_b;   /* byte access to VE */

/* Allocate a DMA-capable buffer using /dev/mem mmap */
/* On V3s, any DRAM address works since there's no IOMMU */
#define TEST_BUF_PHYS 0x43F00000  /* top of 64MB DRAM, unlikely to conflict */
static volatile uint32_t *test_buf;

#define R(base, off) ((base)[(off)/4])
#define W(base, off, val) ((base)[(off)/4] = (val))

static void dump_range(const char *label, volatile uint32_t *base,
                       unsigned start, unsigned end)
{
    printf("\n=== %s ===\n", label);
    unsigned o;
    for (o = start; o <= end; o += 4) {
        uint32_t v = R(base, o);
        if (v != 0)
            printf("  +0x%03X = 0x%08X\n", o, v);
    }
    printf("  (zero regs hidden)\n");
}

static void dump_range_all(const char *label, volatile uint32_t *base,
                           unsigned start, unsigned end)
{
    printf("\n=== %s ===\n", label);
    unsigned o;
    for (o = start; o <= end; o += 4)
        printf("  +0x%03X = 0x%08X\n", o, R(base, o));
}

/* H264 VLD address encoding (from cedrus_regs.h) */
#define VLD_ADDR_VAL(x) (((x)&0x0FFFFFF0)|((x)>>28))
#define VLD_FIRST (1u<<30)
#define VLD_LAST  (1u<<29)
#define VLD_VALID (1u<<28)

int main(void)
{
    printf("=== V3s CedarVE Full Test ===\n\n");

    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) { perror("open /dev/mem"); return 1; }

    ccu   = map_page(0x01C20000);
    sram  = map_page(0x01C00000);
    ve    = map_page(0x01C0E000);
    dram  = map_page(0x01C62000);
    dram2 = map_page(0x01C63000);
    ve_b  = (volatile uint8_t *)ve;

    if (!ccu || !sram || !ve || !dram || !dram2) {
        printf("mmap failed\n"); return 1;
    }

    /* Also map a test buffer for DMA */
    test_buf = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED,
                    mem_fd, TEST_BUF_PHYS);
    if (test_buf == MAP_FAILED) {
        printf("WARNING: can't map test buffer at 0x%08X\n", TEST_BUF_PHYS);
        test_buf = NULL;
    }

    /* ============================================================ */
    /* PHASE 1: Dump current state (before we touch anything)       */
    /* ============================================================ */
    printf("--- PHASE 1: Current state ---\n");
    dump_range("CCU PLLs+gates (0x000-0x070)", ccu, 0x000, 0x070);
    dump_range("CCU module clocks (0x0F0-0x160)", ccu, 0x0F0, 0x160);
    dump_range("CCU resets (0x2C0-0x2D8)", ccu, 0x2C0, 0x2D8);
    dump_range("SRAM controller (0x000-0x010)", sram, 0x000, 0x010);
    dump_range("DRAM controller (0x000-0x0FF)", dram, 0x000, 0x0FF);
    dump_range("DRAM controller 2 (0x000-0x0FF)", dram2, 0x000, 0x0FF);

    /* ============================================================ */
    /* PHASE 2: Enable VE clocks                                     */
    /* ============================================================ */
    printf("\n--- PHASE 2: Enabling VE ---\n");

    /* SRAM to VE */
    uint32_t sram_val = R(sram, 0x004);
    W(sram, 0x004, sram_val & ~(1u<<24));
    printf("SRAM: 0x%08X -> 0x%08X\n", sram_val, R(sram, 0x004));

    /* PLL_VE: 297 MHz (N=99, M=7) */
    W(ccu, 0x018, (99u<<8)|(7u<<0));    /* disable + set N/M */
    usleep(100);
    W(ccu, 0x018, R(ccu,0x018)|(1u<<31)); /* enable */
    usleep(10000);
    printf("PLL_VE: 0x%08X (locked=%d)\n", R(ccu,0x018), !!(R(ccu,0x018)&(1u<<28)));

    /* Try BOTH init orders and report which works */

    /* === Order A: cedrus kernel (reset before clocks) === */
    printf("\n--- Order A: reset -> clocks ---\n");

    /* Reset cycle */
    W(ccu, 0x2C0, R(ccu,0x2C0) & ~(1u<<0)); usleep(100);
    W(ccu, 0x2C0, R(ccu,0x2C0) | (1u<<0));  usleep(100);
    /* Then clocks */
    W(ccu, 0x060, R(ccu,0x060) | (1u<<0));           /* bus gate */
    W(ccu, 0x13C, (1u<<31)|(0u<<24)|(0u<<0));        /* VE_CLK */
    W(ccu, 0x100, R(ccu,0x100) | (1u<<0));           /* DRAM gate */
    usleep(1000);

    printf("BUS_GATE=0x%08X RST=0x%08X VE_CLK=0x%08X DRAM=0x%08X\n",
           R(ccu,0x060), R(ccu,0x2C0), R(ccu,0x13C), R(ccu,0x100));

    /* Park idle then select H264 */
    W(ve, 0x000, 0x00130007); usleep(100);
    printf("VE_VERSION=0x%08X VE_CTRL=0x%08X\n", R(ve,0x0F0), R(ve,0x000));

    /* Scan VE for any non-zero */
    dump_range("VE global after Order A", ve, 0x000, 0x0FC);

    /* Select H264 */
    W(ve, 0x000, 0x00130001);
    dump_range("VE H264 regs after Order A", ve, 0x200, 0x2F0);

    /* VLD DMA test */
    if (test_buf) {
        printf("\n--- VLD DMA Test (Order A) ---\n");

        /* Fill test buffer with known pattern */
        int i;
        for (i = 0; i < 256; i++)
            ((volatile uint8_t *)test_buf)[i] = 0x65 + i; /* starts with 0x65 like IDR NAL */
        /* No cache flush needed — mmap with O_SYNC */

        W(ve, 0x228, R(ve, 0x228)); /* clear status */

        W(ve, 0x238, 256 * 8);          /* VLD_LEN = 2048 bits */
        W(ve, 0x234, 0);                /* VLD_OFFSET = 0 */
        W(ve, 0x23C, TEST_BUF_PHYS + 4095); /* VLD_END */
        W(ve, 0x230, VLD_ADDR_VAL(TEST_BUF_PHYS) | VLD_FIRST | VLD_LAST | VLD_VALID);

        printf("VLD_ADDR=0x%08X VLD_LEN=0x%08X VLD_END=0x%08X\n",
               R(ve,0x230), R(ve,0x238), R(ve,0x23C));

        /* INIT_SWDEC */
        W(ve, 0x224, 7); /* trigger INIT_SWDEC */
        usleep(1000);
        printf("After INIT: STATUS=0x%08X BASIC_BITS=0x%08X\n",
               R(ve,0x228), R(ve,0x2DC));

        /* Try FLUSH 8 bits */
        W(ve, 0x224, 3 | (8<<8)); /* FLUSH_BITS, 8 bits */
        usleep(10000); /* 10ms — generous */
        uint32_t status_a = R(ve, 0x228);
        printf("After FLUSH: STATUS=0x%08X BITS=0x%08X VLD_BUSY=%d\n",
               status_a, R(ve,0x2DC), !!(status_a & 0x100));
    }

    /* === Order B: jemk/cedrus (clocks before reset) === */
    printf("\n--- Order B: clocks -> reset ---\n");

    /* First disable everything */
    W(ve, 0x000, 0x00130007);
    W(ccu, 0x13C, 0);              /* VE_CLK off */
    W(ccu, 0x060, R(ccu,0x060) & ~(1u<<0)); /* bus gate off */
    W(ccu, 0x100, R(ccu,0x100) & ~(1u<<0)); /* DRAM gate off */
    W(ccu, 0x2C0, R(ccu,0x2C0) & ~(1u<<0)); /* assert reset */
    usleep(10000);

    /* Now clocks first, then reset */
    W(ccu, 0x060, R(ccu,0x060) | (1u<<0));           /* bus gate */
    W(ccu, 0x13C, (1u<<31)|(0u<<24)|(0u<<0));        /* VE_CLK */
    W(ccu, 0x100, R(ccu,0x100) | (1u<<0));           /* DRAM gate */
    usleep(1000);
    /* Reset cycle AFTER clocks */
    W(ccu, 0x2C0, R(ccu,0x2C0) & ~(1u<<0)); usleep(100);
    W(ccu, 0x2C0, R(ccu,0x2C0) | (1u<<0));  usleep(100);

    printf("BUS_GATE=0x%08X RST=0x%08X VE_CLK=0x%08X DRAM=0x%08X\n",
           R(ccu,0x060), R(ccu,0x2C0), R(ccu,0x13C), R(ccu,0x100));

    W(ve, 0x000, 0x00130007); usleep(100);
    printf("VE_VERSION=0x%08X\n", R(ve,0x0F0));
    W(ve, 0x000, 0x00130001);

    /* VLD DMA test */
    if (test_buf) {
        printf("\n--- VLD DMA Test (Order B) ---\n");

        W(ve, 0x228, R(ve, 0x228));
        W(ve, 0x238, 256 * 8);
        W(ve, 0x234, 0);
        W(ve, 0x23C, TEST_BUF_PHYS + 4095);
        W(ve, 0x230, VLD_ADDR_VAL(TEST_BUF_PHYS) | VLD_FIRST | VLD_LAST | VLD_VALID);

        W(ve, 0x224, 7);
        usleep(1000);
        printf("After INIT: STATUS=0x%08X BASIC_BITS=0x%08X\n",
               R(ve,0x228), R(ve,0x2DC));

        W(ve, 0x224, 3 | (8<<8));
        usleep(10000);
        uint32_t status_b = R(ve, 0x228);
        printf("After FLUSH: STATUS=0x%08X BITS=0x%08X VLD_BUSY=%d\n",
               status_b, R(ve,0x2DC), !!(status_b & 0x100));
    }

    /* ============================================================ */
    /* PHASE 3: Try writing VE+0x004 (DMA control?)                  */
    /* ============================================================ */
    printf("\n--- PHASE 3: VE+0x004 experiments ---\n");
    printf("VE+0x004 current = 0x%08X\n", R(ve, 0x004));

    /* Try setting bit 1 (DMA_CACHE_EN per some docs) */
    W(ve, 0x004, R(ve,0x004) | (1u<<1));
    printf("VE+0x004 after |= bit1 = 0x%08X\n", R(ve, 0x004));

    if (test_buf) {
        W(ve, 0x000, 0x00130001); /* H264 */
        W(ve, 0x228, R(ve, 0x228));
        W(ve, 0x238, 256 * 8);
        W(ve, 0x234, 0);
        W(ve, 0x23C, TEST_BUF_PHYS + 4095);
        W(ve, 0x230, VLD_ADDR_VAL(TEST_BUF_PHYS) | VLD_FIRST | VLD_LAST | VLD_VALID);
        W(ve, 0x224, 7);
        usleep(1000);
        printf("INIT+DMA_EN: STATUS=0x%08X BITS=0x%08X\n", R(ve,0x228), R(ve,0x2DC));
        W(ve, 0x224, 3 | (8<<8));
        usleep(10000);
        printf("FLUSH+DMA_EN: STATUS=0x%08X BITS=0x%08X BUSY=%d\n",
               R(ve,0x228), R(ve,0x2DC), !!(R(ve,0x228)&0x100));
    }

    /* ============================================================ */
    /* PHASE 4: Scan MPEG engine with different VE+0x004 values     */
    /* ============================================================ */
    printf("\n--- PHASE 4: MPEG engine scan ---\n");
    W(ve, 0x000, 0x00130000); /* select MPEG */
    dump_range("MPEG regs (engine=0)", ve, 0x100, 0x1F0);

    /* ============================================================ */
    /* PHASE 5: AVC encoder scan                                     */
    /* ============================================================ */
    printf("\n--- PHASE 5: AVC encoder scan ---\n");
    W(ve, 0x000, 0x0013000B); /* select AVC */
    dump_range("AVC regs (engine=B)", ve, 0xB00, 0xBC0);

    /* ============================================================ */
    /* PHASE 6: Full DRAM controller dump                            */
    /* ============================================================ */
    dump_range_all("DRAM controller 0x01C62000", dram, 0x000, 0x0FC);
    dump_range_all("DRAM controller 0x01C63000", dram2, 0x000, 0x0FC);

    /* ============================================================ */
    /* PHASE 7: All CCU registers after VE enable                    */
    /* ============================================================ */
    dump_range_all("CCU full after VE enable (0x000-0x070)", ccu, 0x000, 0x070);
    dump_range_all("CCU module clocks (0x0F0-0x160)", ccu, 0x0F0, 0x160);
    dump_range_all("CCU resets (0x2C0-0x2D8)", ccu, 0x2C0, 0x2D8);

    /* ============================================================ */
    /* PHASE 8: Full VE register dump (all engines)                  */
    /* ============================================================ */
    W(ve, 0x000, 0x00130001); /* H264 */
    dump_range("VE full dump (H264 selected)", ve, 0x000, 0xBFF);

    /* Cleanup */
    W(ve, 0x000, 0x00130007); /* idle */

    printf("\n=== DONE ===\n");
    return 0;
}
