#ifndef MERCURY_TEST_PICO_STDLIB_H
#define MERCURY_TEST_PICO_STDLIB_H
#include <stdbool.h>
#include <stdint.h>
typedef unsigned uint;
#define GPIO_FUNC_SPI 1u
void gpio_set_function(uint pin, uint function);
void gpio_pull_up(uint pin);
bool gpio_get(uint pin);
#endif
