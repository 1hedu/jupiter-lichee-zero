#pragma once
#define CLOCK_ID_CORE 4
class CMachineInfo {public:static CMachineInfo*Get(){static CMachineInfo m;return &m;}unsigned GetClockRate(unsigned){return 250000000;}};
