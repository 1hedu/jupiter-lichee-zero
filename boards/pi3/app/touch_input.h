// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
// Original Raspberry Pi display native FT5x06 protocol, following
// raspberrypi/linux rpi-6.6.y edt-ft5x06.c and edt-ft5406.dtsi.
// Header (register 0, 3 bytes) and contact records (register 3) are separate reads.
// Native coordinates invert both axes for the standard 800x480 panel orientation.
struct TouchSample { bool pressed=false; unsigned x=0,y=0,rawX=0,rawY=0; };
template<class Bus> class TouchInput {
 Bus &bus;
 uint16_t ids=0;
 int initial=-1;
 bool settled=false;
public:
 unsigned reads=0,errors=0,presses=0;
 int lastError=0;
 TouchSample last;
 explicit TouchInput(Bus &b):bus(b){}
 bool ReadReg(uint8_t reg,uint8_t *data,unsigned n){
  int r=bus.WriteReadRepeatedStart(0x38,&reg,1,data,n);
  if(r!=int(n)){++errors;lastError=r<0?r:-7;return false;}
  return true;
 }
 bool Poll(TouchSample &out){
  out=TouchSample{};
  uint8_t head[3]={};
  if(!ReadReg(0,head,3))return false;
  unsigned count=head[2]&15;
  if(count>10 || (head[0]&0x70)){++errors;lastError=-8;return false;}
  ++reads;lastError=0;
  if(!settled){
   if(initial<0)initial=head[2];
   if(count && head[2]==initial)return true;
   settled=true;
  }
  if(!count){ids=0;return true;}
  uint8_t points[60]={};
  if(!ReadReg(3,points,6*count))return false;
  uint16_t active=0;
  for(unsigned i=0;i<count;i++){
   const uint8_t *p=points+6*i;unsigned type=p[0]>>6,id=p[2]>>4;
   unsigned x=((p[0]&15)<<8)|p[1],y=((p[2]&15)<<8)|p[3];
   if(id>=10 || type==3 || type==1 || x>=800 || y>=480)continue;
   active|=uint16_t(1u<<id);
   last.x=799-x;last.y=479-y;last.rawX=x;last.rawY=y;
   if(!(ids&(1u<<id)) && !out.pressed){out=last;out.pressed=true;++presses;}
  }
  ids=active;
  return true;
 }
};
