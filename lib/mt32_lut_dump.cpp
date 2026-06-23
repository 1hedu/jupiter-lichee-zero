/* Dumps munt's LogSin LUT and exp LUT to UART for bit-level comparison
 * against Linux reference. Values depend on sin()/log() precision which
 * differs between libm implementations (newlib vs glibc). */
#include "Tables.h"

extern "C" {
    void uart_puts(const char*);
    void uart_puthex(unsigned);
    void uart_putdec(unsigned);
}

extern "C" void mt32_dump_lut(void) {
    const MT32Emu::Tables& t = MT32Emu::Tables::getInstance();

    uart_puts("\n=== LUT DUMP ===\n");
    uart_puts("logsin9[0..63]:\n");
    for (int i = 0; i < 64; i++) {
        if ((i & 7) == 0) { if (i) uart_puts("\n"); uart_puts("  "); }
        uart_puthex(t.logsin9[i]);
        uart_puts(" ");
    }
    uart_puts("\nexp9[0..63]:\n");
    for (int i = 0; i < 64; i++) {
        if ((i & 7) == 0) { if (i) uart_puts("\n"); uart_puts("  "); }
        uart_puthex(t.exp9[i]);
        uart_puts(" ");
    }
    uart_puts("\n=== END LUT ===\n");
}
