#pragma once
#include <circle/types.h>
#define GPIO_PINS 54
enum {GPIOModeInput,GPIOModeAlternateFunction0,GPIOModeAlternateFunction1,GPIOModeAlternateFunction5,GPIOPullModeUp};
class CGPIOPin {public:void AssignPin(unsigned){}void SetMode(unsigned){}void SetPullMode(unsigned){};};
