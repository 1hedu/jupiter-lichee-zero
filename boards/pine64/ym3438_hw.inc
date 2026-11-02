/* PINE A64+ YM3438 write-only GPIO interface. See EXTERNAL.md.
 * D0..D7 = PC0..PC7; A0/A1/nWR/nCS/nIC = PC8..PC12.
 * All are on the Pi-2 header. PF0..PF5 (boot SD) are never touched.
 * A 5 V HCT buffer translates the 3.3 V SoC outputs to YM3438 inputs;
 * /RD must remain high, with no 5 V return path into the PINE64.
 */
#include "jupiter.h"
#include "pmu.h"
#include "pine64_external.h"
#include "external_test.h"

#define YM_PC_CFG0 REG32(0x01c20848u)
#define YM_PC_CFG1 REG32(0x01c2084cu)
#define YM_PC_DAT  REG32(0x01c20858u)
#define YM_PC_PULL REG32(0x01c20864u)
#define YM_A0  (1u << 8)
#define YM_A1  (1u << 9)
#define YM_WR  (1u << 10)
#define YM_CS  (1u << 11)
#define YM_IC  (1u << 12)
#define YM_CTRL (YM_A0 | YM_A1 | YM_WR | YM_CS | YM_IC)
#define YM_PINS (YM_CTRL | 0xffu)

static int ym_clock_source = YM_CLK_SI5351;
static int ym_status;
static uint32_t ym_clock_hz;
static uint32_t ym_last_write;
static uint32_t ym_busy_ticks;
static uint8_t fnum_hi_cache[6];
/* CH3 operator frequencies live on port 0 at AC..AE / A8..AA. */
static uint8_t fnum_hi_cache_ch3[3];

static void ym_ctrl(uint32_t value)
{
    YM_PC_DAT = (YM_PC_DAT & ~YM_CTRL) | (value & YM_CTRL);
    PINE_EXTERNAL_GPIO_CHANGED();
}

static void ym_wait_ready(void)
{
    while (timer_elapsed(ym_last_write, timer_read()) < ym_busy_ticks)
        ;
}

static void ym3438_bus_write(uint8_t port, uint8_t data)
{
    if (ym_status != PINE64_YM_WRITE_ONLY_READY)
        return;
    ym_wait_ready();
    YM_PC_DAT = (YM_PC_DAT & ~0xffu) | data;
    uint32_t control = YM_WR | YM_IC |
                       ((port & 1u) ? YM_A0 : 0u) |
                       ((port & 2u) ? YM_A1 : 0u);
    ym_ctrl(control); /* Select chip, keep write inactive. */
    delay_ns(250u);
    ym_ctrl(control & ~YM_WR);
    delay_ns(500u); /* At least 200 ns write low, independent of CPU PLL. */
    ym_ctrl(control);
    delay_ns(250u);
    ym_ctrl(YM_CTRL);
    ym_last_write = timer_read();
    /* Rounded up from 17 / 83 master-clock periods at 7.67 MHz.
     * Enforced here even if callers forget the public busy_wait(). */
    ym_busy_ticks = (port & 1u) ? 11u * 24u : 3u * 24u;
}

int pine64_ym3438_status(void) { return ym_status; }
uint32_t pine64_ym3438_clock_hz(void) { return ym_clock_hz; }

void ym3438_hw_set_clock(int source) { ym_clock_source = source; }

