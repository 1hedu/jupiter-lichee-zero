#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static volatile uint32_t *test_register(uintptr_t address);
#define REG32(address) (*test_register(address))
#include "sdmmc_host.c"
static uint32_t irq_script[8], script_length, reads, ticks;
static volatile uint32_t test_irq;
static volatile uint32_t *test_register(uintptr_t address) {
    assert(address == SD_BASE + SD_IRQ_RAW);
    test_irq = irq_script[reads < script_length ? reads : script_length-1];
    ++reads;
    return &test_irq;
}
void uart_puts(const char *s) { (void)s; }
void uart_puthex(uint32_t v) { (void)v; }
uint32_t timer_read(void) { ticks += 24; return UINT32_MAX-ticks; }
uint32_t timer_elapsed(uint32_t a, uint32_t b) { return a-b; }
int pine64_timer_ready(void) { return 1; }
static void prepare(uint32_t first, uint32_t second, uint32_t third) {
    irq_script[0]=first; irq_script[1]=second; irq_script[2]=third;
    script_length=3; reads=0; ticks=0; last_irq_status=0;
}
int main(void) {
    prepare(IRQ_RESP_TIMEOUT, IRQ_RESP_TIMEOUT, IRQ_RESP_TIMEOUT|IRQ_CMD_DONE);
    assert(wait_irq(IRQ_CMD_DONE, 10)==PINE64_SD_ERR_TIMEOUT);
    assert(reads==3 && (last_irq_status&IRQ_CMD_DONE));
    prepare(IRQ_RESP_TIMEOUT, IRQ_RESP_TIMEOUT, IRQ_RESP_TIMEOUT);
    assert(wait_irq(IRQ_CMD_DONE, 10)==PINE64_SD_ERR_CONTROLLER);
    assert(reads<=11);
    prepare(IRQ_RESP_TIMEOUT|BIT(6), IRQ_RESP_TIMEOUT|BIT(6), IRQ_RESP_TIMEOUT|BIT(6)|IRQ_CMD_DONE);
    assert(wait_irq(IRQ_CMD_DONE, 10)==PINE64_SD_ERR_CONTROLLER);
    prepare(0, 0, IRQ_CMD_DONE);
    assert(wait_irq(IRQ_CMD_DONE, 10)==0);
    prepare(0, 0, 0);
    assert(wait_irq(IRQ_CMD_DONE, 10)==PINE64_SD_ERR_TIMEOUT);
    assert(!(last_irq_status&IRQ_RESP_TIMEOUT));
    prepare(IRQ_CMD_DONE, IRQ_CMD_DONE|IRQ_DATA_DONE, IRQ_CMD_DONE|IRQ_DATA_DONE|IRQ_AUTO_DONE);
    assert(wait_irq(IRQ_CMD_DONE|IRQ_DATA_DONE|IRQ_AUTO_DONE, 10)==0);
    puts("PASS: deferred response timeout waits for command completion, stalled controller bounded failure, mixed errors, command success, missing IRQ timeout, multi-block completion mask");
}
