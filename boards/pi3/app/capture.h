// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <camera/csi2cameradevice.h>
class CBridgeCapture : public CCSI2CameraDevice {
public:
 explicit CBridgeCapture(CInterruptSystem *irq):CCSI2CameraDevice(irq),active(false) {}
 ~CBridgeCapture(){if(active) Stop();}
 bool Probe() override {return true;}
 bool Start(bool = true) override {active=EnableRX();return active;}
 void Stop() override {if(active){DisableRX();active=false;}}
 bool IsControlSupported(TControl) const override {return false;}
 int GetControlValue(TControl) const override {return 0;}
 bool SetControlValue(TControl,int) override {return false;}
 bool SetControlValuePercent(TControl,unsigned) override {return false;}
 CCameraControl::TControlInfo GetControlInfo(TControl) const override {return {};}
protected:
 bool SetMode(unsigned *w,unsigned *h,unsigned d) override {return *w==640 && *h==480 && d==8;}
 TFormatCode GetPhysicalFormat() const override {return FormatRGB888;}
 TFormatCode GetLogicalFormat() const override {return FormatRGB888;}
 const TRect GetCropInfo() const override {return {0,0,640,480};}
private: bool active;
};
