/*
 * Jupiter SDK — Nuked-SC55 bare-metal driver
 *
 * Replaces mcu.cpp's main() / work_thread() / audio_callback path with
 * a tight render loop that steps the H8 MCU instruction-by-instruction
 * until N stereo samples have been produced. Samples flow through the
 * MCU_PostSample callback hook patched into upstream mcu.cpp.
 */
#include <stdint.h>

#include "nuked_sc55.h"

/* Pull in the upstream MCU header so we can touch the global mcu_t state. */
#include "mcu.h"
#include "mcu_interrupt.h"
#include "mcu_timer.h"
#include "submcu.h"
#include "pcm.h"

/* Patched-in callback hook (defined in upstream mcu.cpp) */
extern "C" void MCU_SetSampleCallback(void (*cb)(int16_t left, int16_t right));

/* Globals defined across upstream files */
extern int rom2_mask;          /* mcu.cpp */
extern uint8_t rom1[];         /* mcu.cpp */
extern uint8_t rom2[];         /* mcu.cpp */

/* The bit/address scrambler used to load wave ROMs (mcu.cpp, no header) */
extern void unscramble(uint8_t *src, uint8_t *dst, int len);

/* mcu.cpp internal functions that aren't in any header */
extern void MCU_Init(void);
extern void MCU_Reset(void);
extern void MCU_ReadInstruction(void);
extern void MCU_UpdateUART_RX(void);
extern void MCU_UpdateUART_TX(void);
extern void MCU_UpdateAnalog(uint64_t cycles);
extern void TIMER_Reset(void); /* present upstream, absent from mcu_timer.h */

#ifdef JUPITER_BOARD_PINE64
extern "C" void pine64_service(void);
extern "C" void uart_puts(const char *s);
#endif

/* ---- Sample sink: forward to the user callback ---- */
static sc55_sample_cb_t s_user_cb = nullptr;
static bool s_initialized;
static volatile uint32_t s_samples_pending;
static bool s_carry_valid;
static int16_t s_carry_left, s_carry_right;

static void sample_sink(int16_t left, int16_t right)
{
    if (s_user_cb) s_user_cb(left, right);
}

/* ---- Public API ---- */

extern "C" void sc55_set_sample_callback(sc55_sample_cb_t cb)
{
    s_user_cb = cb;
}

extern "C" int sc55_init(const uint8_t *rom1_data, uint32_t rom1_len,
                          const uint8_t *rom2_data, uint32_t rom2_len,
                          const uint8_t *waverom1_data, uint32_t waverom1_len,
                          const uint8_t *waverom2_data, uint32_t waverom2_len,
                          const uint8_t *rom_sm_data, uint32_t rom_sm_len)
{
    s_initialized = false;
    s_samples_pending = 0;
    s_carry_valid = false;
    /* Reject short/mismatched buffers before unscramble can read them. */
    if (!rom1_data || !rom2_data || !waverom1_data || !waverom2_data || !rom_sm_data ||
        rom1_len != 0x8000 || rom2_len != 0x80000 ||
        waverom1_len != 0x200000 || waverom2_len != 0x100000 || rom_sm_len != 0x1000)
        return -1;

    /* SC-55 mk2 model selection */
    romset = ROM_SET_MK2;
    mcu_mk1 = 0;
    mcu_cm300 = 0;
    mcu_st = 0;
    mcu_jv880 = 0;
    mcu_scb55 = 0;
    mcu_sc155 = 0;

    /* Copy rom1 (32K) and rom2 (512K) directly */
    if (rom1_len > 0x8000)  rom1_len = 0x8000;
    if (rom2_len > 0x80000) rom2_len = 0x80000;
    for (uint32_t i = 0; i < rom1_len; i++) rom1[i] = rom1_data[i];
    for (uint32_t i = 0; i < rom2_len; i++) rom2[i] = rom2_data[i];
    rom2_mask = (int)(rom2_len - 1);

    /* Wave ROMs need bit/address scrambling — use upstream unscramble().
     * Note: unscramble() reads `len` bytes from src; src must be at
     * least `len` long. Our embedded ROMs match the expected sizes. */
    if (waverom1_len >= 0x200000) {
        unscramble((uint8_t *)waverom1_data, waverom1, 0x200000);
    }
    if (waverom2_len >= 0x100000) {
        unscramble((uint8_t *)waverom2_data, waverom2, 0x100000);
    }

    /* Sub-MCU ROM (4K) */
    if (rom_sm_len > 0x1000) rom_sm_len = 0x1000;
    for (uint32_t i = 0; i < rom_sm_len; i++) sm_rom[i] = rom_sm_data[i];

    /* Wire our sample sink into the patched MCU_PostSample hook */
    MCU_SetSampleCallback(sample_sink);

    /* Match upstream startup: the sub-MCU reset loads its ROM reset
     * vector, and PCM_Reset clears its cycle counter and voice state.
     * Merely zeroing the H8 leaves the mkII's MIDI sub-MCU at PC=0. */
    uart_read_ptr = uart_write_ptr = 0;
    TIMER_Reset();
    MCU_Init();
    MCU_Reset();
    SM_Reset();
    PCM_Reset();
    s_initialized = true;
    return 0;
}

