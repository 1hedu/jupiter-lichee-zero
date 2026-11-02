#include <circle/i2cmaster.h>
#include <circle/bcm2835.h>
#include <circle/synchronize.h>
#include <circle/timer.h>
#include <cassert>
#include <cstdio>
static int mode=0;static unsigned fifoReads=0;
u32 read32(uintptr a){
 unsigned off=a-(ARM_IO_BASE+0x205000);
 if(off==ARM_BSC_S__OFFSET){
  if(mode==0)return 0;
  if(mode==1)return (1<<1)|(1<<0)|(1<<5);
  if(mode==2)return (1<<0)|(1<<5);
  if(mode==3)return (1<<1)|(1<<8);
 }
 if(off==ARM_BSC_FIFO__OFFSET){++fifoReads;return 0xa5;}return 0;
}
void write32(uintptr,u32){}
int main(){
 CI2CMaster bus(0,FALSE,2);assert(bus.Initialize());u8 b[4]={},reg=2;
 assert(bus.Read(15,b,4)==-I2C_MASTER_TIMEOUT);assert(!held&&!peripheral);
 assert(bus.Write(15,b,4)==-I2C_MASTER_TIMEOUT);assert(!held&&!peripheral);
 assert(bus.WriteReadRepeatedStart(15,&reg,1,b,4)==-I2C_MASTER_TIMEOUT);assert(!held&&!peripheral);
 mode=2;fifoReads=0;assert(bus.Read(15,b,4)==-I2C_MASTER_TIMEOUT);assert(fifoReads==4);
 fifoReads=0;assert(bus.WriteReadRepeatedStart(15,&reg,1,b,4)==-I2C_MASTER_TIMEOUT);assert(fifoReads==4);
 mode=1;assert(bus.Read(15,b,4)==4);for(auto x:b)assert(x==0xa5);
 assert(bus.Write(15,b,4)==4);assert(bus.WriteReadRepeatedStart(15,&reg,1,b,4)==4);
 mode=3;assert(bus.Read(15,b,4)==-I2C_MASTER_ERROR_NACK);
 mode=0;fakeTicks=0xffffe000;assert(bus.Write(15,b,4)==-I2C_MASTER_TIMEOUT);
 assert(!held&&!peripheral);
 puts("PASS: actual Circle I2C source, stuck controller, bounded FIFO, timeout recovery, NACK and clock wrap");
}
