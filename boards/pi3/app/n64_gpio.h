// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <circle/gpiopin.h>
#include <circle/memio.h>
#include <circle/bcm2835.h>
#include <circle/synchronize.h>
#include "n64_protocol.h"
class N64GPIO {
 CGPIOPin pin{27,GPIOModeInputPullUp};
 uint32_t direction=0;
 static constexpr uint32_t mask=1u<<27;
 static constexpr uintptr fsel=ARM_GPIO_BASE+8; // GPIO20..29; GPIO27 field [23:21]
public:
 static constexpr unsigned Pin=27; // J8 physical pin 13
 void Initialize(){pin.SetMode(GPIOModeInputPullUp);}
 uint32_t Frequency(){uint32_t f;asm volatile("mrc p15, 0, %0, c14, c0, 0":"=r"(f));return f;}
 static inline uint32_t Now(){uint32_t lo,hi;asm volatile("mrrc p15, 0, %0, %1, c14":"=r"(lo),"=r"(hi));return lo;}
 void Begin(){EnterCritical(FIQ_LEVEL);direction=read32(fsel)&~(7u<<21);write32(ARM_GPIO_GPCLR0,mask);DataSyncBarrier();}
 void End(){LeaveCritical();}
 inline void Low(){write32(fsel,direction|(1u<<21));DataSyncBarrier();}
 inline void Release(){write32(fsel,direction);DataSyncBarrier();}
 inline bool High(){return (read32(ARM_GPIO_GPLEV0)&mask)!=0;}
};
