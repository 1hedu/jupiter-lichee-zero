/* Runs recovered startup/MMU/GIC/IRQ code on a Cortex-A53 architecture.
 * Hardware-specific register bases for console/GIC are selected by
 * PINE64_CPU_TEST. No hardware SD, display, audio or TF-A claim is made.
 */
#include "jupiter.h"
#include "peripherals.h"
#include "usb.h"
#include <stdint.h>

_Static_assert(GENMASK(31, 0) == UINT32_MAX, "full-width register mask");
_Static_assert(GENMASK(31, 31) == 0x80000000u, "top-bit register mask");

static volatile uint32_t bss_probe[16384];
static volatile uint32_t low_probe[16384] __attribute__((section(".bss_low")));
static volatile uint32_t usb_probe[32] __attribute__((section(".bss_usb"), aligned(64)));
static volatile uint32_t initialized_data = 0xdead1234u;
static uint32_t neon_words[4] __attribute__((aligned(16)));
static uint8_t memory_source[160] __attribute__((aligned(16)));
static uint8_t memory_target[160] __attribute__((aligned(16)));
volatile uint32_t cpu_irq_seen;
extern void cpu_irq_callback(void);
extern uint32_t cpu_irq_probe(void);
extern uint32_t __bss_start, __bss_end, __bss_low_end;

void uart_puts(const char *s)
{
    volatile uint32_t *uart = (volatile uint32_t *)0x09000000u;
    while (*s) {
        while (uart[0x18 / 4] & (1u << 5))
            ;
        uart[0] = (uint8_t)*s++;
    }
}

static void finish(int status) __attribute__((noreturn));
static void finish(int status)
{
    uint32_t block[2] = {0x20026u, (uint32_t)status};
    register uint32_t op __asm__("r0") = 0x20u;
    register uint32_t arg __asm__("r1") = (uint32_t)(uintptr_t)block;
    __asm__ volatile("svc 0xab" : "+r"(op) : "r"(arg) : "memory");
    for (;;)
        __asm__ volatile("wfi");
}

static void require(int condition, const char *reason)
{
    if (!condition) {
        uart_puts("CPU smoke FAIL: ");
        uart_puts(reason);
        uart_puts("\n");
        finish(1);
    }
}

