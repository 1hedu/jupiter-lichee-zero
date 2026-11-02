#pragma once
#include <cassert>
inline int peripheral=0;
#define PeripheralEntry() (++peripheral)
#define PeripheralExit() do {assert(peripheral>0);--peripheral;} while(0)
