#include <cassert>
#include <cstdio>
#include "../app/packet.h"
extern "C" {
#include "jupiter32x.h"

}
int main(){
 uint8_t p[514];assert(!mercury_packet(nullptr,514,false,0));assert(!mercury_packet(p,1,false,0));
 assert(!mercury_packet(p,100,false,0));g_state.width=320;g_state.height=224;g_state.back=1;
 for(unsigned a=0;a<256;a++)for(int bars=0;bars<2;bars++){
  size_t n=mercury_packet(p,sizeof p,bars,a);assert(n>=2 && n<=sizeof p);
  assert((p[0]|(p[1]<<8))==n-2);assert(p[n-1]==0xff);
  j32x_exec_displaylist(p+2,n-3); // Skip blocking SCENE_END swap (no scanout core on host).
  if(bars){assert(g_state.fb[1][10]==0xe0);assert(g_state.fb[1][160+100*320]==0xff);}
 }
 puts("PASS: production packets, little-endian framing, all cube angles and bars through unchanged Mercury renderer");
}
