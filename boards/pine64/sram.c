/* A64 SRAM ownership is different from V3s. Do not probe or remap it.
 * SRAM_C at 0x18000 is DE2 line-buffer storage (SYS_CTRL+4 bit24).
 * SRAM_A2 holds secure TF-A and SRAM_C1 at 0x01d00000 is Cedar VE storage.
 * A portable allocator must not steal either firmware/display/VE memory. */
#include "jupiter.h"

void sram_init(void)
{
    uart_puts("[sram/pine64] SRAM reserved by TF-A, DE2 and VE; CPU probe disabled\n");
    uart_puts("[sram/pine64] SYS_CTRL+4=");
    uart_puthex(REG32(0x01c00004u)); uart_puts(" (read only)\n");
}
uint32_t sram_probe(void) { return 0; }
uint32_t sram_size(void) { return 0; }
