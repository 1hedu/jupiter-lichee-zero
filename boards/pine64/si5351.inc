/* Si5351A on the PINE A64+ Pi-2 header: PH2 SCL (5), PH3 SDA (3).
 * Private GPIO I2C uses the 24 MHz timer, independent of APB2 / audio PLL.
 * Every pin is either an input or an output driving LOW: external 3.3 V
 * pull-ups are required. No PB, PF, camera, PMIC or clock-tree writes.
 * Register programming follows Skyworks AN619 rev. 0.8.
 */
#include "jupiter.h"
#include "pmu.h"
#include "pine64_external.h"
#include "external_test.h"

#define SI_PH_CFG0 REG32(0x01c208fcu)
#define SI_PH_DAT  REG32(0x01c2090cu)
#define SI_PH_PULL REG32(0x01c20918u)
#define SI_SCL 2u
#define SI_SDA 3u
#define SI_ADDR 0x60u

static void si_pin(unsigned pin, int release)
{
    unsigned shift = pin * 4u;
    /* Preload LOW before making a pin an output, including after another
     * user of the PH bank has updated its data register. */
    if (!release)
        SI_PH_DAT &= ~(1u << pin);
    SI_PH_CFG0 = (SI_PH_CFG0 & ~(15u << shift)) |
                 (release ? 0u : (1u << shift));
    PINE_EXTERNAL_GPIO_CHANGED();
}

static int si_read_pin(unsigned pin)
{
    return !!(SI_PH_DAT & (1u << pin));
}

static void si_half_period(void) { delay_us(5u); }

static int si_scl_high(void)
{
    si_pin(SI_SCL, 1);
    uint32_t start = timer_read();
    unsigned polls = 100000u;
    while (!si_read_pin(SI_SCL)) {
        if (timer_elapsed(start, timer_read()) >= 24000u || !--polls)
            return -1; /* Bounded 1 ms clock-stretch / short-circuit wait. */
    }
    return 0;
}

static int si_stop(void)
{
    si_pin(SI_SCL, 0);
    si_pin(SI_SDA, 0);
    si_half_period();
    int rc = si_scl_high();
    si_half_period();
    si_pin(SI_SDA, 1);
    si_half_period();
    return rc || !si_read_pin(SI_SDA) ? -1 : 0;
}

static int si_start(void)
{
    si_pin(SI_SDA, 1);
    if (si_scl_high())
        return -1;
    si_half_period();
    if (!si_read_pin(SI_SDA))
        return -1;
    si_pin(SI_SDA, 0);
    si_half_period();
    si_pin(SI_SCL, 0);
    return 0;
}

static int si_send(uint8_t value)
{
    for (unsigned bit = 0; bit < 8u; ++bit) {
        si_pin(SI_SDA, !!(value & 0x80u));
        si_half_period();
        if (si_scl_high())
            return -1;
        si_half_period();
        si_pin(SI_SCL, 0);
        value <<= 1;
    }
    si_pin(SI_SDA, 1);
    si_half_period();
    if (si_scl_high())
        return -1;
    si_half_period();
    int nack = si_read_pin(SI_SDA);
    si_pin(SI_SCL, 0);
    return nack ? -1 : 0;
}

static int si_receive(uint8_t *value)
{
    uint8_t data = 0;
    si_pin(SI_SDA, 1);
    for (unsigned bit = 0; bit < 8u; ++bit) {
        si_half_period();
        if (si_scl_high())
            return -1;
        si_half_period();
        data = (uint8_t)((data << 1) | si_read_pin(SI_SDA));
        si_pin(SI_SCL, 0);
    }
    /* Single-byte read: NACK the last byte, then STOP. */
    si_half_period();
    if (si_scl_high())
        return -1;
    si_half_period();
    si_pin(SI_SCL, 0);
    *value = data;
    return 0;
}

static int si_write_reg(uint8_t reg, uint8_t value)
{
    int rc = si_start() || si_send(SI_ADDR << 1) ||
             si_send(reg) || si_send(value);
    if (si_stop())
        rc = -1;
    return rc ? -1 : 0;
}

static int si_read_reg(uint8_t reg, uint8_t *value)
{
    int rc = si_start() || si_send(SI_ADDR << 1) || si_send(reg) ||
             si_start() || si_send((SI_ADDR << 1) | 1u) || si_receive(value);
    if (si_stop())
        rc = -1;
    return rc ? -1 : 0;
}

