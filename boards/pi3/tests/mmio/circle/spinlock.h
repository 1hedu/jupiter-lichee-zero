#pragma once
#include <cassert>
#define TASK_LEVEL 0
inline int held=0;
class CSpinLock {public:CSpinLock(int){}void Acquire(){assert(!held);held=1;}void Release(){assert(held);held=0;}};
