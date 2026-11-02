
#include <assert.h>
#include <stdio.h>
#include "sdmmc_host.c"
void uart_puts(const char *s) { (void)s; }
void uart_puthex(uint32_t v) { (void)v; }
uint32_t timer_read(void) { assert(!"unexpected hardware path"); return 0; }
uint32_t timer_elapsed(uint32_t a, uint32_t b) { return a-b; }
int pine64_timer_ready(void) { return 1; }
int main(void) {
    const uint32_t sd32[4] = {0,0xffff0000u,0,0x40000000u};
    const uint32_t sd256[4] = {0,0xffff0000u,7,0x40000000u};
    const uint32_t sd2tb[4] = {0,0xffff0000u,63,0x40000000u};
    const uint32_t sduc[4] = {0,0,0,0x80000000u};
    const uint32_t sd2gb_v1[4] = {0,0xc0038000u,0x000a03ffu,0};
    const uint32_t sd128mb_v1[4] = {0,0xc0030000u,0x000900ffu,0};
    const uint32_t invalid_v1[4] = {0,0,0x00080000u,0};
    assert(csd_capacity(sd32)==67108864u);
    assert(csd_capacity(sd256)==536870912u);
    assert(csd_capacity(sd2tb)==0);
    assert(csd_capacity(sduc)==0);
    assert(csd_capacity(sd2gb_v1)==4194304u);
    assert(csd_capacity(sd128mb_v1)==262144u);
    assert(csd_capacity(invalid_v1)==0);
    uint32_t clk=oscillator_clock_value(400000u);
    assert((clk>>24&3)==0);
    assert((clk&0x80000000u)!=0);
    assert(12000000u/((1u<<((clk>>16)&3))*((clk&15)+1))==400000u);
    assert(oscillator_clock_value(12000000u)==0x80000000u);
    uint32_t buf[512];
    assert(validate_request(0,1,buf)==PINE64_SD_ERR_NOT_READY);
    card.initialised=1; card.is_sdhc=1; card.num_blocks=65536;
    assert(validate_request(65535,1,buf)==0);
    assert(validate_request(65535,2,buf)==PINE64_SD_ERR_ARGUMENT);
    assert(validate_request(UINT32_MAX,2,buf)==PINE64_SD_ERR_ARGUMENT);
    assert(validate_request(0,0,NULL)==0);
    assert(validate_request(0,1,NULL)==PINE64_SD_ERR_ARGUMENT);
    assert(validate_request(0,1,(char *)buf+1)==PINE64_SD_ERR_ARGUMENT);
    assert(validate_request(0,1,(void *)(UINTPTR_MAX-3))==PINE64_SD_ERR_ARGUMENT);
    assert(sdmmc_read_blocks(0,0,NULL)==0);
    card_write_protected=1;
    assert(sdmmc_write_blocks(0,1,buf)==PINE64_SD_ERR_CARD);
    card.is_sdhc=0; card.num_blocks=UINT32_MAX;
    assert(validate_request(8388608u,1,buf)==PINE64_SD_ERR_ARGUMENT);
    puts("PASS: CSD capacity, SDUC/2TiB rejection, A64 clock divider, range/alignment/pointer checks, zero reads, protected writes");
}
