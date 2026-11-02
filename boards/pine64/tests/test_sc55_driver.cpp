/* Tests the actual driver against a small emulator fixture. No ROM assets.
 * The fixture deliberately emits two frames from each PCM_Update so odd
 * render requests exercise the oversampling carry across API calls.
 *
 * c++ -std=c++11 -Iinclude -Ithird_party/nuked_sc55/src \
 *   -Ithird_party/nuked_sc55_stubs lib/nuked_sc55_driver.cpp \
 *   boards/pine64/tests/test_sc55_driver.cpp -o /tmp/test_sc55_driver
 * /tmp/test_sc55_driver
 */
#include "nuked_sc55.h"
#include "mcu.h"
#include "pcm.h"
#include "submcu.h"
#include <cassert>
#include <cstdio>
#include <cstring>

mcu_t mcu;
pcm_t pcm;
submcu_t sm;
int romset, rom2_mask;
int mcu_mk1, mcu_cm300, mcu_st, mcu_jv880, mcu_scb55, mcu_sc155;
uint8_t rom1[0x8000], rom2[0x80000];
uint8_t waverom1[0x200000], waverom2[0x100000], sm_rom[0x1000];
uint32_t uart_read_ptr, uart_write_ptr;

static void (*sink)(int16_t, int16_t);
static unsigned mcu_resets, sm_resets, pcm_resets, timer_resets;
static unsigned emitted, received, uart_bytes;
static bool stalled;

extern "C" void MCU_SetSampleCallback(void (*cb)(int16_t, int16_t)) { sink = cb; }
void unscramble(uint8_t *src, uint8_t *dst, int len) { std::memcpy(dst, src, len); }
void MCU_Init(void) { std::memset(&mcu, 0, sizeof(mcu)); }
void MCU_Reset(void) { ++mcu_resets; }
void SM_Reset(void) { ++sm_resets; sm.pc = 0x1234; }
void PCM_Reset(void) { ++pcm_resets; std::memset(&pcm, 0, sizeof(pcm)); emitted = 0; }
void TIMER_Reset(void) { ++timer_resets; }
void MCU_PostUART(uint8_t) { ++uart_bytes; }
void MCU_ReadInstruction(void) { }
void MCU_Interrupt_Handle(void) { }
void MCU_UpdateUART_RX(void) { }
void MCU_UpdateUART_TX(void) { }
void MCU_UpdateAnalog(uint64_t) { }
void TIMER_Clock(uint64_t) { }
void SM_Update(uint64_t) { }
void PCM_Update(uint64_t)
{
    if (stalled) return;
    for (int n = 0; n < 2; ++n) {
        ++emitted;
        if (sink) sink((int16_t)emitted, (int16_t)-emitted);
    }
}

static void receive(int16_t left, int16_t right)
{
    ++received;
    assert(left == (int16_t)received && right == (int16_t)-received);
}

static int initialize()
{
    static uint8_t data[0x200000];
    return sc55_init(data, 0x8000, data, 0x80000,
                     data, 0x200000, data, 0x100000, data, 0x1000);
}

int main()
{
    sc55_post_uart(0x90);
    assert(!uart_bytes);
    assert(initialize() == 0);
    assert(mcu_resets == 1 && sm_resets == 1 && pcm_resets == 1 && timer_resets == 1);
    assert(sm.pc == 0x1234 && sc55_native_rate() == 66207);
    sc55_set_sample_callback(receive);
    unsigned expected = 0;
    for (unsigned count = 1; count <= 31; ++count) {
        sc55_render(count);
        expected += count;
        assert(received == expected);
    }
    assert(emitted == expected);  /* even total, so no pending carry */
    sc55_post_uart(0x90);
    assert(uart_bytes == 1);

    /* Warmup must advance time and discard output, regardless of PCM rate. */
    uint64_t cycles = mcu.cycles;
    sc55_warmup(10);
    assert(mcu.cycles - cycles == 240000);
    assert(received == expected);

    assert(initialize() == 0);
    received = 0;
    stalled = true;
    sc55_render(1);
    assert(!received && mcu.cycles == 1024 * 12);
    cycles = mcu.cycles;
    sc55_render(100);
    assert(mcu.cycles == cycles);  /* stopped after a progress failure */

    assert(sc55_init(nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0) < 0);
    sc55_render(1);
    assert(mcu.cycles == cycles);
    std::puts("PASS SC55 resets, exact oversampled callbacks, clock warmup, and bounded stall");
}
