#pragma once
class CTimer {public:static CTimer*Get(){static CTimer t;return &t;}void usDelay(unsigned){};};
