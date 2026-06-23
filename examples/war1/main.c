/*
 * Jupiter SDK — Warcraft 1 port
 *
 * Bare-metal entry point: bring up Jupiter hardware, then hand off to
 * Stratagus's canonical main (stratagusMain → InitLua → InitSound →
 * InitVideo → ShowTitleScreens → PreMenuSetup → MenuLoop). No custom
 * launcher, no shortcut into a specific .smp — the title menu and
 * campaign/scenario selection live in scripts/guichan.lua, exactly as
 * upstream War1gus runs them on desktop.
 *
 * Build: make GAME=examples/war1/main.c
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"
#include <string.h>
#include <stdint.h>

#define D(s) uart_puts("[DBG] " s "\n")

/* Defined in war1_bridge.cpp — wraps stratagusMain with our pre-LoadCcl
 * compat shims (Lua print → UART, gettext _() stub, ListFilesInDirectory
 * shim, table.getn/foreach/foreachi). */
extern void war1_run(void);

void main(void)
{
    uart_puts("\n\n=== Warcraft 1 / Stratagus port ===\n");

    D("init: timer_init"); timer_init();
    D("init: mmu_init");   mmu_init();
    D("init: irq_init");   irq_init();
    D("init: pmu_init");   pmu_init();
    D("init: video_init"); video_init();
    D("init: input_init"); input_init(INPUT_N64);
    /* Audio bring-up — sequence matches examples/opn2_jupiter so the
     * codec doesn't start cold:
     *   1. audio_init (codec + DMA descriptors, halves zeroed)
     *   2. audio_set_rate (war1gus WAV native rate; PLL+÷2048 yields ~11021)
     *   3. pre-fill mix_buf with silence so audio_start's prefill copies
     *      valid samples instead of letting mix_rd race past mix_wr
     *   4. audio_start (DMA running)
     *   5. irq_global_enable (boot held cpsid; without this the codec just
     *      replays the initial half forever) */
    D("init: audio_init"); audio_init();
    audio_set_rate(11025);
    audio_set_hp_vol(63);          /* max HP PA gain (0 dB) */
    audio_set_master_volume(255);  /* max digital mix gain */
    extern int16_t mix_buf[];
    extern volatile uint32_t mix_wr;
    for (int i = 0; i < 2048; i++) { mix_buf[i] = 0; mix_wr++; }
    D("init: audio_start"); audio_start();
    D("init: irq_global_enable"); irq_global_enable();

    /* Clear all display layers once before stratagusMain takes over. */
    memset32_neon(FB0_ADDR,  0xFF000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(OVL1_ADDR, 0x00000000, LCD_W * LCD_H * 4);
    memset32_neon(SPR_ADDR,  0x00000000, LCD_W * LCD_H * 4);
    dcache_clean_fb(FB0_ADDR);
    dcache_clean_fb(OVL_ADDR);
    dcache_clean_fb(OVL1_ADDR);
    dcache_clean_fb(SPR_ADDR);

    war1_run();   /* never returns */

    for (;;) { }
}
