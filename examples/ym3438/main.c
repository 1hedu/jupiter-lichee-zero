/*
 * Jupiter SDK — Real YM3438 Hardware VGM Player
 *
 * Plays VGM files through a physical YM3438 (OPN2C) chip connected
 * via GPIO + Si5351 clock generator. The V3s parses VGM commands and
 * sends register writes to the real chip at the correct timing.
 * Audio output is the chip's analog DAC — the real deal.
 *
 * Hardware:
 *   V3s PB0-PB7 → 74HCT245 → YM3438 D0-D7
 *   V3s PG0-PG4 → 74HCT245 → YM3438 A0,A1,/WR,/CS,/IC
 *   Si5351 CLK0 → 74HCT245 → YM3438 φM (7.670454 MHz)
 *   Si5351 I2C  → V3s PB6(SCL), PB7(SDA) (configured at boot, then released)
 *   YM3438 MOL/MOR → coupling caps → headphones
 *
 * Build: make GAME=examples/ym3438/main.c
 */
#include <stddef.h>
#include "jupiter.h"
#include "fighting_back.h"

/* ================================================================
 *  VGM parser — sends register writes to real hardware
 *
 *  VGM format: commands are single-byte opcodes followed by operands.
 *  We handle the subset needed for YM2612/YM3438 VGMs:
 *    0x52 aa dd  → write port 0 register aa = dd
 *    0x53 aa dd  → write port 1 register aa = dd
 *    0x61 nn nn  → wait nnnn samples (at 44100 Hz)
 *    0x62        → wait 735 samples (1/60 sec)
 *    0x63        → wait 882 samples (1/50 sec)
 *    0x66        → end of sound data
 *    0x70-0x7F   → wait (n+1) samples
 *    0x80-0x8F   → YM2612 DAC write + wait n samples
 *    0x67 ...    → data block (PCM data for DAC)
 *    0xE0 ...    → seek in data block
 * ================================================================ */

/* VGM header offsets */
#define VGM_OFS_EOF         0x04
#define VGM_OFS_VERSION     0x08
#define VGM_OFS_YM2612_CLK  0x2C
#define VGM_OFS_DATA_OFS    0x34
#define VGM_OFS_LOOP_OFS    0x1C
#define VGM_OFS_LOOP_SAMPS  0x20

static const uint8_t *vgm_data;
static uint32_t vgm_len;
static const uint8_t *vgm_stream;  /* current position in command stream */
static const uint8_t *vgm_loop;   /* loop point (NULL if no loop) */
static const uint8_t *vgm_pcm;    /* PCM data block base */
static uint32_t vgm_pcm_pos;      /* current PCM offset */
static uint32_t vgm_wait;         /* samples to wait before next command */
static uint8_t  vgm_done;

static uint32_t vgm_read32(uint32_t offset)
{
    return vgm_data[offset] | (vgm_data[offset+1] << 8) |
           (vgm_data[offset+2] << 16) | (vgm_data[offset+3] << 24);
}

static void vgm_hw_init(const uint8_t *data, uint32_t len)
{
    vgm_data = data;
    vgm_len = len;
    vgm_pcm = NULL;
    vgm_pcm_pos = 0;
    vgm_done = 0;
    vgm_wait = 0;

    /* Data offset (relative to 0x34) */
    uint32_t data_ofs = vgm_read32(VGM_OFS_DATA_OFS);
    if (data_ofs == 0) data_ofs = 0x0C;  /* pre-1.50 default */
    vgm_stream = data + 0x34 + data_ofs;

    /* Loop offset (relative to 0x1C) */
    uint32_t loop_ofs = vgm_read32(VGM_OFS_LOOP_OFS);
    vgm_loop = loop_ofs ? (data + 0x1C + loop_ofs) : NULL;

    uint32_t ym_clock = vgm_read32(VGM_OFS_YM2612_CLK);
    uart_puts("[vgm] YM2612 clock: "); uart_putdec(ym_clock); uart_puts(" Hz\n");
    uart_puts("[vgm] data at offset 0x"); uart_puthex((uint32_t)(vgm_stream - data));
    uart_puts(", loop at 0x");
    if (vgm_loop) uart_puthex((uint32_t)(vgm_loop - data));
    else uart_puts("none");
    uart_puts("\n");
}