static int si_bus_init(void)
{
    si_pin(SI_SCL, 1);
    si_pin(SI_SDA, 1);
    SI_PH_DAT &= ~((1u << SI_SCL) | (1u << SI_SDA));
    SI_PH_PULL = (SI_PH_PULL & ~((3u << 4) | (3u << 6))) |
                 (1u << 4) | (1u << 6);
    if (si_scl_high())
        return -1;
    /* If a reset interrupted a slave transmission, at most nine clocks
     * and STOP release that transaction. Never wait forever for a chip. */
    for (unsigned i = 0; i < 9u && !si_read_pin(SI_SDA); ++i) {
        si_pin(SI_SCL, 0);
        si_half_period();
        if (si_scl_high())
            return -1;
        si_half_period();
    }
    return si_stop();
}

/* 25 MHz crystal; PLLA = 25 MHz * (30 + 85227/125000) = 767045400 Hz.
 * PLL P1=3415, P2=34056, P3=125000. MS0 integer /100, R0 /1 gives
 * 7,670,454 Hz; its absolute accuracy follows the external crystal.
 * The generator is dedicated to this demo: all other outputs stay off. */
static const uint8_t si_program[][2] = {
    {3, 0xff},
    {16, 0x80}, {17, 0x80}, {18, 0x80}, {19, 0x80},
    {20, 0x80}, {21, 0x80}, {22, 0x80}, {23, 0x80},
    {26, 0xe8}, {27, 0x48}, {28, 0x00}, {29, 0x0d},
    {30, 0x57}, {31, 0x10}, {32, 0x85}, {33, 0x08},
    {42, 0x00}, {43, 0x01}, {44, 0x00}, {45, 0x30},
    {46, 0x00}, {47, 0x00}, {48, 0x00}, {49, 0x00},
    {165, 0x00}, /* CLK0 initial phase = 0. */
};

int pine64_si5351_start(void)
{
    uint8_t status = 0, value;
    const char *error = "I2C bus stuck (PH2 SCL / PH3 SDA)";
    int configured = 0;
    timer_init();
    if (si_bus_init())
        goto fail;
    error = "no I2C ACK/read at 0x60; attach a 3.3 V Si5351A with 25 MHz crystal";
    if (si_read_reg(0, &status))
        goto fail;
    for (unsigned i = 0; status & 0x80u; ++i) {
        error = "SYS_INIT timeout";
        if (i == 100u)
            goto fail;
        delay_us(1000u);
        if (si_read_reg(0, &status)) {
            error = "I2C read failed while waiting for SYS_INIT";
            goto fail;
        }
    }
    configured = 1;
    error = "register write/readback failed";
    for (unsigned i = 0; i < sizeof(si_program) / sizeof(si_program[0]); ++i) {
        uint8_t reg = si_program[i][0], data = si_program[i][1];
        if (si_write_reg(reg, data) || si_read_reg(reg, &value) || value != data)
            goto fail;
    }
    /* Preserve reserved bits. PLLA uses the crystal; no spread spectrum.
     * Ignore optional external OEB for CLK0 so its state is deterministic. */
    if (si_read_reg(15, &value) || si_write_reg(15, value & ~4u) ||
        si_read_reg(149, &value) || si_write_reg(149, value & ~0x80u) ||
        si_read_reg(9, &value) || si_write_reg(9, value | 1u) ||
        si_write_reg(16, 0x4fu) || /* integer MS0, PLLA, 8 mA drive. */
        si_write_reg(177, 0x20u)) /* Reset PLLA, not the SoC audio PLL. */
        goto fail;

    error = "PLLA did not lock or crystal signal is missing";
    for (unsigned i = 0; ; ++i) {
        if (si_read_reg(0, &status)) {
            error = "I2C read failed during PLL lock check";
            goto fail;
        }
        /* SYS_INIT, LOL_A, LOS_XTAL; unused PLLB / CLKIN are not errors. */
        if (!(status & 0xa8u))
            break;
        if (i == 100u)
            goto fail;
        delay_us(1000u);
    }
    error = "CLK0 enable/readback failed";
    if (si_write_reg(3, 0xfeu) || si_read_reg(3, &value) || value != 0xfeu ||
        si_read_reg(16, &value) || value != 0x4fu)
        goto fail;

    uart_puts("[si5351/pine64] I2C ACK, register readback and PLLA lock OK; CLK0 programmed 7670454 Hz\n");
    return 0;

fail:
    if (configured)
        (void)si_write_reg(3, 0xffu);
    si_pin(SI_SCL, 1);
    si_pin(SI_SDA, 1);
    uart_puts("[si5351/pine64] ");
    uart_puts(error);
    uart_puts("; clock unavailable\n");
    return -1;
}

void si5351_init(void) { (void)pine64_si5351_start(); }
