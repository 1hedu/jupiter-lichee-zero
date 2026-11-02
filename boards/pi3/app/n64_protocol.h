// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
// Hardware-independent transaction engine, instantiated with Pi GPIO/counter IO.
// The pin is only pulled LOW or released to input (external 3.3-V pull-up).
enum N64Error { N64OK=0,N64NotReady,N64StuckLow,N64NoReply,N64Truncated,N64BadPulse };
struct N64Report {uint16_t buttons=0;int8_t x=0,y=0;};
enum {N64_A=0x8000,N64_B=0x4000,N64_Z=0x2000,N64_START=0x1000,
 N64_UP=0x0800,N64_DOWN=0x0400,N64_LEFT=0x0200,N64_RIGHT=0x0100,
 N64_L=0x0020,N64_R=0x0010,N64_CUP=8,N64_CDOWN=4,N64_CLEFT=2,N64_CRIGHT=1};
template<class IO> class N64Protocol {
 IO &io;uint32_t hz;
 uint32_t Ticks(unsigned ns)const{return uint32_t((uint64_t(hz)*ns+500000000)/1000000000);}
 void Wait(uint32_t start,uint32_t ticks){while(uint32_t(io.Now()-start)<ticks){}}
 bool Level(bool high,uint32_t maxTicks){uint32_t start=io.Now();
  while(io.High()!=high)if(uint32_t(io.Now()-start)>=maxTicks)return false;
  return true;
 }
public:
 unsigned bits=0;N64Error error=N64NotReady;
 N64Protocol(IO &h,uint32_t frequency):io(h),hz(frequency){}
 bool Poll(N64Report &out){
  bits=0;error=N64NotReady;if(hz<1000000 || hz>100000000)return false;
  const uint32_t one=Ticks(1000),three=Ticks(3000),cell=Ticks(4000);
  struct Guard {IO &io;Guard(IO&i):io(i){io.Begin();}~Guard(){io.Release();io.End();}} guard(io);
  io.Release();
  if(!Level(true,Ticks(10000))){error=N64StuckLow;return false;}
  // status command 0x01, MSB first
  for(unsigned bit=0;bit<8;bit++){
   io.Low();uint32_t start=io.Now();Wait(start,bit==7?one:three);
   io.Release();Wait(start,cell);
  }
  io.Low();uint32_t stop=io.Now();Wait(stop,one);io.Release();
  // Arm immediately, rather than delaying into the first reply bit.
  if(!Level(true,Ticks(6000))){error=N64StuckLow;return false;}
  uint8_t raw[4]={};
  for(unsigned bit=0;bit<32;bit++){
   if(!Level(false,Ticks(bit?12000:80000))){error=bit?N64Truncated:N64NoReply;return false;}
   uint32_t edge=io.Now();
   if(!Level(true,Ticks(5000))){error=N64BadPulse;return false;}
   uint32_t low=io.Now()-edge;
   if(low<Ticks(400) || low>Ticks(4200)){error=N64BadPulse;return false;}
   raw[bit/8]=uint8_t((raw[bit/8]<<1)|(low<Ticks(2000)));++bits;
  }
  out.buttons=uint16_t((raw[0]<<8)|raw[1]);out.x=int8_t(raw[2]);out.y=int8_t(raw[3]);
  error=N64OK;return true;
 }
};
// Establish a baseline before reporting edges. Failed/partial replies never
// publish buttons; a disconnected controller is re-baselined on reconnection.
class N64Edges {
 uint16_t previous=0;unsigned stable=0;
public:
 bool connected=false;
 uint16_t Feed(bool valid,const N64Report&r,unsigned failures){
  if(!valid){if(failures>=6){connected=false;stable=0;}return 0;}
  if(stable<2){previous=r.buttons;++stable;connected=stable==2;return 0;}
  uint16_t pressed=r.buttons&~previous;previous=r.buttons;connected=true;return pressed;
 }
};
