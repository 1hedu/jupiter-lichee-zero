#ifndef PINE64_YM3438_DEMO_SETUP_H
#define PINE64_YM3438_DEMO_SETUP_H
#include "jupiter.h"
#include "pine64_external.h"
#include "pine64_video.h"

/* Run before audio/synth initialization so an absent clock generator
 * returns normally without leaving a started producer or DMA channel. */
static inline int pine64_ym3438_demo_setup(int source)
{
    ym3438_hw_set_clock(source);
    ym3438_hw_init();
    if (pine64_ym3438_status() == PINE64_YM_WRITE_ONLY_READY)
        return 1;
    pine64_notice("YM3438 CLOCK UNAVAILABLE",
                  "Needs Si5351A with 25 MHz crystal.\n"
                  "Pi-2 pin 3 PH3 SDA; pin 5 PH2 SCL.\n"
                  "Use 3.3 V I2C pull-ups and common GND.\n"
                  "See EXTERNAL.md for the YM3438 adapter.");
    return 0;
}
#endif
