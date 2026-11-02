#include <cassert>
#include <cstdio>
#include "../app/tc358743_pi3.h"
int main(){
 CI2CMaster bus;tc_bus=&bus;
 assert(tc358743_init()==0); assert(tc_i2c_addr==15);
 assert(bus.mem[TC_FIFOCTL]==(374&255));
 assert((tc_rd16(TC_CONFCTL)&TC_MASK_VBUFEN)==0);
 tc358743_stream_on();assert(tc_rd16(TC_CONFCTL)&TC_MASK_VBUFEN);
 tc358743_stream_off();assert(!(tc_rd16(TC_CONFCTL)&TC_MASK_VBUFEN));
 bus=CI2CMaster{};bus.address=31;assert(tc358743_init()==0);assert(tc_i2c_addr==31);assert(!tc_io_error);
 bus=CI2CMaster{};bus.address=33;assert(tc358743_init()==TC_ERR_NOACK);
 bus=CI2CMaster{};bus.mem[1]=1;assert(tc358743_init()==TC_ERR_ID);
 bus=CI2CMaster{};bus.failWriteAt=10;assert(tc358743_init()==TC_ERR_VERIFY);
 bus=CI2CMaster{};bus.failRead=true;assert(tc358743_init()==TC_ERR_NOACK);
 puts("PASS: bridge probe, alternate address, stream sequencing, absent bridge, bad ID, read/write faults");
}
