/*
 * Jupiter SDK — Nuked-SC55 LCD stub
 *
 * mcu.cpp's MCU_Read/MCU_Write tables hand off LCD-mapped memory accesses
 * to LCD_Write / LCD_Enable. Upstream lcd.cpp is the SC-55's front-panel
 * LCD display, which we don't render anywhere. Stub the symbols so the
 * link succeeds; the LCD writes are silently dropped.
 *
 * Function signatures match third_party/nuked_sc55/src/lcd.h exactly so
 * the C++-mangled symbols line up.
 */
#include <stdint.h>
#include <string>

void LCD_Init(void)                              {}
void LCD_UnInit(void)                            {}
void LCD_Write(uint32_t address, uint8_t data)   { (void)address; (void)data; }
void LCD_Enable(uint32_t enable)                 { (void)enable; }
bool LCD_QuitRequested()                         { return false; }
void LCD_Update(void)                            {}
void LCD_SetBackPath(const std::string &path)    { (void)path; }