void ym3438_hw_init(void)
{
    ym_status = PINE64_YM_UNINITIALIZED;
    ym_clock_hz = 0;
    timer_init();
    if (ym_clock_source == YM_CLK_SI5351) {
        if (pine64_si5351_start()) {
            ym_status = PINE64_YM_CLOCK_FAILED;
            uart_puts("[ym3438/pine64] external clock unavailable; GPIO writes disabled\n");
            return;
        }
        ym_clock_hz = 7670454u;
    } else if (ym_clock_source == YM_CLK_EXT_XTAL) {
        ym_clock_hz = 8000000u;
        uart_puts("[ym3438/pine64] external 8 MHz oscillator required at phi-M; frequency is not measurable on this write-only bus\n");
    } else {
        ym_status = PINE64_YM_CLOCK_UNSUPPORTED;
        uart_puts("[ym3438/pine64] CSI MCLK is on the camera connector, not Pi-2; select Si5351 or external 8 MHz\n");
        return;
    }

    /* Preload all idle levels BEFORE switching pins to outputs. Preserve
     * PC13..PC16; no complete-bank data writes or other-bank pin changes. */
    YM_PC_DAT = (YM_PC_DAT & ~YM_PINS) | YM_CTRL;
    YM_PC_PULL &= ~0x03ffffffu; /* No pulls for our thirteen push-pull pins. */
    YM_PC_CFG0 = 0x11111111u;
    YM_PC_CFG1 = (YM_PC_CFG1 & ~0x000fffffu) | 0x00011111u;
    ym_ctrl(YM_CTRL & ~YM_IC);
    delay_us(10u); /* /IC low longer than 24 master-clock cycles. */
    ym_ctrl(YM_CTRL);
    delay_us(30u);
    for (unsigned i = 0; i < sizeof(fnum_hi_cache); ++i)
        fnum_hi_cache[i] = 0;
    for (unsigned i = 0; i < sizeof(fnum_hi_cache_ch3); ++i)
        fnum_hi_cache_ch3[i] = 0;
    ym_busy_ticks = 0;
    ym_last_write = timer_read();
    ym_status = PINE64_YM_WRITE_ONLY_READY;
    uart_puts("[ym3438/pine64] PC0..PC12 write interface configured; /RD tied high, YM3438 presence cannot be detected\n");
}

void ym3438_hw_write(uint8_t addr, uint8_t data)
{
    ym3438_bus_write(0, addr);
    ym3438_bus_write(1, data);
}

void ym3438_hw_write_port1(uint8_t addr, uint8_t data)
{
    ym3438_bus_write(2, addr);
    ym3438_bus_write(3, data);
}

void ym3438_hw_busy_wait(void)
{
    if (ym_status == PINE64_YM_WRITE_ONLY_READY)
        ym_wait_ready();
}

static void ym_write_pair(uint8_t port, uint8_t addr, uint8_t data)
{
    if (port)
        ym3438_hw_write_port1(addr, data);
    else
        ym3438_hw_write(addr, data);
}

void ym3438_hw_vgm_write(uint8_t port, uint8_t addr, uint8_t data)
{
    if (ym_status != PINE64_YM_WRITE_ONLY_READY)
        return;
    port = !!port;
    unsigned ch = (addr & 3u) + (port ? 3u : 0u);
    if (ym_clock_source == YM_CLK_EXT_XTAL && addr >= 0xa4u && addr <= 0xa6u) {
        fnum_hi_cache[ch] = data;
        ym_write_pair(port, addr, data);
    } else if (ym_clock_source == YM_CLK_EXT_XTAL && addr >= 0xa0u && addr <= 0xa2u) {
        uint8_t hi = fnum_hi_cache[ch];
        uint32_t fnum = ((((uint32_t)hi & 7u) << 8) | data);
        fnum = (fnum * 62837u) >> 16; /* Original NTSC/8 MHz compensation. */
        ym_write_pair(port, addr + 4u, (hi & 0xf8u) | (fnum >> 8));
        ym_write_pair(port, addr, (uint8_t)fnum);
    } else if (ym_clock_source == YM_CLK_EXT_XTAL && !port &&
               addr >= 0xacu && addr <= 0xaeu) {
        fnum_hi_cache_ch3[addr & 3u] = data;
        ym_write_pair(0, addr, data);
    } else if (ym_clock_source == YM_CLK_EXT_XTAL && !port &&
               addr >= 0xa8u && addr <= 0xaau) {
        uint8_t hi = fnum_hi_cache_ch3[addr & 3u];
        uint32_t fnum = ((((uint32_t)hi & 7u) << 8) | data);
        fnum = (fnum * 62837u) >> 16;
        ym_write_pair(0, addr + 4u, (hi & 0xf8u) | (fnum >> 8));
        ym_write_pair(0, addr, (uint8_t)fnum);
    } else {
        ym_write_pair(port, addr, data);
    }
    ym3438_hw_busy_wait();
}
