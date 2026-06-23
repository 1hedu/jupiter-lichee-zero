/*
 * Jupiter SDK — Retro Controller Input
 *
 * GPIO bit-bang drivers for NES, SNES, Genesis (3/6-button), and N64.
 * All timing derived from PMU cycle counter at 1.2GHz.
 *
 * Pin mapping (all controllers on PF + PE, freeing PB/PG for YM3438):
 *   NES/SNES:  PF0=OUT, PF1=CLK, PF2=DATA
 *   Genesis:   PF0-PF5=D0-D5, PE1=SELECT
 *   N64:       PE20=DATA
 *
 * NES + N64 can be used simultaneously (no pin overlap).
 * Genesis is exclusive with NES/SNES (shares PF0-PF2).
 *
 * PF0-PF5 are SDC0 (TF card) — must release SD card before using.
 * PE1 is LCD_DE — confirmed unused by the panel (HV sync mode).
 * PE20 is CSI_FIELD — unused (MIPI CSI, not parallel).
 */
#include "jupiter.h"
#include "input.h"
#include "pmu.h"

/* ================================================================== */
/* GPIO registers                                                       */
/* ================================================================== */

/* Port E: offset 4 * 0x24 = 0x90 from PIO_BASE */
#define PE_CFG0     REG32(PIO_BASE + 0x90)  /* PE0-PE7 */
#define PE_CFG2     REG32(PIO_BASE + 0x98)  /* PE16-PE23 */
#define PE_DAT      REG32(PIO_BASE + 0xA0)
#define PE_PULL0    REG32(PIO_BASE + 0xAC)  /* PE0-PE15 */
#define PE_PULL1    REG32(PIO_BASE + 0xB0)  /* PE16-PE31 */

/* Port F: offset 5 * 0x24 = 0xB4 from PIO_BASE */
#define PF_CFG0     REG32(PIO_BASE + 0xB4)  /* PF0-PF7 */
#define PF_DAT      REG32(PIO_BASE + 0xC4)
#define PF_PULL0    REG32(PIO_BASE + 0xD0)  /* PF0-PF15 */

/* ================================================================== */
/* Pin definitions                                                      */
/* ================================================================== */

/* NES / SNES — Port F */
#define NES_OUT     (1u << 0)   /* PF0, output */
#define NES_CLK     (1u << 1)   /* PF1, output */
#define NES_DATA    (1u << 2)   /* PF2, input  */

/* Genesis — PF0-PF5 input, PE1 output */
#define GEN_D_MASK  0x3F        /* PF0-PF5: D0-D5 */
#define GEN_SELECT  (1u << 1)   /* PE1, output (SELECT/TH) */

/* N64 — PE20 bidirectional */
#define N64_DATA    (1u << 20)  /* PE20 */

/* delay_us / delay_ns moved to include/pmu.h so other lib/ TUs (joybus,
 * cpak) can use them without redeclaring. */

/* ================================================================== */
/* Pin helpers                                                          */
/* ================================================================== */
static inline void pf_set(uint32_t mask)   { PF_DAT |= mask; }
static inline void pf_clr(uint32_t mask)   { PF_DAT &= ~mask; }
static inline int  pf_read(uint32_t mask)  { return (PF_DAT & mask) ? 1 : 0; }
static inline uint32_t pf_read_mask(void)  { return PF_DAT & GEN_D_MASK; }

static inline void pe_set(uint32_t mask)   { PE_DAT |= mask; }
static inline void pe_clr(uint32_t mask)   { PE_DAT &= ~mask; }
static inline int  pe_read(uint32_t mask)  { return (PE_DAT & mask) ? 1 : 0; }

/* N64 joybus primitives extracted to lib/n64_joybus.c so cpak.c can
 * reuse the same protocol layer. Declarations live in n64_joybus.h. */
#include "n64_joybus.h"

/* ================================================================== */
/* State                                                                */
/* ================================================================== */
static int input_type;
static uint32_t curr_buttons;
static uint32_t prev_buttons;

/* ================================================================== */
/* NES protocol                                                         */
/* ================================================================== */
/*
 * Shift register (4021): OUT captures parallel state,
 * then CLK shifts out bits serially on DATA.
 *
 * Bit order: A, B, Select, Start, Up, Down, Left, Right
 * Active LOW (0 = pressed).
 */
