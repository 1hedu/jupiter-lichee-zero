#pragma once
#define CLOCKHZ 1000000
inline unsigned fakeTicks=0;
class CTimer {public:static unsigned GetClockTicks(){fakeTicks+=1000;return fakeTicks;}static void SimpleusDelay(unsigned){};};
