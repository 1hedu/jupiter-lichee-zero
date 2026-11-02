JUPITER PI 3 — MERCURY M2
=======================
First bare-metal Raspberry Pi 3 B/B+ Mercury target. No Linux.
This is a hardware bring-up image, not the full 46-demo menu port.

IN THIS REPO (boards/pi3/)
Imported from the 2026-10-17 M2 handoff. Hardware result: Pi 3 + Pico +
HDMI -> TC358743 -> Pi CSI -> original 7-inch DSI panel confirmed working
by the user after reversing a backwards CSI ribbon.
Changed since the M2 image: SPI is now MODE 3 (CSPIMaster(1 MHz, 1, 1)) to
match the Pico firmware from commit 448b8ef. Mode 0 never delivered a
display list (the Pico's PL022 accepts one byte per CS assertion in mode 0),
so the Pico kept showing its own self-test cube and the C bars/cube toggle
appeared to do nothing. The M2 image (mode 0) and the new Pico firmware do
not work together: flash both, or neither.
Vendored code is not committed: ./fetch_vendor.sh clones Circle and Circle
libcamera at the pinned commits in SOURCE-BASELINES.txt, applies
patches/*.patch, and downloads the SHA256-checked Pi boot firmware.
The Mercury renderer used by tests/ is this repo's mercury/ directly.

INSTALL
1. Extract jupiter-pi3-mercury-m2.img and flash the WHOLE image to an SD card
   using Raspberry Pi Imager (Use custom) or Etcher. Flashing erases the card.
2. Connect the original official 7-inch (800x480) screen to DISPLAY/DSI,
   and the capture bridge to CAMERA/CSI. Power the screen as specified by
   Raspberry Pi. A USB keyboard is optional; touch, keyboard, N64 and serial are implemented.
   The official Touch Display 2 is not supported by this Pi 3 M2 driver.
3. Power the Pico using its existing arrangement. It must run the SPI
   mode-3 Mercury firmware (commit 448b8ef or later).

COMMAND LINK — default SPOTPEAR firmware preset
Signal    Pi 3 physical pin / BCM GPIO      Pico GPIO / physical pin
MOSI      19 / GPIO10                       GP4 / 6
SCLK      23 / GPIO11                       GP6 / 9
CS        24 / GPIO8 (CE0)                  GP5 / 7
GND       25 / GND                         GND / 8
3.3 V logic. Common ground required. No MISO or READY wire is used.
Other existing Pico board presets may assign different SPI pins: retain
those firmware assignments. Never substitute the earlier PINE64 GPIO names.

VIDEO RETURN
Pico HDMI output -> TC358743 HDMI input -> Pi 3 CAMERA connector.
Use a Pi-compatible two-lane 15-pin CAMERA module/ribbon. This image assumes
its bridge reference oscillator is 27 MHz and Pico output is 640x480p60.
Connect ribbon cables with both boards powered off, using the module's
orientation instructions. Use CAMERA, not DISPLAY. Power the bridge as its
module requires; this image enables the Pi camera connector's power signal.
The TC358743 and FT5406 touch controller share CAMERA/DISPLAY I2C0
(GPIO44/45), not header I2C1. Firmware touch polling is disabled; the
application polls FT5406 directly. Do not remove disable_touchscreen=1
while using this build. I2C operations time out after 50 ms.

WHAT SHOULD HAPPEN
The Pi displays the Jupiter M2 banner and automatically attempts capture.
It sends rotating dusk-coloured cube commands at up to 30 Hz. Press C to
switch to colour bars with a white triangle. Capture displays only incoming
CSI pixels; there is no locally rendered substitute picture on the Pi.
If capture is absent, SPI commands still run. The Pico's TV output changing
confirms command reception; the local SPI counter alone cannot prove it.

The display uses its firmware-reported resolution. On the original screen,
the full 640x480 capture is scaled to 512x384 beneath the controls; it is
not cropped. Tap the four blue buttons to change scenes, pause, retry or
stop/start. USB keyboard and serial remain available. HDMI fallback uses
firmware display detection when no original DSI screen is connected.

Controls (USB keyboard or serial):
 C           Cube / bars
 Arrows      Select a blue button (USB keyboard)
 Enter       Activate selected button
 Z           Pause/resume automatic command sending
 R           Stop/reinitialize capture
 X           Stop/start the bring-up
 Escape      Ignored; special keys never reboot

Serial: 115200, 8N1; 3.3 V adapter. Pi pin 8/GPIO14 TX -> adapter RX,
Pi pin 10/GPIO15 RX <- adapter TX, and common GND. Do not connect adapter VCC.

A useful first log is:
 [pi3/mercury] FIRST CSI FRAME RECEIVED
Followed by increasing CSI frame counts and RX errors=00000000.
This proves reception events, not visual correctness: check the captured
bars/cube too. For failures, retain the entire boot log and bridge diagnostics.
Missing bridge, no signal, I2C errors, and no recent frames are distinguished.

IMPLEMENTED
- Circle AArch32 boot, native-resolution framebuffer, original official
  DSI screen, direct FT5406 touch input, USB keyboard, UART, timers and SPI0.
- Little-endian length-prefixed Mercury SPI protocol, mode 3, 1 MHz.
- TC358743 probe/configuration, camera I2C/power, two-lane RGB888 capture.
- CSI RGB888 data type 0x24, 1920-byte stride, 921600-byte DMA frames,
  three buffers, BGR-to-RGB565 display conversion, error-frame rejection.

VERIFIED IN SOFTWARE
- Pi 3 AArch32 target compiles and links from the bundled source.
- Bridge transport tests cover both addresses, stream mute/unmute, missing
  hardware, invalid ID, and I2C read/write failures (ASan/UBSan).
- All 256 cube angles and bars packets run through the unchanged Mercury
  parser/rasterizer; length/end-marker and selected bars pixels are checked.
  Host rendering omits the final blocking swap, which needs the scanout core.
- Actual Circle I2C code passes stuck-controller, FIFO bounds, timeout
  recovery, NACK and timer-wrap tests. Screen fitting and touch regions pass.
- Full SD partition, boot-file readback and hashes are checked.

NOT YET HARDWARE-VALIDATED
The user confirmed M1 boots and displays its buttons on the original DSI screen.
M2 touch, N64 timing, keyboard changes, SPI reception and CSI capture still
require physical testing. Host protocol tests cannot validate GPIO timing.
TC358743 settings retain the upstream fixed 27-MHz/640x480 configuration;
other bridge oscillators or input formats need separate settings.
Audio, SD runtime access, full Jupiter menu and the remaining demos
are subsequent port work. Allwinner DE2/Cedar implementations are not included.

SOURCE / BUILD
Install an arm-none-eabi GCC C/C++ toolchain (and git, curl), then:
 TOOLCHAIN_PREFIX=arm-none-eabi- ./build.sh
build.sh runs ./fetch_vendor.sh first (network needed once).
Build output: app/kernel8-32.img (also copied into boot/). The SD card's
FAT partition takes boot/: bootcode.bin, start.elf, fixup.dat, config.txt,
cmdline.txt, kernel8-32.img.
For host regressions: ./fetch_vendor.sh && sh tests/run.sh (the I2C test
compiles the patched Circle i2cmaster.cpp).
Exact dependency revisions and local changes are in SOURCE-BASELINES.txt.

M2 INPUT CHANGES
- USB complete key strings are decoded, preventing arrow/function-key escape
  prefixes from triggering the old reboot shortcut. Arrows select; Enter acts.
- Native touch reads the 3-byte header then 6-byte contact records, inverts
  both axes per Raspberry Pi's original-display overlay, and filters held
  contacts and stale startup reports. Firmware touch remains disabled.
- Status row shows touch health, coordinates, press count, N64 connection,
  receive bits/error, CSI frames and last action.
- N64 DATA: Pi physical pin 13 (BCM27), NOT Pico pin 13.
  Supply controller from 3.3V (Pi pin 1 or 17), GND (pin 6 or 14), with a
  2.2k-4.7k resistor from DATA to 3.3V. No 5V on DATA. Existing SPI unchanged.
  D-pad selects; A activates; Start pauses; B stops/starts; Z retries capture;
  C buttons switch scene. Stick and L/R have no assigned actions.
- N64 uses the ARM counter and open-drain GPIO; failed replies never publish
  buttons and reconnection establishes a baseline before generating edges.
- A white fade alone does not establish a cause. The old code demonstrably
  rebooted on arrow-key prefixes; hardware reset/power issues remain possible
  if ordinary C/Z/X keys also reset the replacement build.