/* Process VGM commands until we hit a wait or end. Returns samples to wait. */
static uint32_t vgm_hw_tick(void)
{
    while (!vgm_done) {
        uint8_t cmd = *vgm_stream++;

        if (cmd == 0x52) {
            /* Port 0 write */
            uint8_t addr = *vgm_stream++;
            uint8_t data = *vgm_stream++;
            ym3438_hw_vgm_write(0, addr, data);
        }
        else if (cmd == 0x53) {
            /* Port 1 write */
            uint8_t addr = *vgm_stream++;
            uint8_t data = *vgm_stream++;
            ym3438_hw_vgm_write(1, addr, data);
        }
        else if (cmd == 0x61) {
            /* Wait N samples */
            uint32_t n = vgm_stream[0] | (vgm_stream[1] << 8);
            vgm_stream += 2;
            return n;
        }
        else if (cmd == 0x62) {
            return 735;  /* 1/60 sec at 44100 Hz */
        }
        else if (cmd == 0x63) {
            return 882;  /* 1/50 sec */
        }
        else if (cmd == 0x66) {
            /* End of data — loop or stop */
            if (vgm_loop) {
                vgm_stream = vgm_loop;
                uart_puts("[vgm] loop\n");
            } else {
                vgm_done = 1;
                return 0;
            }
        }
        else if (cmd >= 0x70 && cmd <= 0x7F) {
            /* Short wait: (cmd & 0x0F) + 1 samples */
            return (cmd & 0x0F) + 1;
        }
        else if (cmd >= 0x80 && cmd <= 0x8F) {
            /* YM2612 DAC write from data block + wait */
            if (vgm_pcm) {
                ym3438_hw_vgm_write(0, 0x2A, vgm_pcm[vgm_pcm_pos]);
                vgm_pcm_pos++;
            }
            uint32_t wait = cmd & 0x0F;
            if (wait) return wait;
        }
        else if (cmd == 0x67) {
            /* Data block */
            vgm_stream++;  /* 0x66 compatibility byte */
            uint8_t type = *vgm_stream++;
            uint32_t size = vgm_stream[0] | (vgm_stream[1] << 8) |
                           (vgm_stream[2] << 16) | (vgm_stream[3] << 24);
            vgm_stream += 4;
            if (type == 0x00) {
                vgm_pcm = vgm_stream;
                uart_puts("[vgm] PCM block: "); uart_putdec(size); uart_puts(" bytes\n");
            }
            vgm_stream += size;
        }
        else if (cmd == 0xE0) {
            /* Seek in data block */
            vgm_pcm_pos = vgm_stream[0] | (vgm_stream[1] << 8) |
                          (vgm_stream[2] << 16) | (vgm_stream[3] << 24);
            vgm_stream += 4;
        }
        else {
            /* Unknown command — skip safely */
            uart_puts("[vgm] unknown cmd 0x"); uart_puthex(cmd); uart_puts("\n");
        }
    }
    return 0;
}

/* ================================================================
 *  Main
 * ================================================================ */

void main(void)
{
    uart_puts("\n\n=== Jupiter SDK — Real YM3438 VGM Player ===\n");
    timer_init();
    mmu_init();
    irq_init();

    /* Step 1: Program Si5351 clock (7.670454 MHz) via I2C on PB6/PB7,
     * then release PB6/PB7 back to GPIO for the data bus. */
    si5351_init();

    /* Step 2: Initialize YM3438 GPIO + reset the chip */
    ym3438_hw_init();

    /* Step 3: Parse VGM header */
    vgm_hw_init(vgm_fighting_back, vgm_fighting_back_len);

    /* Step 4: Play! Process commands, wait with hardware timer. */
    uart_puts("\nPlaying through real YM3438...\n\n");

    uint32_t total_samples = 0;
    uint32_t last_sec = 0;

    while (!vgm_done) {
        /* Process commands until a wait is needed */
        uint32_t wait_samples = vgm_hw_tick();

        if (wait_samples > 0) {
            /* Convert samples at 44100 Hz to microseconds:
             * us = samples * 1000000 / 44100 ≈ samples * 23 */
            uint32_t wait_us = (wait_samples * 1000000ULL) / 44100;
            uint32_t t0 = timer_read();
            while (ticks_to_us(timer_elapsed(t0, timer_read())) < wait_us)
                ;
            total_samples += wait_samples;
        }

        /* Print progress every second */
        uint32_t cur_sec = total_samples / 44100;
        if (cur_sec != last_sec) {
            uart_puts("t="); uart_putdec(cur_sec); uart_puts("s\n");
            last_sec = cur_sec;
        }
    }

    uart_puts("\nDone.\n");
    while (1);
}
