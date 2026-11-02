// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/usb/usbkeyboard.h>
#include <circle/spimaster.h>
#include <circle/i2cmaster.h>
#include "capture.h"
#include "touch_input.h"
#include "n64_gpio.h"

enum TShutdownMode {ShutdownNone, ShutdownHalt, ShutdownReboot};
class CKernel {
public:
 CKernel(); ~CKernel(){}; boolean Initialize(); TShutdownMode Run();
private:
 static void KeyPressed(const char *s);
 void PollTouch();
 void PollN64();
 void DrawStatus();
 char SelectedAction() const;
 void DrawControls();
 static void Removed(CDevice*,void*);
 void Print(const char*);
 bool StartCapture(); void StopCapture(); void ShowFrame(); bool SendScene();
 CActLED led; CKernelOptions options; CDeviceNameService names;
 CScreenDevice screen; CSerialDevice serial; CExceptionHandler exceptions;
 CInterruptSystem irq; CTimer timer; CLogger logger; CUSBHCIDevice usb;
 CSPIMaster spi; CI2CMaster i2c; CBridgeCapture capture;
 TouchInput<CI2CMaster> touch;
 N64GPIO n64io; N64Protocol<N64GPIO> *n64; N64Edges n64edges; N64Report n64state;
 unsigned lastTouch,lastN64,n64Failures,n64Replies,selected,lastUI;
 char lastAction; unsigned touchLastGood;
 CUSBKeyboardDevice *keyboard;
 bool capturing,bridgeReady,paused,bars,enabled;
 unsigned angle,frames,transfers,lastFrame,lastStatus;
 volatile char key;
 static CKernel *self;
};
