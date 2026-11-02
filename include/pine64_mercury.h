#ifndef JUPITER_PINE64_MERCURY_H
#define JUPITER_PINE64_MERCURY_H

#include <stdint.h>
#include <stddef.h>

/* Wire contract of claude/sdk-review-cleanup-ok3job (448b8ef+): mode 3,
 * CS low, u16 little-endian byte length, then the display list, CS high.
 * There is no acknowledgement, READY signal or framebuffer readback. */
#define PINE64_MERCURY_SPI_HZ 1000000u
#define PINE64_MERCURY_MAX_LIST 4096u
#define PINE64_MERCURY_SELECT_US 25000u
#define PINE64_MERCURY_WIDTH 320u
#define PINE64_MERCURY_HEIGHT 224u

enum pine64_mercury_error {
    PINE64_MERCURY_OK = 0,
    PINE64_MERCURY_ERR_ARGUMENT = -1,
    PINE64_MERCURY_ERR_TIMER = -2,
    PINE64_MERCURY_ERR_RESET = -3,
    PINE64_MERCURY_ERR_SPI_TIMEOUT = -4,
    PINE64_MERCURY_ERR_FIFO = -5,
    PINE64_MERCURY_ERR_DISPLAY_LIST = -6,
    PINE64_MERCURY_ERR_NOT_READY = -7,
    PINE64_MERCURY_ERR_I2C = -8,
    PINE64_MERCURY_ERR_BRIDGE_ID = -9
};

typedef struct {
    uint32_t spi_hz, lists_sent, bytes_sent, timeouts, fifo_errors;
    uint32_t last_spi_control, last_spi_fifo, last_spi_status;
    int32_t last_error;
} pine64_mercury_stats_t;

typedef struct {
    uint16_t chip_id, width, height;
    uint8_t address, status, idle_lines;
    int32_t error;
} pine64_mercury_bridge_t;

/* Init tests the local controller, not whether a Pico is attached. Only
 * PC0 MOSI, PC2 SCK and PC3 CS are claimed. PC1/MISO and PC4 are untouched. */
int pine64_mercury_init(void);
void pine64_mercury_deinit(void);
/* Validate every command before selecting the slave. Complete scenes end
 * with 0xff; command-only uploads (e.g. SET_RES) are also wire-compatible.
 * Zero means clocks completed locally, NOT that the Pico accepted a frame. */
int pine64_mercury_validate_displaylist(const uint8_t *list, size_t length);
int pine64_mercury_send_displaylist(const uint8_t *list, size_t length);
const pine64_mercury_stats_t *pine64_mercury_stats(void);
const char *pine64_mercury_error_string(int error);
void pine64_mercury_dump_diagnostics(void);

/* Optional, bounded read-only TC358743 check on PI-2 pin 5 PH2/SCL and
 * pin 3 PH3/SDA, 3.3 V open-drain. Reads ID, HDMI status and active size;
 * never resets/configures the bridge or enables its CSI-2 transmitter.
 * Pin configuration is restored before return. A64 has parallel CSI,
 * so HDMI lock cannot be reported as a captured frame on this board. */
int pine64_mercury_probe_bridge(pine64_mercury_bridge_t *bridge);

#endif
