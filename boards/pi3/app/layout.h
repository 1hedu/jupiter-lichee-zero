// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
struct MercuryView {unsigned x,y,w,h;};
static inline MercuryView mercury_view(unsigned w,unsigned h){
 if(!w || h<=96)return {0,0,0,0};
 unsigned vw=w,vh=(unsigned)((unsigned long long)vw*480/640);
 if(vh>h-96){vh=h-96;vw=(unsigned)((unsigned long long)vh*640/480);}
 if(!vw || !vh)return {0,0,0,0};
 return {(w-vw)/2,96+(h-96-vh)/2,vw,vh};
}
static inline char mercury_touch_key(unsigned x,unsigned y,unsigned width){
 if(y<32 || y>=80 || !width || x>=width)return 0;
 const char keys[4]={'c','z','r','x'};return keys[(unsigned long long)x*4/width];
}
