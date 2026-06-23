/*
 * Jupiter SDK — Retro Controller Input
 *
 * Supports NES, SNES, Genesis (3+6 button), and N64 controllers
 * via GPIO bit-banging on the V3s.
 *
 * Pin mapping — all controllers on PF + PE (frees PB/PG for YM3438):
 *
 *   NES / SNES:
 *     PF0 = OUT    (output)        — 2.54mm header, after SD release
 *     PF1 = CLK    (output)
 *     PF2 = DATA   (input, pull-up)
 *
 *   Genesis:
 *     PF0 = D0     (input, pull-up) — Up / Z
 *     PF1 = D1     (input, pull-up) — Down / Y
 *     PF2 = D2     (input, pull-up) — Left / X
 *     PF3 = D3     (input, pull-up) — Right / Mode
 *     PF4 = D4     (input, pull-up) — TL: A / B
 *     PF5 = D5     (input, pull-up) — TR: Start / C
 *     PE1 = SELECT (output, TH line)
 *
 *   N64:
 *     PE20 = DATA  (bidirectional, open-drain, pull-up)
 *
 * NES + N64 can be used simultaneously (no pin overlap).
 * Genesis is exclusive with NES/SNES (shares PF0-PF2).
 * PF0-PF5 are SDC0 — release SD card before using controllers.
 * All controllers coexist with YM3438 (PB0-PB7 + PG0-PG4).
 */
#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

/* Controller types */
#define INPUT_NES      0
#define INPUT_SNES     1
#define INPUT_GENESIS  2
#define INPUT_N64      3

/* Unified button bitmask (superset of all controllers) */
#define BTN_A       (1u << 0)
#define BTN_B       (1u << 1)
#define BTN_X       (1u << 2)   /* SNES, N64 (C-Down) */
#define BTN_Y       (1u << 3)   /* SNES, N64 (C-Left) */
#define BTN_START   (1u << 4)
#define BTN_SELECT  (1u << 5)   /* NES, SNES */
#define BTN_UP      (1u << 6)
#define BTN_DOWN    (1u << 7)
#define BTN_LEFT    (1u << 8)
#define BTN_RIGHT   (1u << 9)
#define BTN_L       (1u << 10)  /* SNES, Genesis (X), N64 */
#define BTN_R       (1u << 11)  /* SNES, Genesis (Y), N64 */
#define BTN_C       (1u << 12)  /* Genesis */
#define BTN_Z       (1u << 13)  /* Genesis, N64 */
#define BTN_MODE    (1u << 14)  /* Genesis 6-button */
#define BTN_C_UP    (1u << 15)  /* N64 */
#define BTN_C_DOWN  (1u << 16)  /* N64 */
#define BTN_C_LEFT  (1u << 17)  /* N64 */
#define BTN_C_RIGHT (1u << 18)  /* N64 */

/* Controller state */
typedef struct {
    uint32_t buttons;    /* bitmask of pressed buttons (BTN_*) */
    int8_t   stick_x;    /* N64 analog stick X (-80..+80), 0 for others */
    int8_t   stick_y;    /* N64 analog stick Y (-80..+80), 0 for others */
    uint8_t  connected;  /* 1 if controller responded */
    uint8_t  six_btn;    /* Genesis: 1 if 6-button controller detected */
} input_state_t;

/* Initialize GPIO for the specified controller type.
 * Call once at startup. Configures pin direction and pull-ups.
 * Can be called multiple times for different types — pins don't conflict
 * (NES/SNES use PG0-2, Genesis uses PG3+PB0-5, N64 uses PG4). */
void input_init(int type);

/* Poll the controller and return current state.
 * Uses the last type passed to input_init().
 * Takes ~50-100us depending on controller type. */
input_state_t input_poll(void);

/* Poll a specific controller type directly (for multi-controller setups).
 * Call input_init() for each type first to configure the GPIO. */
input_state_t input_poll_nes(void);
input_state_t input_poll_snes(void);
input_state_t input_poll_genesis(void);
input_state_t input_poll_n64(void);

/* Edge detection helpers (updated by input_poll, not poll_*) */
uint32_t input_pressed(void);    /* buttons pressed THIS frame */
uint32_t input_released(void);   /* buttons released THIS frame */
uint32_t input_held(void);       /* buttons held (current state) */

#endif /* INPUT_H */