int main(void)
{
    uint32_t cpsr, midr;
    __asm__ volatile("mrs %0, cpsr" : "=r"(cpsr));
    __asm__ volatile("mrc p15, 0, %0, c0, c0, 0" : "=r"(midr));
    require((midr & 0xff00fff0u) == 0x4100d030u, "not Cortex-A53");
    /* MRS masks the execution-state bits; the Thumb function pointer
     * proves interworking, while CPSR.M checks the privileged mode. */
    require((cpsr & 0x1fu) == 0x13u && ((uintptr_t)main & 1u), "main is not Thumb SVC");
    require((pine64_boot_cpsr & 0x3fu) == 0x13u, "entry was not ARM SVC");
    require(pine64_boot_fdt == 0x47000000u, "lost r2 DTB");
    require((pine64_boot_sctlr & 0x72183007u) == (1u << 12), "SCTLR not normalized");
    require((uintptr_t)&__bss_start >= 0x50000000u &&
            (uintptr_t)&__bss_end <= 0x58000000u, "menu BSS outside high arena");
    require(initialized_data == 0xdead1234u, "initialized data was damaged");
    for (unsigned i = 0; i < 16384u; ++i)
        require(bss_probe[i] == 0 && low_probe[i] == 0, "poisoned BSS was not cleared");
    for (unsigned i = 0; i < 32u; ++i)
        require(usb_probe[i] == 0, "poisoned USB DMA BSS was not cleared");
    for (uintptr_t p = 0x43800000u; p < 0x44000000u; p += 4u)
        require(*(volatile uint32_t *)p == 0, "VRAM was not fully cleared");
    require(*(volatile uint32_t *)0x40fffffcu == 0xa5a5a5a5u,
            "startup cleared outside the actual low BSS");
    __asm__ volatile("vmov.i32 q15, #0x5a\n\tvst1.32 {d30-d31}, [%0]" ::
                     "r"(neon_words) : "d30", "d31", "memory");
    for (unsigned i = 0; i < 4; ++i)
        require(neon_words[i] == 0x5au, "upper NEON registers failed");
    uart_puts("CPU smoke: entry, high/low BSS, full VRAM, Thumb and NEON PASS\n");

    /* Exercise the newest shared NEON helpers on the actual Cortex-A53
     * instruction path: zero size, short words, bulk tails and odd copies. */
    for (unsigned length = 0; length <= 132u; ++length) {
        for (unsigned i = 0; i < sizeof(memory_target); ++i) {
            memory_source[i] = (uint8_t)(i * 19u + 7u);
            memory_target[i] = 0xa5;
        }
        memset32_neon((uint32_t)(uintptr_t)(memory_target + 16), 0x39393939u, length);
        for (unsigned i = 0; i < sizeof(memory_target); ++i) {
            uint8_t expected = i >= 16u && i < 16u + (length & ~3u) ? 0x39 : 0xa5;
            require(memory_target[i] == expected, "NEON word fill tail or boundary");
            memory_target[i] = 0xa5;
        }
        memcpy_neon(memory_target + 16, memory_source + 16, length);
        for (unsigned i = 0; i < sizeof(memory_target); ++i) {
            uint8_t expected = i >= 16u && i < 16u + length ? memory_source[i] : 0xa5;
            require(memory_target[i] == expected, "NEON byte copy tail or boundary");
        }
    }
    uart_puts("CPU smoke: latest NEON zero-size, word tails, odd copies and full-width masks PASS\n");

    uint32_t frequency;
    __asm__ volatile("mrc p15, 0, %0, c14, c0, 0" : "=r"(frequency));
    require(frequency >= 1000000u && frequency <= 100000000u && frequency % 1000u == 0,
            "USB generic counter frequency invalid");
    uint64_t counter_first = pine64_usb_counter();
    unsigned counter_bound = 1000000u;
    while (pine64_usb_counter() == counter_first && --counter_bound)
        ;
    require(counter_bound != 0, "EL1 physical generic counter did not advance");
    uart_puts("CPU smoke: EL1 physical generic counter for USB timeouts PASS\n");

    mmu_init();
    uint32_t sctlr, ttbr, ttbcr, dacr;
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));
    __asm__ volatile("mrc p15, 0, %0, c2, c0, 0" : "=r"(ttbr));
    __asm__ volatile("mrc p15, 0, %0, c2, c0, 2" : "=r"(ttbcr));
    __asm__ volatile("mrc p15, 0, %0, c3, c0, 0" : "=r"(dacr));
    require((sctlr & 0x1005u) == 0x1005u, "MMU or cache stayed disabled");
    require((ttbr & 0x7bu) == 0x4au && ttbcr == 0 && dacr == 1u,
            "incorrect short-descriptor table walk attributes");
    const uint32_t *table = (const uint32_t *)(uintptr_t)(ttbr & 0xffffc000u);
    require(table[0x01c] == 0x01c10c16u, "peripherals not shareable Device/XN");
    require(table[0x500] == 0x50011c0eu && table[0xbff] == 0xbff11c0eu,
            "high BSS/upper 2 GiB mapping incorrect");
    require(table[0x580] == 0x58011c12u, "USB DMA is not Normal non-cacheable/XN");
    usb_probe[0] = 0x8585a5a5u;
    require(usb_probe[0] == 0x8585a5a5u, "USB DMA memory write/read failed");
    bss_probe[0] = 0x1234abcdu;
    low_probe[0] = 0xfedc4321u;
    *(volatile uint32_t *)0xbffff000u = 0x6666aaaau;
    *(volatile uint32_t *)0x43fffffcu = 0x77775555u;
    dcache_clean_range((uint32_t)(uintptr_t)bss_probe, sizeof(bss_probe));
    dcache_invalidate_range((uint32_t)(uintptr_t)bss_probe, sizeof(bss_probe));
    mmu_init();
    require(bss_probe[0] == 0x1234abcdu && low_probe[0] == 0xfedc4321u &&
            *(volatile uint32_t *)0xbffff000u == 0x6666aaaau &&
            *(volatile uint32_t *)0x43fffffcu == 0x77775555u,
            "cache maintenance or repeated MMU initialization damaged data");
    uart_puts("CPU smoke: 2 GiB mapping, cache maintenance and idempotent MMU PASS\n");

    irq_init();
    irq_register(1u, cpu_irq_callback);
    irq_global_enable();
    require(cpu_irq_probe() == 0, "IRQ context/dispatch timeout");
    require(cpu_irq_seen != 0, "IRQ callback did not run");
    uart_puts("CPU smoke: real GIC IRQ delivery, full VFP context and return PASS\n");
    uart_puts("CPU smoke PASS: A64 EL2 -> A32 EL1 and recovered menu core\n");
    finish(0);
}
