// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scenes.h"
static size_t mercury_packet(uint8_t *out,size_t capacity,bool bars,uint8_t angle){
 if(!out || capacity<2)return 0;
 size_t n=bars?mercury_scene_bars(out+2,capacity-2):mercury_scene_cube(out+2,capacity-2,angle);
 if(!n || n>65535)return 0;
 out[0]=(uint8_t)n;out[1]=(uint8_t)(n>>8);return n+2;
}
