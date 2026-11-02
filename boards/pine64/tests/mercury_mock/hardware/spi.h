#ifndef MERCURY_TEST_SPI_H
#define MERCURY_TEST_SPI_H
#include <stdbool.h>
#include "hardware/structs/spi.h"
typedef struct { unsigned index; } spi_inst_t;
extern spi_inst_t mock_spi0, mock_spi1;
#define spi0 (&mock_spi0)
#define spi1 (&mock_spi1)
#define SPI_CPOL_0 0u
#define SPI_CPHA_0 0u
#define SPI_CPOL_1 1u
#define SPI_CPHA_1 1u
#define SPI_MSB_FIRST 0u
unsigned spi_init(spi_inst_t *spi, unsigned baud);
void spi_set_slave(spi_inst_t *spi, bool slave);
void spi_set_format(spi_inst_t *spi, unsigned bits, unsigned cpol, unsigned cpha, unsigned order);
bool spi_is_readable(spi_inst_t *spi);
spi_hw_t *spi_get_hw(spi_inst_t *spi);
#endif