input_state_t input_poll_nes(void)
{
    input_state_t st = {0};

    pf_set(NES_OUT);
    delay_us(12);
    pf_clr(NES_OUT);
    delay_us(6);

    uint8_t raw = 0;
    for (int i = 0; i < 8; i++) {
        if (!pf_read(NES_DATA)) raw |= (1 << i);
        pf_clr(NES_CLK);
        delay_us(6);
        pf_set(NES_CLK);
        delay_us(6);
    }

    if (raw & 0x01) st.buttons |= BTN_A;
    if (raw & 0x02) st.buttons |= BTN_B;
    if (raw & 0x04) st.buttons |= BTN_SELECT;
    if (raw & 0x08) st.buttons |= BTN_START;
    if (raw & 0x10) st.buttons |= BTN_UP;
    if (raw & 0x20) st.buttons |= BTN_DOWN;
    if (raw & 0x40) st.buttons |= BTN_LEFT;
    if (raw & 0x80) st.buttons |= BTN_RIGHT;

    st.connected = 1;
    return st;
}

/* ================================================================== */
/* SNES protocol                                                        */
/* ================================================================== */
input_state_t input_poll_snes(void)
{
    input_state_t st = {0};

    pf_set(NES_OUT);
    delay_us(12);
    pf_clr(NES_OUT);
    delay_us(6);

    uint16_t raw = 0;
    for (int i = 0; i < 16; i++) {
        if (!pf_read(NES_DATA)) raw |= (1 << i);
        pf_clr(NES_CLK);
        delay_us(6);
        pf_set(NES_CLK);
        delay_us(6);
    }

    if (raw & 0x0001) st.buttons |= BTN_B;
    if (raw & 0x0002) st.buttons |= BTN_Y;
    if (raw & 0x0004) st.buttons |= BTN_SELECT;
    if (raw & 0x0008) st.buttons |= BTN_START;
    if (raw & 0x0010) st.buttons |= BTN_UP;
    if (raw & 0x0020) st.buttons |= BTN_DOWN;
    if (raw & 0x0040) st.buttons |= BTN_LEFT;
    if (raw & 0x0080) st.buttons |= BTN_RIGHT;
    if (raw & 0x0100) st.buttons |= BTN_A;
    if (raw & 0x0200) st.buttons |= BTN_X;
    if (raw & 0x0400) st.buttons |= BTN_L;
    if (raw & 0x0800) st.buttons |= BTN_R;

    st.connected = 1;
    return st;
}

/* ================================================================== */
/* Genesis protocol                                                     */
/* ================================================================== */
input_state_t input_poll_genesis(void)
{
    input_state_t st = {0};
    uint32_t d;

    /* Cycle 1: SELECT HIGH */
    pe_set(GEN_SELECT);
    delay_us(6);
    d = pf_read_mask();
    if (!(d & 0x01)) st.buttons |= BTN_UP;
    if (!(d & 0x02)) st.buttons |= BTN_DOWN;
    if (!(d & 0x04)) st.buttons |= BTN_LEFT;
    if (!(d & 0x08)) st.buttons |= BTN_RIGHT;
    if (!(d & 0x10)) st.buttons |= BTN_B;
    if (!(d & 0x20)) st.buttons |= BTN_C;

    /* Cycle 2: SELECT LOW */
    pe_clr(GEN_SELECT);
    delay_us(6);
    d = pf_read_mask();
    if (!(d & 0x10)) st.buttons |= BTN_A;
    if (!(d & 0x20)) st.buttons |= BTN_START;

    /* Cycle 3: SELECT HIGH */
    pe_set(GEN_SELECT);
    delay_us(6);

    /* Cycle 4: SELECT LOW — detect 6-button */
    pe_clr(GEN_SELECT);
    delay_us(6);
    d = pf_read_mask();
    int is_6btn = ((d & 0x0F) == 0x00);

    if (is_6btn) {
        st.six_btn = 1;

        /* Cycle 5: SELECT HIGH → extra buttons */
        pe_set(GEN_SELECT);
        delay_us(6);
        d = pf_read_mask();
        if (!(d & 0x01)) st.buttons |= BTN_Z;
        if (!(d & 0x02)) st.buttons |= BTN_R;     /* Y → R */
        if (!(d & 0x04)) st.buttons |= BTN_L;     /* X → L */
        if (!(d & 0x08)) st.buttons |= BTN_MODE;

        /* Cycle 6: SELECT LOW (cleanup) */
        pe_clr(GEN_SELECT);
        delay_us(6);
    }

    /* Leave SELECT HIGH (idle) */
    pe_set(GEN_SELECT);

    st.connected = 1;
    return st;
}

/* ================================================================== */
/* N64 protocol                                                         */
/* ================================================================== */

/* n64_send_bit / n64_send_stop / n64_send_byte / n64_recv_bit live in
 * lib/n64_joybus.c — extracted so cpak.c can use them too. */

