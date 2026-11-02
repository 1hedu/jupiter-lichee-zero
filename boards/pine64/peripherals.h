#ifndef JUPITER_PINE64_PERIPHERALS_H
#define JUPITER_PINE64_PERIPHERALS_H
#include <stdint.h>

/* Board-specific diagnostics; sdmmc.h keeps the shared Jupiter block API. */
enum pine64_sd_error {
    PINE64_SD_ERR_ARGUMENT   = -1,
    PINE64_SD_ERR_NOT_READY  = -2,
    PINE64_SD_ERR_TIMEOUT    = -3,
    PINE64_SD_ERR_CONTROLLER = -4,
    PINE64_SD_ERR_CARD       = -5,
    PINE64_SD_ERR_TIMER      = -6,
    PINE64_SD_ERR_NO_CARD    = -7,
    PINE64_SD_ERR_VOLTAGE    = -8,
    PINE64_SD_ERR_CAPACITY   = -9
};

int pine64_timer_ready(void);
extern uint32_t pine64_boot_fdt, pine64_boot_cpsr, pine64_boot_sctlr;
void pine64_boot_report(void);

#endif
