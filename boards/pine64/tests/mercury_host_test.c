/* Production A64 MMIO driver -> captured electrical byte stream ->
 * unchanged 4fb5904e Pico SPI receiver -> unchanged list parser/raster.
 * Only controller registers, GPIO/time and Pico peripheral reads are
 * mocked. This does not model HDMI electrical timing or prove hardware
 * reception, and deliberately tests the absence of a reply channel. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include "jupiter.h"
#include "pine64_mercury.h"
#include "jupiter32x.h"
#include "hardware/spi.h"
#include "../../../examples/mercury_pine64/scenes.h"

enum {
    GCR=0x01c68004u,TCR=0x01c68008u,IER=0x01c68010u,ISR=0x01c68014u,
    FCR=0x01c68018u,FSR=0x01c6801cu,CCR=0x01c68024u,MBC=0x01c68030u,
    MTC=0x01c68034u,BCC=0x01c68038u,TXD=0x01c68200u,
    GATE=0x01c20060u,CLOCK=0x01c200a0u,RESET=0x01c202c0u,
    PC_CFG=0x01c20848u,PC_DATA=0x01c20858u,PC_DRIVE=0x01c2085cu,PC_PULL=0x01c20864u,
    PH_CFG=0x01c208fcu,PH_DATA=0x01c2090cu,PH_PULL=0x01c20918u
};
static struct {
    uint32_t gcr,tcr,ier,isr,fcr,ccr,mbc,mtc,bcc,gate,clock,reset;
    uint32_t pc_cfg,pc_data,pc_drive,pc_pull,ph_cfg,ph_data,ph_pull;
    uint32_t us,selected_at,assertions,releases,bursts,max_burst,service;
    uint8_t fifo[64],wire[PINE64_MERCURY_MAX_LIST+2];
    size_t fifo_used,wire_used;
    int selected,timer_ready,reset_stuck,fifo_stuck,exchange_stuck,fifo_error;
    int clock_stuck,data_stuck;
} hw;

static uint32_t *reg32(uintptr_t address)
{
    switch(address) {
    case GCR:return &hw.gcr; case TCR:return &hw.tcr; case IER:return &hw.ier;
    case ISR:return &hw.isr; case FCR:return &hw.fcr; case CCR:return &hw.ccr;
    case MBC:return &hw.mbc; case MTC:return &hw.mtc; case BCC:return &hw.bcc;
    case GATE:return &hw.gate; case CLOCK:return &hw.clock; case RESET:return &hw.reset;
    case PC_CFG:return &hw.pc_cfg; case PC_DATA:return &hw.pc_data;
    case PC_DRIVE:return &hw.pc_drive; case PC_PULL:return &hw.pc_pull;
    case PH_CFG:return &hw.ph_cfg; case PH_DATA:return &hw.ph_data; case PH_PULL:return &hw.ph_pull;
    default: assert(!"unexpected MMIO (no CSI/DSI, PF/SD or PLL access allowed)"); return NULL;
    }
}
uint32_t mercury_test_read32(uintptr_t address)
{
    if(address==FSR) return (uint32_t)hw.fifo_used << 16;
    if(address==PH_DATA) {
        uint32_t v=hw.ph_data;
        for(unsigned pin=2;pin<=3;++pin) {
            if(((hw.ph_cfg>>(pin*4))&15u)==0) v|=1u<<pin;
            else assert(!((hw.ph_data>>pin)&1u)); /* open drain never drives high */
        }
        if(hw.clock_stuck) v&=~4u;
        if(hw.data_stuck) v&=~8u;
        return v;
    }
    return *reg32(address);
}
void mercury_test_write32(uintptr_t address,uint32_t value)
{
    if(address==PC_DATA) {
        if((hw.pc_cfg&0xf000u)==0x1000u) {
            int selected=!(value&8u);
            if(selected&&!hw.selected) {
                ++hw.assertions; hw.wire_used=0; hw.selected_at=hw.us;
            } else if(!selected&&hw.selected) ++hw.releases;
            hw.selected=selected;
        }
    } else if(address==GCR&&value&BIT(31)) {
        if(!hw.reset_stuck) value&=~BIT(31);
    } else if(address==FCR) {
        hw.fifo_used=0;
        if(!hw.fifo_stuck) value=0;
    } else if(address==ISR) {
        hw.isr&=~value;
        return;
    }
    *reg32(address)=value;
    if(address!=TCR||!(value&BIT(31))) return;
    assert(hw.selected);
    assert(hw.us-hw.selected_at>=PINE64_MERCURY_SELECT_US);
    assert((value&3u)==0); /* existing firmware is mode 0 */
    assert(value&BIT(8)); /* RX discard: MISO is not used */
    assert(hw.ccr==(BIT(12)|11u)&&hw.clock==BIT(31));
    assert(hw.mbc&&hw.mbc<=63&&hw.mbc==hw.mtc&&hw.mbc==hw.bcc);
    assert(hw.fifo_used==hw.mbc);
    if(hw.exchange_stuck) return;
    assert(hw.wire_used+hw.fifo_used<=sizeof(hw.wire));
    memcpy(hw.wire+hw.wire_used,hw.fifo,hw.fifo_used);
    hw.wire_used+=hw.fifo_used;
    hw.us+=(uint32_t)hw.fifo_used*8u;
    if(hw.max_burst<hw.fifo_used) hw.max_burst=(uint32_t)hw.fifo_used;
    hw.fifo_used=0;
    ++hw.bursts;
    hw.tcr&=~BIT(31);
    hw.isr|=BIT(12)|(hw.fifo_error?BIT(8):0);
}
void mercury_test_write8(uintptr_t address,uint8_t value)
{
    assert(address==TXD&&hw.fifo_used<64);
    hw.fifo[hw.fifo_used++]=value;
}
void timer_init(void) {}
int pine64_timer_ready(void) {return hw.timer_ready;}
uint32_t timer_read(void) { hw.us+=10; return UINT32_MAX-hw.us*24u; }
uint32_t timer_elapsed(uint32_t start,uint32_t end) { return start-end; }
void pine64_service(void) { ++hw.service; }
void uart_puts(const char *s) {(void)s;}
void uart_puthex(uint32_t v) {(void)v;}
void uart_putdec(uint32_t v) {(void)v;}

