#pragma once
#include <stdint.h>
#include <string.h>
class CI2CMaster {
public:
 uint8_t mem[65536]{}; int address=15; int writes=0,reads=0,failWriteAt=-1;bool failRead=false;
 int Write(uint8_t a,const void*p,unsigned n){
  if(a!=address || int(++writes)==failWriteAt)return -1;
  auto b=static_cast<const uint8_t*>(p);if(n<3)return -1;
  unsigned r=(b[0]<<8)|b[1];if(r+n-2>65536)return -1;
  memcpy(mem+r,b+2,n-2);return n;
 }
 int WriteReadRepeatedStart(uint8_t a,const void*p,unsigned n,void*q,unsigned m){
  ++reads;if(a!=address || failRead || n!=2)return -1;
  auto b=static_cast<const uint8_t*>(p);unsigned r=(b[0]<<8)|b[1];
  if(r+m>65536)return -1;memcpy(q,mem+r,m);return m;
 }
};
