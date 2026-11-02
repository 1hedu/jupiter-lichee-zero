/* PINE A64+ external sound/controller wiring and observable status.
 * Connector pin numbers and electrical requirements: boards/pine64/EXTERNAL.md
 */
#ifndef PINE64_EXTERNAL_H
#define PINE64_EXTERNAL_H

#include <stdint.h>

#define PINE64_YM_UNINITIALIZED       0
#define PINE64_YM_WRITE_ONLY_READY   1
#define PINE64_YM_CLOCK_FAILED      -1
#define PINE64_YM_CLOCK_UNSUPPORTED -2

/* READY means the GPIO interface and its selected clock configuration are
 * ready. /RD is tied high: the YM3438 itself cannot be detected or read. */
int pine64_ym3438_status(void);
uint32_t pine64_ym3438_clock_hz(void);

/* Configure the external Si5351A (25 MHz crystal) at address 0x60.
 * PH2 = Pi-2 pin 5 SCL; PH3 = Pi-2 pin 3 SDA. Returns 0 on verified
 * register programming + PLL lock, or -1 on NACK, timeout or readback error.
 * This checks the generator, not a clock waveform or the attached YM3438. */
int pine64_si5351_start(void);

#endif