/* Pico peripheral mock: spi_is_readable makes the next received byte
 * available in DR. The actual receiver owns framing/length validation. */
spi_inst_t mock_spi0={0},mock_spi1={1};
static spi_hw_t pico_hw;
static const uint8_t *pico_wire;
static size_t pico_size,pico_read;
static unsigned pico_pins,pico_pulls,pico_format;
unsigned spi_init(spi_inst_t *spi,unsigned baud) {assert(spi==spi0);return baud;}
void spi_set_slave(spi_inst_t *spi,bool slave) {assert(spi==spi0&&slave);}
void spi_set_format(spi_inst_t *spi,unsigned bits,unsigned cpol,unsigned cpha,unsigned order)
{
    assert(spi==spi0&&bits==8&&cpol==0&&cpha==0&&order==0);pico_format=1;
}
void gpio_set_function(unsigned pin,unsigned function) {assert(function==1);pico_pins|=1u<<pin;}
void gpio_pull_up(unsigned pin) {pico_pulls|=1u<<pin;}
bool gpio_get(unsigned pin) {assert(pin==PIN_SPI_CSN);return pico_read==pico_size;}
bool spi_is_readable(spi_inst_t *spi)
{
    assert(spi==spi0);
    if(pico_read==pico_size) return false;
    pico_hw.dr=pico_wire[pico_read++];return true;
}
spi_hw_t *spi_get_hw(spi_inst_t *spi) {assert(spi==spi0);return &pico_hw;}
static int receive(uint8_t *out,const uint8_t *wire,size_t length)
{
    pico_wire=wire;pico_size=length;pico_read=0;
    int n=j32x_spi_recv_displaylist(out,4096);
    assert(pico_read==length);
    return n;
}
static atomic_int run_swap;
static void *scanout(void *arg)
{
    (void)arg;
    while(atomic_load_explicit(&run_swap,memory_order_relaxed)) j32x_swap_apply();
    return NULL;
}
static uint32_t crc32_bytes(const uint8_t *p,size_t count)
{
    uint32_t crc=UINT32_MAX;
    while(count--) {
        crc^=*p++;
        for(unsigned bit=0;bit<8;++bit) crc=(crc>>1)^(0xedb88320u&-(crc&1u));
    }
    return ~crc;
}
static uint32_t send_render(const uint8_t *dl,size_t length)
{
    uint32_t assertions=hw.assertions;
    assert(pine64_mercury_send_displaylist(dl,length)==0);
    assert(hw.assertions==assertions+1&&hw.assertions==hw.releases&&!hw.selected);
    assert(hw.wire_used==length+2&&hw.wire[0]==(uint8_t)length&&hw.wire[1]==(uint8_t)(length>>8));
    assert(!memcmp(hw.wire+2,dl,length));
    uint8_t received[4096];
    assert(receive(received,hw.wire,hw.wire_used)==(int)length);
    assert(!memcmp(received,dl,length));
    j32x_exec_displaylist(received,(uint32_t)length);
    assert(g_state.width==320&&g_state.height==224);
    return crc32_bytes(g_state.fb[g_state.front],J32X_FB_SIZE);
}
int main(void)
{
    memset(&hw,0,sizeof(hw));
    hw.timer_ready=1;hw.pc_cfg=0xa5b6c7d8u;hw.pc_pull=0xa5b6c7d8u;hw.pc_drive=0x5a6b7c8du;
    uint32_t cfg=hw.pc_cfg,pull=hw.pc_pull,drive=hw.pc_drive;
    assert(pine64_mercury_init()==0);
    assert((hw.pc_cfg&0xff0fu)==0x1404u);
    assert((hw.pc_cfg&~0xff0fu)==(cfg&~0xff0fu)); /* PC1/MISO, PC4 untouched */
    assert((hw.pc_pull&~0xf3u)==(pull&~0xf3u));
    assert((hw.pc_drive&~0xf3u)==(drive&~0xf3u));
    j32x_spi_init();
    assert(pico_format&&pico_pins==0xf0u&&pico_pulls==(1u<<5)); /* latest SpotPear preset */
    memset(&g_state,0,sizeof(g_state));g_state.front=0;g_state.back=1;g_state.width=320;g_state.height=224;
    atomic_store(&run_swap,1);pthread_t thread;assert(pthread_create(&thread,NULL,scanout,NULL)==0);
    uint8_t dl[4096];
    size_t length=mercury_scene_bars(dl,sizeof(dl));assert(length==176);
    uint32_t bars=send_render(dl,length);
    static const uint8_t colors[8]={0xe0,0x1c,0x03,0xfc,0xe3,0x1f,0xff,0x00};
    for(unsigned i=0;i<8;++i) {
        assert(g_state.fb[g_state.front][5*320+i*40+20]==colors[i]);
        assert(g_state.fb[g_state.front][220*320+i*40+20]==colors[i]);
    }
    assert(g_state.fb[g_state.front][100*320+160]==0xff);
    assert(g_state.fb[g_state.front][205*320+160]==0);
    uint32_t cube0=0,cube64=0;
    for(unsigned angle=0;angle<256;++angle) {
        length=mercury_scene_cube(dl,sizeof(dl),(uint8_t)angle);assert(length==176);
        uint32_t crc=send_render(dl,length);
        assert(crc!=bars);
        if(angle==0) cube0=crc;
        if(angle==64) cube64=crc;
    }
    assert(cube0!=cube64);
    printf("Existing firmware receiver/parser: bars CRC %08x, dusk cube 0/64 %08x/%08x; all 256 angles passed\n",bars,cube0,cube64);
    /* Maximum receiver payload spans 66 <=63-byte hardware sub-bursts
     * under ONE CS assertion; only complete command uploads are sent. */
    memset(dl,0,sizeof(dl));dl[4095]=0xff;send_render(dl,sizeof(dl));assert(hw.max_burst==63);
    uint32_t before=hw.assertions;
    uint8_t bad[]={0x01,0xe0,0x02};
    assert(pine64_mercury_send_displaylist(bad,sizeof(bad))==PINE64_MERCURY_ERR_DISPLAY_LIST);
    const uint8_t reserved[]={0x06},trailing[]={0xff,0},badres[]={0x20,31,0,224,0};
    assert(pine64_mercury_send_displaylist(reserved,sizeof(reserved))<0);
    assert(pine64_mercury_send_displaylist(trailing,sizeof(trailing))<0);
    assert(pine64_mercury_send_displaylist(badres,sizeof(badres))<0);
    uint8_t oversized[]={0x11,0,0x01,0x10};
    assert(pine64_mercury_send_displaylist(oversized,sizeof(oversized))<0);
    assert(pine64_mercury_send_displaylist(NULL,1)<0);
    assert(pine64_mercury_send_displaylist(dl,4097)<0);
    assert(hw.assertions==before); /* malformed frames cause no electrical transaction */
    uint8_t partial[]={14,0,0x02,0};uint8_t out[4096];
    assert(receive(out,partial,sizeof(partial))<0);
    uint8_t badlength[]={1,16,0xff};assert(receive(out,badlength,sizeof(badlength))<0);
    assert(receive(out,NULL,0)==0);
    /* Local transfer succeeds with no Pico response. The API deliberately
     * reports only local sends, so disconnected hardware cannot fake a
     * received frame or increment a remote-success counter. */
    length=mercury_scene_cube(dl,sizeof(dl),7);
    uint32_t crc_before=crc32_bytes(g_state.fb[g_state.front],J32X_FB_SIZE);
    assert(pine64_mercury_send_displaylist(dl,length)==0);
    assert(crc32_bytes(g_state.fb[g_state.front],J32X_FB_SIZE)==crc_before);
    hw.exchange_stuck=1;
    assert(pine64_mercury_send_displaylist(dl,length)==PINE64_MERCURY_ERR_SPI_TIMEOUT);
    assert(!hw.selected&&hw.assertions==hw.releases);hw.exchange_stuck=0;
    hw.fifo_error=1;
    assert(pine64_mercury_send_displaylist(dl,length)==PINE64_MERCURY_ERR_FIFO);
    assert(!hw.selected);hw.fifo_error=0;
    hw.fifo_stuck=1;
    assert(pine64_mercury_send_displaylist(dl,length)==PINE64_MERCURY_ERR_RESET);
    assert(!hw.selected);hw.fifo_stuck=0;
    hw.ph_cfg=0xa0000000u;hw.ph_pull=0x3a3a3a3au;hw.ph_data=0x12340000u;
    uint32_t ph_cfg=hw.ph_cfg,ph_pull=hw.ph_pull;
    pine64_mercury_bridge_t bridge_status;
    assert(pine64_mercury_probe_bridge(&bridge_status)==PINE64_MERCURY_ERR_I2C);
    assert(bridge_status.idle_lines==3&&bridge_status.address==0&&hw.ph_cfg==ph_cfg&&hw.ph_pull==ph_pull);
    hw.clock_stuck=1;
    assert(pine64_mercury_probe_bridge(&bridge_status)==PINE64_MERCURY_ERR_I2C);
    assert(bridge_status.idle_lines==1&&hw.ph_cfg==ph_cfg&&hw.ph_pull==ph_pull);hw.clock_stuck=0;
    hw.data_stuck=1;
    assert(pine64_mercury_probe_bridge(&bridge_status)==PINE64_MERCURY_ERR_I2C);
    assert(bridge_status.idle_lines==2&&hw.ph_cfg==ph_cfg&&hw.ph_pull==ph_pull);hw.data_stuck=0;
    pine64_mercury_deinit();
    assert(hw.pc_cfg==cfg&&hw.pc_pull==pull&&hw.pc_drive==drive);
    hw.reset_stuck=1;assert(pine64_mercury_init()==PINE64_MERCURY_ERR_RESET);pine64_mercury_deinit();
    hw.reset_stuck=0;hw.timer_ready=0;assert(pine64_mercury_init()==PINE64_MERCURY_ERR_TIMER);
    atomic_store(&run_swap,0);assert(pthread_join(thread,NULL)==0);
    puts("A64 mode/pins/CS framing, 4096-byte list, malformed input, missing reply, bounded faults and absent/stuck bridge tests passed");
    return 0;
}
