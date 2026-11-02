#include <cassert>
#include <cstdio>
#include <initializer_list>
#include "../app/layout.h"
int main(){
 auto v=mercury_view(800,480);assert(v.x==144 && v.y==96 && v.w==512 && v.h==384);
 for(unsigned w: {0u,100u,640u,720u,800u,1920u})for(unsigned h:{0u,64u,96u,480u,600u,1280u}){
  v=mercury_view(w,h);assert(v.x+v.w<=w && v.y+v.h<=h);
  if(v.w&&v.h)assert(v.y>=96);
 }
 assert(mercury_touch_key(0,32,800)=='c');assert(mercury_touch_key(200,40,800)=='z');
 assert(mercury_touch_key(400,79,800)=='r');assert(mercury_touch_key(799,79,800)=='x');
 assert(!mercury_touch_key(800,79,800));assert(!mercury_touch_key(0,80,800));assert(!mercury_touch_key(0,0,0));
 puts("PASS: 800x480 full-frame fitting, display bounds and four touch regions");
}
