#include "../app/touch_input.h"
#include "../app/n64_protocol.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <initializer_list>
struct Bus {
 unsigned count=0,x=700,y=430;int fault=0;unsigned calls=0;
 int WriteReadRepeatedStart(unsigned a,const uint8_t*r,unsigned w,uint8_t*d,unsigned n){
  assert(a==0x38&&w==1);++calls;if(fault)return fault;
  memset(d,0,n);if(*r==0){assert(n==3);d[2]=count;}
  else {assert(*r==3&&n==6*count);d[0]=x>>8;d[1]=x;d[2]=y>>8;d[3]=y;}
  return n;
 }
};
struct IO {
 uint32_t t=0,start=0,fall=0;unsigned pulses=0;bool low=false,active=false,stuck=false,absent=false;unsigned limit=32;
 uint32_t data=0x8100fe7f;unsigned begins=0,ends=0;
 void Begin(){++begins;}void End(){++ends;}
 uint32_t Now(){return t++;}
 void Low(){low=true;fall=t++;}
 void Release(){if(low){unsigned duration=t-fall;assert(duration>=20&&duration<=63);low=false;if(++pulses==9){start=t+40;active=true;}}++t;}
 bool High(){++t;if(stuck||low)return false;if(!active||absent)return true;
  uint32_t elapsed=t-start;if(elapsed>=limit*80)return true;
  unsigned bit=elapsed/80;unsigned width=(data&(1u<<(31-bit)))?20:60;
  return elapsed%80>=width;
 }
};
int main(){
 Bus b;TouchInput<Bus> touch(b);TouchSample s;
 assert(touch.Poll(s)&&!s.pressed);b.count=1;
 assert(touch.Poll(s)&&s.pressed&&s.x==99&&s.y==49);
 assert(touch.Poll(s)&&!s.pressed);b.fault=-1;assert(!touch.Poll(s)&&!s.pressed);
 b.fault=0;assert(touch.Poll(s)&&!s.pressed);b.count=0;assert(touch.Poll(s));b.count=1;assert(touch.Poll(s)&&s.pressed);
 Bus stale;stale.count=1;TouchInput<Bus> ts(stale);assert(ts.Poll(s)&&!s.pressed);stale.count=0;assert(ts.Poll(s));stale.count=1;assert(ts.Poll(s)&&s.pressed);
 b.count=11;assert(!touch.Poll(s));b.count=1;b.x=900;assert(touch.Poll(s)&&!s.pressed);
 for(uint32_t pattern: {0u,0xffffffffu,0x8100fe7fu,0x5555aaaau}){
  IO io;io.data=pattern;io.t=0xfffffff0;N64Protocol<IO> protocol(io,20000000);N64Report r;
  assert(protocol.Poll(r));assert(r.buttons==pattern>>16&&uint8_t(r.x)==((pattern>>8)&255)&&uint8_t(r.y)==(pattern&255));
  assert(io.pulses==9&&io.begins==1&&io.ends==1&&!io.low);
 }
 for(unsigned mode=0;mode<3;mode++){
  IO io;io.stuck=mode==0;io.absent=mode==1;io.limit=mode==2?12:32;
  N64Protocol<IO> p(io,20000000);N64Report r;r.buttons=0x1234;
  assert(!p.Poll(r)&&r.buttons==0x1234&&io.ends==1&&!io.low);
 }
 N64Edges e;N64Report r;r.buttons=N64_A;assert(!e.Feed(true,r,0));assert(!e.Feed(true,r,0)&&e.connected);
 r.buttons=0;assert(!e.Feed(true,r,0));r.buttons=N64_A;assert(e.Feed(true,r,0)==N64_A);assert(!e.Feed(true,r,0));
 assert(!e.Feed(false,r,6)&&!e.connected);assert(!e.Feed(true,r,0));assert(!e.Feed(true,r,0));
 puts("PASS: touch sequence/orientation/edges/faults; N64 waveforms/wrap/timeout/reconnection");
}
