/*
 * Minimal N64 poll diagnostic: prints the input_state_t each poll so we
 * can see exactly what the emulator's virt-n64-pad delivered.
 *
 * Compile via the Jupiter Makefile:
 *   make GAME=/mnt/c/Users/darek/licheeEmu/docs/n64_diag_main.c
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"

void main(void)
{
    timer_init();
    mmu_init();
    pmu_init();
    video_init();

    uart_puts("\n=== N64 diag ===\n");
    input_init(INPUT_N64);

    uint32_t frame = 0;

    /* --- step 1: manually trigger one falling edge then sample PE_DAT
     * in a tight loop to see whether the pad's response bits are
     * visible at all from the guest side. */
    #define PE_CFG2 (*(volatile uint32_t *)(PIO_BASE + 0x98))
    #define PE_DAT  (*(volatile uint32_t *)(PIO_BASE + 0xA0))
    #define N64_BIT (1u << 20)

    /* Trigger a single falling edge to kick the pad's RX, then sample
     * PE20 continuously to record the TX bits. Prints a run-length of
     * stable levels so we can see real timing. */
    uart_puts("[diag] kicking pad, then sampling for ~500us\n");
    PE_DAT &= ~N64_BIT;
    PE_CFG2 = (PE_CFG2 & ~(0xFu << 16)) | (0x1u << 16);
    for (volatile int w = 0; w < 5; w++);
    PE_CFG2 = (PE_CFG2 & ~(0xFu << 16));

    /* Collect 1024 samples with pmu timestamp of each change. */
    #define NSAM 1024
    uint32_t edges[32];
    uint8_t  lvls[32];
    int nev = 0;
    uint32_t t_start = pmu_cycles();
    int prev_lv = (PE_DAT >> 20) & 1;
    edges[nev] = 0;
    lvls[nev] = prev_lv;
    nev++;
    for (int i = 0; i < NSAM*4 && nev < 32; i++) {
        int lv = (PE_DAT >> 20) & 1;
        if (lv != prev_lv) {
            edges[nev] = pmu_cycles() - t_start;
            lvls[nev]  = lv;
            nev++;
            prev_lv = lv;
        }
    }
    uart_puts("[diag] "); uart_putdec(nev); uart_puts(" edges in ~");
    uart_putdec((pmu_cycles() - t_start) / 1200);
    uart_puts("us\n");
    for (int i = 0; i < nev; i++) {
        uart_puts("  t+"); uart_putdec(edges[i] / 1200);
        uart_puts("us lvl="); uart_putdec(lvls[i]);
        uart_puts("\n");
    }

    while (1) {
        input_state_t st = input_poll_n64();

        /* Print once per ~60 polls (~1s) so we can read it. */
        if ((frame & 63) == 0) {
            uart_puts("poll=");
            uart_putdec(frame);
            uart_puts(" connected=");
            uart_putdec(st.connected);
            uart_puts(" buttons=");
            uart_puthex(st.buttons);
            uart_puts(" pmu=");
            uart_puthex(pmu_cycles());
            uart_puts("\n");
        }
        frame++;

        /* 60Hz pacing */
        uint32_t t0 = timer_read();
        while (ticks_to_us(timer_elapsed(t0, timer_read())) < 16667);
    }
}
