# Getting Started — Zero to Color Bars

A single linear walkthrough: from "I just heard about this" to
"my LCD shows the colorbars demo and UART prints `[video] init complete`."

Everything in this guide is mandatory the first time. Subsequent
rebuilds are just `make GAME=… && copy to SD card`.

Estimated time end-to-end: **2-3 hours** (most of it waiting on
U-Boot to compile and shipping for parts you don't have yet).

---

## 1. What to buy

The complete bill of materials for a from-scratch first boot. None
of these are optional unless noted.

| Item | Approx. cost | Notes |
|---|---|---|
| **Lichee Pi Zero** (V3s board) | $15-20 | The plain "Lichee Pi Zero" or "Lichee Pi Zero W" — both work. Buy from Sipeed's official store, AliExpress, or Mouser. |
| **480×272 RGB parallel LCD** | $10-15 | Reference panel: HY0430IPS04-04. Any 40-pin FPC 480×272 panel with the standard RGB pinout works; if you buy a different one, you'll likely need to tweak `LCD_W`/`LCD_H` and the TCON timing in `include/v3s.h`. Sipeed sells a matched LCD; AliExpress has clones for less. |
| **microSD card, 1 GB or larger** | $5 | Class 10 or better. Brand-name (SanDisk/Samsung/Kingston) to avoid bad clones. |
| **microSD-to-USB reader** | $3 | For flashing from your build machine. |
| **micro-USB cable** | $0 (probably have one) | Powers the Lichee Pi Zero from any USB port (PC, phone charger, USB battery). |
| **USB-to-UART adapter, 3.3V** | $3-5 | CP2102, CH340, or FT232 — all work. You need this for debug output; first-boot is opaque without it. **Make sure it's 3.3V**, not 5V — 5V will damage the V3s. |
| **3 jumper wires (female-female)** | $1 | UART connections to the Lichee header. |

Optional from-day-one (skip if you just want to see color bars):

| Item | For |
|---|---|
| N64 / NES / SNES / Genesis controller | Input — only needed for examples that take input |
| MIDI breakout (see [`MIDI_HW_GUIDE.md`](MIDI_HW_GUIDE.md)) | FB-01 / MT-32 / WaveTerm editors driving real synths |
| YM3438 + 8 MHz crystal + audio mixing parts | Real FM audio output, see [`SHOPPING_LIST.md`](SHOPPING_LIST.md) |

---

## 2. Install the software prerequisites

You need three things on your build machine: the ARM toolchain,
`u-boot-tools` (for `mkimage`), and Python 3.

### Linux (Debian / Ubuntu / WSL Ubuntu)

```bash
sudo apt update
sudo apt install u-boot-tools python3 git make build-essential
```

For the ARM toolchain, the apt package is often old. Use the
**official ARM release** to get 14.2:

```bash
# Download from https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
# Pick "arm-none-eabi" → Linux x86_64 hosted, version 14.2.Rel1
# Example URL (check site for current):
wget https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz

# Extract
tar xf arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz -C ~/
# Add to PATH (also add this to ~/.bashrc to make it permanent)
export PATH=~/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi/bin:$PATH

# Verify
arm-none-eabi-gcc --version    # should report 14.2.1
```

### macOS

```bash
brew install u-boot-tools python3 git make
brew install --cask gcc-arm-embedded   # ARM official toolchain
arm-none-eabi-gcc --version
```

If you don't have Homebrew: install it from <https://brew.sh> first.
macOS as a build host is "should work" — not extensively tested by
the SDK author. Linux / WSL is the safe choice.

### Windows (native)

Use **WSL2 with Ubuntu** — it's the only supported path. Install
WSL from PowerShell as administrator:

```powershell
wsl --install -d Ubuntu
```

Then reboot, open the Ubuntu terminal, and follow the Linux
instructions above.

(Native Windows MinGW builds aren't supported. The asset-prep
scripts assume `wsl python3` invocations.)

---

## 3. Wire the hardware

### 3.1 Attach the LCD

The Lichee Pi Zero has a **40-pin FPC connector** on the back for
the LCD. Flip the brown plastic latch up, slide the FPC ribbon
cable in (contacts facing down toward the board), latch back down.
The HY0430IPS04-04 panel ships with this cable pre-soldered.

**Orientation matters.** If you see a garbled or inverted image,
the cable's flipped — re-seat with contacts on the correct side.

### 3.2 Hook up UART for debug output

You can't debug without this. The boot ROM, U-Boot, and Jupiter
all log to UART0 at 115200 8N1.

| USB-UART pin | Lichee Pi Zero 2.54 mm header pin | Signal |
|---|---|---|
| RX (input) | pin 18 (PB8) | UART0_TX |
| TX (output) | pin 20 (PB9) | UART0_RX |
| GND | pin 29 | GND |

**Important:** RX-to-TX cross-wiring. Your adapter's RX listens to
the Pi Zero's TX (pin 18) and vice versa.

Open a serial terminal on your build machine pointed at the
adapter's device node:
- Linux/macOS: `picocom -b 115200 /dev/ttyUSB0` (or
  `screen /dev/ttyUSB0 115200`)
- Windows (native): PuTTY or TeraTerm at COMx, 115200 8N1, no
  flow control
- WSL: USB devices need explicit forwarding via `usbipd` (see
  <https://learn.microsoft.com/en-us/windows/wsl/connect-usb>) or
  just use a Windows terminal alongside the WSL build environment

### 3.3 Power

Plug a micro-USB cable from any USB port (PC, phone charger, USB
battery) into the Lichee Pi Zero's micro-USB port. No power switch —
power-up is automatic on plug-in. Current draw is < 500 mA, so any
USB 2.0 port can run it.

Don't power the board yet — finish the SD card first.

---

## 4. Build U-Boot

U-Boot is what the V3s's mask ROM loads first; it initializes DRAM
and then loads your `jupiter.bin`. This is a one-time build per
machine.

```bash
# Need a cross-compiler for Linux-on-ARM in addition to the
# bare-metal one (yes, two different toolchains, sorry):
sudo apt install gcc-arm-linux-gnueabihf bison flex libssl-dev

# Clone and build
cd ~  # or wherever you keep source
git clone https://source.denx.de/u-boot/u-boot.git
cd u-boot
git checkout v2024.01

make CROSS_COMPILE=arm-linux-gnueabihf- LicheePi_Zero_defconfig
make CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc)
```

Takes ~5-10 minutes on a modern laptop. Output: `u-boot-sunxi-with-spl.bin`
in the U-Boot directory. Save its path — you'll `dd` it onto the SD
card next.

For a deeper dive (faster-boot patched SPL, "Approach B"), see
[`UBOOT_SPL_GUIDE.md`](UBOOT_SPL_GUIDE.md). The stock Approach A
described here is what you want for first-time setup.

---

## 5. Prepare the SD card

Find your SD card's device path. **Triple-check this** — picking the
wrong device will wipe your build machine's disk.

```bash
# Linux: lsblk before and after inserting the card; the new entry is it
lsblk
# It'll be something like /dev/sdb or /dev/mmcblk0
# In this guide we'll call it /dev/sdX — substitute your real path
```

### 5.1 Write U-Boot

The V3s mask ROM reads SPL from a fixed SD-card offset (8 KB).

```bash
sudo dd if=~/u-boot/u-boot-sunxi-with-spl.bin of=/dev/sdX bs=1024 seek=8 conv=notrunc
sync
```

### 5.2 Partition the card

The boot partition is a FAT volume that U-Boot reads `jupiter.bin`
and `boot.scr` from. We'll start it at 1 MB to leave U-Boot's
~512 KB intact at the front.

```bash
sudo parted /dev/sdX --script -- \
    mklabel msdos \
    mkpart primary fat32 1MiB 100% \
    set 1 boot on
sudo mkfs.vfat -F 32 /dev/sdX1
```

(If `parted` isn't installed: `sudo apt install parted`.)

### 5.3 Mount and copy Jupiter files

```bash
mkdir -p /tmp/sd
sudo mount /dev/sdX1 /tmp/sd
```

Now build the SDK example you want to boot. From the `jupiter`
repository root:

```bash
make GAME=examples/colorbars/main.c
make boot
```

That produces `build/jupiter.bin` (the example) and `build/boot.scr`
(the U-Boot launch script). Copy both onto the SD card:

```bash
sudo cp build/jupiter.bin build/boot.scr /tmp/sd/
sudo umount /tmp/sd
```

(The `make sdcard MNT=/tmp/sd` target does the cp + umount in one
shot once you've got the mount path set up.)

### 5.4 Unplug, insert into Lichee Pi Zero

Pull the card out of your build machine, push it into the Lichee Pi
Zero's SD slot (contacts facing the PCB).

---

## 6. First boot

1. **Make sure your serial terminal is running** and listening to
   the right USB-UART device at 115200 8N1.
2. **Plug in the micro-USB cable** to the Lichee Pi Zero.

You should see on the serial console, within about 3 seconds:

```
U-Boot SPL 2024.01 (...)
DRAM: 64 MiB
Trying to boot from MMC1

U-Boot 2024.01 (...)
...
Hit any key to stop autoboot: 0
reading boot.scr
reading jupiter.bin
123456 bytes read in 5 ms
## Executing script at 41000000
Switching to game

[video] killing U-Boot TCON...
[video] clearing framebuffers...
[video] ccu_init...
[video] gpio_init...
[video] de2_init...
[video] tcon_init...
[video] init complete. Register dump:
  ...
```

And on the LCD: **eight vertical color bars** (red, green, blue,
white, yellow, cyan, magenta, black).

That's a successful first boot.

---

## 7. Try a more interesting example

Once color bars work, swap to another example:

```bash
make GAME=examples/parallax/main.c
sudo mount /dev/sdX1 /tmp/sd
sudo cp build/jupiter.bin /tmp/sd/
sudo umount /tmp/sd
# Power-cycle the Lichee Pi Zero (unplug + replug USB)
```

`make boot` only needs to run once — the `boot.scr` doesn't change
when the example does.

Browse [`../README.md`](../README.md) for the full list of 60
examples.

---

## 8. Troubleshooting

When nothing happens, work top-down through the chain:

| Symptom | Where to look |
|---|---|
| LED on board never lights | Power: bad cable, dead USB port, micro-USB connector not fully seated |
| LED lights but UART is silent | Either UART wiring (TX/RX swapped, GND not connected) or your terminal's wrong baud/device. Test the adapter by shorting its RX↔TX and typing in the terminal — your keystrokes should echo back. |
| UART shows U-Boot SPL but never "Switching to game" | `boot.scr` or `jupiter.bin` not on the SD card's FAT partition, or partition isn't being recognized. Run `examples/sdmmc` to verify the SD layer. |
| UART shows the game's "[video] init complete" but LCD is black | LCD wiring. Re-seat the FPC cable. If your LCD isn't HY0430IPS04-04 and times out, you may need to adjust TCON values in `include/v3s.h`. |
| LCD shows garbled / inverted / partial image | FPC cable inserted backwards or LCD timing mismatch. |
| `make` fails with "command not found" | Toolchain not in PATH. Re-check section 2. |
| `make boot` fails with "mkimage: command not found" | `u-boot-tools` not installed. `sudo apt install u-boot-tools` (or brew equivalent). |
| U-Boot complains about no MMC0 | SD card not detected. Wrong slot, not seated, or the dd-write to offset 8 KB didn't take. Re-run section 5.1. |

When stuck, the UART log is the truth — paste it into the SDK
issue tracker or a forum and someone can diagnose from it. The LCD
is just an output device; the UART is the diagnostic channel.

---

## 9. What's verified vs not

Not everything in the SDK has been wrung out against real silicon.
Before you build on top of an SDK feature, scan
[`../LIMITATIONS.md`](../LIMITATIONS.md) — particularly the
"Verification status" matrix near the top. If you're picking up the
MIDI work, the editor SysEx round-trips, or the Genesis-style
WaveTerm port for cross-compilation to a real Genesis: those are
all "built but not yet exercised against the external hardware
they target."

---

## 10. Next steps

- [`../README.md`](../README.md) — full feature surface, library API
- [`UBOOT_SPL_GUIDE.md`](UBOOT_SPL_GUIDE.md) — patched SPL for ~100 ms boots
- [`WIRING_GUIDE.md`](WIRING_GUIDE.md) — add a controller, YM3438, Mars Pico
- [`PIN_OVERLAYS.md`](PIN_OVERLAYS.md) — choose a GPIO config for your peripheral mix
- [`MIDI_HW_GUIDE.md`](MIDI_HW_GUIDE.md) — build the DIN-5 breakout
- [`ports/WC1_BUILD_GUIDE.md`](ports/WC1_BUILD_GUIDE.md) — build the Warcraft 1 port
- [`GAME_DEV_TRICK_BIBLE.md`](GAME_DEV_TRICK_BIBLE.md) — 15-chapter cookbook (DMA, NEON, tiles, sprites, dirty rects, audio, raster effects)
- [`JUPITER_HARDWARE_REFERENCE.md`](JUPITER_HARDWARE_REFERENCE.md) — full V3s register reference
