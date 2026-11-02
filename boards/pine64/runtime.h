#ifndef PINE64_RUNTIME_H
#define PINE64_RUNTIME_H
#include <stdint.h>
uint32_t pine64_serial_buttons(void);
int pine64_serial_connected(void);
void pine64_status(const char *title, const char *details);
#endif