input_state_t input_poll_n64(void)
{
    input_state_t st = {0};

    /* Critical section: bit-bang timing is jitter-sensitive; if any IRQ
     * fires mid-transmission the bits drift. Audio mix tick @ ~1.87 ms
     * absolutely overlaps the 1+ ms read window. Mask IRQs across the
     * whole transaction. */
    uint32_t saved = n64_joybus_lock();

    n64_send_byte(0x01);
    n64_send_stop();

    uint8_t data[4] = {0};
    int got_all = 1;
    for (int byte = 0; byte < 4 && got_all; byte++) {
        for (int bit = 7; bit >= 0; bit--) {
            int v = n64_recv_bit();
            if (v < 0) { got_all = 0; break; }
            if (v) data[byte] |= (1 << bit);
        }
    }

    n64_joybus_unlock(saved);

    if (!got_all) {
        st.connected = 0;
        return st;
    }
    st.connected = 1;

    if (data[0] & 0x80) st.buttons |= BTN_A;
    if (data[0] & 0x40) st.buttons |= BTN_B;
    if (data[0] & 0x20) st.buttons |= BTN_Z;
    if (data[0] & 0x10) st.buttons |= BTN_START;
    if (data[0] & 0x08) st.buttons |= BTN_UP;
    if (data[0] & 0x04) st.buttons |= BTN_DOWN;
    if (data[0] & 0x02) st.buttons |= BTN_LEFT;
    if (data[0] & 0x01) st.buttons |= BTN_RIGHT;

    if (data[1] & 0x20) st.buttons |= BTN_L;
    if (data[1] & 0x10) st.buttons |= BTN_R;
    if (data[1] & 0x08) st.buttons |= BTN_C_UP;
    if (data[1] & 0x04) st.buttons |= BTN_C_DOWN;
    if (data[1] & 0x02) st.buttons |= BTN_C_LEFT;
    if (data[1] & 0x01) st.buttons |= BTN_C_RIGHT;

    st.stick_x = (int8_t)data[2];
    st.stick_y = (int8_t)data[3];

    return st;
}

/* ================================================================== */
/* Public API                                                           */
/* ================================================================== */

void input_init(int type)
{
    input_type = type;
    curr_buttons = 0;
    prev_buttons = 0;

    switch (type) {
    case INPUT_NES:
    case INPUT_SNES:
        /* PF0=output(OUT), PF1=output(CLK), PF2=input(DATA) */
        PF_CFG0 = (PF_CFG0 & ~0x000FFF) | 0x000011;
        /* Pull-up on PF2: bits [5:4] = 01 */
        PF_PULL0 = (PF_PULL0 & ~(3u << 4)) | (1u << 4);
        /* CLK idles HIGH, OUT idles LOW */
        pf_set(NES_CLK);
        pf_clr(NES_OUT);
        uart_puts("[input] NES/SNES init (PF0-PF2)\n");
        break;

    case INPUT_GENESIS:
        /* PF0-PF5 = input (D0-D5) */
        PF_CFG0 = (PF_CFG0 & 0xFF000000);  /* clear bits [23:0] */
        /* Pull-ups on PF0-PF5: each 2 bits = 01 */
        PF_PULL0 = (PF_PULL0 & ~0x0FFF) | 0x0555;
        /* PE1 = output (SELECT) — bits [7:4] of PE_CFG0 = 0001 */
        PE_CFG0 = (PE_CFG0 & ~(0xFu << 4)) | (0x1u << 4);
        /* SELECT idles HIGH */
        pe_set(GEN_SELECT);
        uart_puts("[input] Genesis init (PF0-5, PE1=SELECT)\n");
        break;

    case INPUT_N64:
        /* PE20 = input initially (open-drain with pull-up) */
        n64_pin_input();
        /* Pull-up on PE20: bits [9:8] of PE_PULL1 = 01 */
        PE_PULL1 = (PE_PULL1 & ~(3u << 8)) | (1u << 8);
        uart_puts("[input] N64 init (PE20)\n");
        break;
    }
}

input_state_t input_poll(void)
{
    input_state_t st;

    prev_buttons = curr_buttons;

    switch (input_type) {
    case INPUT_NES:     st = input_poll_nes();     break;
    case INPUT_SNES:    st = input_poll_snes();    break;
    case INPUT_GENESIS: st = input_poll_genesis(); break;
    case INPUT_N64:     st = input_poll_n64();     break;
    default:
        st = (input_state_t){0};
        break;
    }

    curr_buttons = st.buttons;
    return st;
}

uint32_t input_pressed(void)  { return curr_buttons & ~prev_buttons; }
uint32_t input_released(void) { return ~curr_buttons & prev_buttons; }
uint32_t input_held(void)     { return curr_buttons; }
