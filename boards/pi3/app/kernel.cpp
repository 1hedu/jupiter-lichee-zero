// SPDX-License-Identifier: GPL-3.0-or-later
#include "kernel.h"
#include "packet.h"
#include "layout.h"
#include "keyboard_input.h"
#include "tc358743_pi3.h"
#include <camera/camerabuffer.h>
#include <circle/util.h>
#include <circle/string.h>
#include <circle/bcmpropertytags.h>
CKernel *CKernel::self=nullptr;
CKernel::CKernel():screen(0,0),timer(&irq),logger(LogNotice,&timer),
 usb(&irq,&timer,TRUE),spi(1000000,1,1),i2c(0,FALSE,2),capture(&irq),touch(i2c),n64(nullptr),lastTouch(0),lastN64(0),n64Failures(0),n64Replies(0),selected(0),lastUI(0),lastAction('-'),touchLastGood(0),
 keyboard(nullptr),capturing(false),bridgeReady(false),paused(false),bars(false),enabled(true),
 angle(0),frames(0),transfers(0),lastFrame(0),lastStatus(0),key(0) {self=this;led.Blink(3);}
boolean CKernel::Initialize(){
 if(!screen.Initialize() || !serial.Initialize(115200)) return FALSE;
 logger.Initialize(&serial);
 if(!irq.Initialize() || !timer.Initialize()) return FALSE;
 Print("Jupiter Pi 3 M2 - AArch32 bare metal / Circle\n");
 Print("Application baseline: sdk-review-cleanup-ok3job @ 4fb5904e\n");
 Print("Mercury: SPI mode 3, 1 MHz (Pico firmware 448b8ef+)\n");
 if(!spi.Initialize()) {Print("SPI initialization FAILED\n");return FALSE;}
 if(!usb.Initialize()) Print("USB unavailable; use serial controls.\n");
 if(!i2c.Initialize()) Print("CAMERA I2C0 unavailable. SPI remains usable.\n");
 n64io.Initialize();
 n64=new N64Protocol<N64GPIO>(n64io,n64io.Frequency());
 Print("N64 DATA: physical pin 13 / BCM27; 3.3V pull-up required\n");
 tc_bus=&i2c;
 return TRUE;
}
void CKernel::Print(const char*s){screen.Write(s,strlen(s));serial.Write(s,strlen(s));}
void CKernel::KeyPressed(const char*s){char c=mercury_keyboard_key(s);if(c)self->key=c;}
// Reads match the native Raspberry Pi driver: header, then complete records.
// Firmware polling remains disabled, so camera and touch share one ARM owner.
void CKernel::PollTouch(){
 unsigned now=timer.GetTicks();if(now-lastTouch<HZ/50)return;lastTouch=now;
 TouchSample sample;
 if(touch.Poll(sample))touchLastGood=now;
 if(sample.pressed){
  char c=mercury_touch_key(sample.x,sample.y,screen.GetWidth());
  CString log;log.Format("[touch] raw=%u,%u screen=%u,%u action=%c\n",
   sample.rawX,sample.rawY,sample.x,sample.y,c?c:'-');serial.Write(log,log.GetLength());
  if(c){selected=sample.x*4/screen.GetWidth();key=c;}
 }
}
char CKernel::SelectedAction() const {static const char keys[4]={'c','z','r','x'};return keys[selected];}
void CKernel::PollN64(){
 unsigned now=timer.GetTicks();if(now-lastN64<HZ/50)return;lastN64=now;
 N64Report report;
 bool valid=n64 && n64->Poll(report);
 if(valid){++n64Replies;n64Failures=0;n64state=report;}else ++n64Failures;
 uint16_t pressed=n64edges.Feed(valid,report,n64Failures);
 if(pressed & (N64_LEFT|N64_UP)){selected=(selected+3)%4;DrawControls();}
 if(pressed & (N64_RIGHT|N64_DOWN)){selected=(selected+1)%4;DrawControls();}
 if(pressed & N64_A)key=SelectedAction();
 else if(pressed & N64_START)key='z';
 else if(pressed & N64_B)key='x';
 else if(pressed & N64_Z)key='r';
 else if(pressed & (N64_CUP|N64_CDOWN|N64_CLEFT|N64_CRIGHT))key='c';
}
void CKernel::DrawControls(){
 const char *title="\033[1;1HJUPITER / MERCURY M2 - D-pad selects, A activates";screen.Write(title,strlen(title));
 const char *labels[]={bars?"SHOW CUBE":"SHOW BARS",paused?"RESUME":"PAUSE","RETRY CAPTURE",enabled?"STOP":"START"};
 for(unsigned i=0;i<4;i++){
  unsigned x0=i*screen.GetWidth()/4,x1=(i+1)*screen.GetWidth()/4;
  for(unsigned y=32;y<80 && y<screen.GetHeight();y++)for(unsigned x=x0;x<x1;x++)
   screen.SetPixel(x,y, (x==x0 || x+1==x1 || y==32 || y==79)?(i==selected?YELLOW_COLOR:WHITE_COLOR):BLUE_COLOR);
  CString pos;pos.Format("\033[4;%uH%s",1+(x0+8)/8,labels[i]);screen.Write(pos,pos.GetLength());
 }
}
void CKernel::DrawStatus(){
 CString s;unsigned now=timer.GetTicks();
 s.Format("\033[6;1H\033[KTouch:%s %u (%u,%u) N64:%s [%u/%u] CSI:%u Act:%c",
  touch.reads && now-touchLastGood<HZ?"OK":"ERR",touch.presses,touch.last.x,touch.last.y,
  n64edges.connected?"OK":"---",n64?n64->bits:0,n64?unsigned(n64->error):0,frames,lastAction);
 screen.Write(s,s.GetLength());
}
void CKernel::Removed(CDevice*,void*){self->keyboard=nullptr;}
bool CKernel::SendScene(){
 u8 packet[514]; size_t n=mercury_packet(packet,sizeof packet,bars,angle++);
 if(!n) return false;
 int sent=spi.Write(0,packet,n);
 if(sent!=int(n)){Print("SPI local transfer FAILED\n");return false;}
 ++transfers;return true; // Protocol has no remote acknowledgement.
}
bool CKernel::StartCapture(){
 if(capturing) return true;
 Print("Bridge: probing CAMERA I2C0, addresses 0x0f/0x1f...\n");
 CBcmPropertyTags tags; TPropertyTagGPIOState power;
 power.nGPIO=133;power.nState=1;
 if(!tags.GetTag(PROPTAG_SET_SET_GPIO_STATE,&power,sizeof power,8)){
  Print("CAMERA power-enable failed; cannot probe bridge.\n");return false;
 }
 timer.MsDelay(20);
 int r=tc358743_init();
 if(r){CString s;s.Format("Bridge initialization FAILED (%d); SPI-only operation. R retries.\n",r);Print(s);return false;}
 bridgeReady=true;
 // Initialize receiver once. Later retries reuse IRQ/power and buffers.
 static bool rxInitialized=false;
 if(!rxInitialized){
  if(!capture.Initialize() || !capture.SetFormat(640,480,8) || !capture.AllocateBuffers(3)){
   Print("CSI receiver setup FAILED\n");return false;
  }
  rxInitialized=true;
 }
 tc_io_error=0;
 if(!capture.Start()) {Print("CSI start FAILED\n");return false;}
 tc358743_stream_on();
 if(tc_io_error){tc358743_stream_off();capture.Stop();Print("Bridge stream write FAILED\n");return false;}
 capturing=true;lastFrame=timer.GetTicks();capture.FlushBuffers();
 Print("CSI armed: RGB888 / 640x480 / 2 lanes. Waiting for real frame...\n");
 return true;
}
void CKernel::StopCapture(){if(capturing){tc358743_stream_off();capture.Stop();capture.FlushBuffers();capturing=false;}}
void CKernel::ShowFrame(){
 CCameraBuffer*b=capture.GetNextBuffer();if(!b)return;
 const u8*p=static_cast<const u8*>(b->GetPtr());
 // CSI-2 RGB888 payload order is B,G,R; do not invoke Bayer conversion.
 const auto fmt=capture.GetFormatInfo();
 const auto view=mercury_view(screen.GetWidth(),screen.GetHeight());
 for(unsigned y=0;y<view.h;y++){
  const u8*row=p+(y*480/view.h)*fmt.BytesPerLine;
  for(unsigned x=0;x<view.w;x++){
   unsigned sx=x*640/view.w;
   screen.SetPixel(x+view.x,y+view.y,COLOR16(row[3*sx+2]>>3,row[3*sx+1]>>3,row[3*sx]>>3));
  }
 }
 capture.BufferProcessed();++frames;lastFrame=timer.GetTicks();
 if(frames==1){const char *msg="[pi3/mercury] FIRST CSI FRAME RECEIVED\n";serial.Write(msg,strlen(msg));}
}
TShutdownMode CKernel::Run(){
 Print("Arrows select | Enter activates | C scene | Z pause | R retry | X stop/start\n");
 Print("SPI counts local writes only. Capture is experimental until board-tested.\n");
 StartCapture();screen.Write("\033[H\033[J",6);DrawControls();DrawStatus(); unsigned lastSend=timer.GetTicks();
 for(;;){
  usb.UpdatePlugAndPlay();
  PollTouch();
  PollN64();
  if(!keyboard){keyboard=static_cast<CUSBKeyboardDevice*>(names.GetDevice("ukbd1",FALSE));
   if(keyboard){keyboard->RegisterKeyPressedHandler(KeyPressed);keyboard->RegisterRemovedHandler(Removed);}}
  char c=key;key=0;char sc;
  if(serial.Read(&sc,1)==1)c=sc;
  if(c==1){selected=(selected+3)%4;DrawControls();c=0;}
  if(c==2){selected=(selected+1)%4;DrawControls();c=0;}
  if(c=='\n'||c=='\r')c=SelectedAction();
  if(c)lastAction=c;
  if(c=='c'||c=='C'){bars=!bars;SendScene();}
  if(c=='\n'||c=='\r'||c=='z'||c=='Z')paused=!paused;
  if(c=='x'||c=='X'){enabled=!enabled;if(!enabled)StopCapture();else StartCapture();DrawControls();}
  if(c=='r'||c=='R'){enabled=true;StopCapture();StartCapture();DrawControls();}
  if(c){DrawControls();DrawStatus();}
  unsigned now=timer.GetTicks();
  if(enabled && !paused && now-lastSend>=HZ/30){SendScene();lastSend=now;}
  if(capturing)ShowFrame();
  if(now-lastUI>=HZ/10){DrawStatus();lastUI=now;}
  if(now-lastStatus>=HZ*2){
   CString s;s.Format("[pi3/mercury] local SPI=%u CSI frames=%u RX errors=%08x dropped=%u%s\n",transfers,frames,capture.GetReceiveErrors(),capture.GetDroppedFrames(),
     capturing && now-lastFrame>HZ*2?" NO RECENT FRAME":"");serial.Write(s,s.GetLength());
   if(bridgeReady){tc_io_error=0;auto d=tc358743_diag();
    s.Format("[bridge] %ux%u sys=%02x csi=%04x err=%08x i2c=%s\n",d.width,d.height,
      tc358743_status(),d.csi_status,d.csi_err,tc_io_error?"FAIL":"OK");serial.Write(s,s.GetLength());}
   s.Format("[input] touch reads=%u errors=%u last-error=%d presses=%u; N64 replies=%u failures=%u bits=%u error=%u buttons=%04x\n",
    touch.reads,touch.errors,touch.lastError,touch.presses,n64Replies,n64Failures,n64?n64->bits:0,n64?unsigned(n64->error):0,n64state.buttons);serial.Write(s,s.GetLength());
   lastStatus=now;if((now/(HZ*2))&1)led.On();else led.Off();
  }
  timer.usDelay(1000);
 }
}