extern "C" void sc55_post_uart(uint8_t b)
{
    if (s_initialized) MCU_PostUART(b);
}

static void counting_sink(int16_t left, int16_t right)
{
    if (s_samples_pending) {
        --s_samples_pending;
        if (s_user_cb) s_user_cb(left, right);
    } else {
        /* PCM_Update may produce two oversampled frames in one MCU
         * step. Keep the extra frame for the next render call instead
         * of leaking an extra callback or dropping part of the audio. */
        s_carry_left = left;
        s_carry_right = right;
        s_carry_valid = true;
    }
}

/* Run one MCU instruction + per-cycle subsystem updates, mirroring
 * the upstream work_thread() body without SDL/lcd glue. */
static inline void sc55_step_one(void)
{
    if (!mcu.ex_ignore) MCU_Interrupt_Handle();
    else                mcu.ex_ignore = 0;

    if (!mcu.sleep) MCU_ReadInstruction();

    mcu.cycles += 12;  /* upstream FIXME: assume 12 cycles per instruction */

    PCM_Update(mcu.cycles);
    TIMER_Clock(mcu.cycles);

    if (!mcu_mk1 && !mcu_jv880 && !mcu_scb55) {
        SM_Update(mcu.cycles);
    } else {
        MCU_UpdateUART_RX();
        MCU_UpdateUART_TX();
    }

    MCU_UpdateAnalog(mcu.cycles);
}

extern "C" void sc55_warmup(uint32_t milliseconds)
{
    if (!s_initialized || !milliseconds) return;
    if (milliseconds > 10000u) milliseconds = 10000u;
    /* PCM firmware changes its slot count and oversampling while it
     * boots, so counting nominal 66207 Hz output frames is not a clock.
     * The upstream mkII work loop models a 24 MHz MCU with 12 cycles
     * per instruction. Advance that clock while discarding all output. */
    uint64_t steps = (uint64_t)milliseconds * 2000u;
    uint32_t service_count = 0;
    s_carry_valid = false;
    s_samples_pending = 0;
    MCU_SetSampleCallback(nullptr);
    while (steps--) {
        sc55_step_one();
#ifdef JUPITER_BOARD_PINE64
        if (++service_count == 4096u) {
            service_count = 0;
            pine64_service();
        }
#else
        (void)service_count;
#endif
    }
    MCU_SetSampleCallback(sample_sink);
}

extern "C" void sc55_render(uint32_t frames)
{
    if (!s_initialized || !frames) return;
    /* Re-arm the counting sink (replaces the plain user callback for
     * the duration of this call so we know when to stop). */
    MCU_SetSampleCallback(counting_sink);
    s_samples_pending = frames;
    if (s_carry_valid) {
        s_carry_valid = false;
        counting_sink(s_carry_left, s_carry_right);
    }

    /* Upstream advances 12 emulated cycles/instruction. At 24 MHz and
     * 66207 Hz the normal case is about 30 instructions/output frame.
     * Keep a generous finite bound, without frames*1024 overflowing. */
    uint64_t safety = (uint64_t)frames * 1024u;
    uint32_t service_count = 0;
    while (s_samples_pending && safety) {
        sc55_step_one();
        safety--;
#ifdef JUPITER_BOARD_PINE64
        if (++service_count == 4096u) {
            service_count = 0;
            pine64_service();
        }
#else
        (void)service_count;
#endif
    }

    MCU_SetSampleCallback(sample_sink);
    if (s_samples_pending) {
        /* The caller can detect a short render from its callback count.
         * Do not manufacture silence and report it as emulator output. */
        s_initialized = false;
#ifdef JUPITER_BOARD_PINE64
        uart_puts("[sc55/pine64] sample progress timeout; render stopped\n");
#endif
    }
}

extern "C" uint32_t sc55_native_rate(void)
{
    return (mcu_mk1 || mcu_jv880) ? 64000u : 66207u;
}
