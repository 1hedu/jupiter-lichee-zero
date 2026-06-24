# Jupiter SDK — U-Boot SPL Build & Modification Guide

The V3s cannot run code from DRAM until the DDR2 controller is initialized.
The Boot ROM (BROM) in the chip can only load ~24KB into SRAM. U-Boot's SPL
(Secondary Program Loader) fits in that 24KB, initializes DRAM, then loads
the next stage. We use the SPL as our "BIOS" and replace everything after it
with our bare-metal game code.

---

## 1. SD Card Storage Map

The V3s BROM reads from fixed locations on the SD card:

```
Offset      Size     Contents
──────────────────────────────────────────────────
0KB         8KB      Partition table (MBR) — leave alone
8KB         32KB     SPL (sunxi-spl.bin) — loaded by BROM into SRAM
40KB        ~512KB   Payload (jupiter.bin) — loaded by SPL into DRAM
1MB+        ...      FAT/ext4 partitions (game assets, optional)
```

Sector math: 1 sector = 512 bytes.
- SPL starts at sector 16 (byte offset 8192 = 8KB)
- Payload starts at sector 80 (byte offset 40960 = 40KB)

---

## 2. Two Approaches

### Approach A: Minimal Patch (Recommended to Start)

Keep the stock SPL + full U-Boot, but configure U-Boot to automatically
load and jump to `jupiter.bin` from the SD card. No source modification.
Easiest to set up, slightly slower boot (U-Boot proper takes ~1-2 seconds).

### Approach B: Direct Jump 

Patch the SPL to skip U-Boot proper entirely. After DRAM init, the SPL
reads `jupiter.bin` from a raw SD card offset into `0x41000000` and
jumps directly to it. Boot time: ~100ms from power-on to game code running.

This guide covers both.

---

## 3. Approach A: Stock U-Boot with Auto-Boot Script

### 3.1 Build U-Boot

On your Linux build machine (not in Claude's container):

```bash
# Install toolchain
sudo apt install gcc-arm-linux-gnueabihf make bison flex libssl-dev

# Clone U-Boot
git clone https://source.denx.de/u-boot/u-boot.git
cd u-boot
git checkout v2024.01   # or latest stable tag

# Configure for Lichee Pi Zero
make CROSS_COMPILE=arm-linux-gnueabihf- LicheePi_Zero_defconfig

# Build
make CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc)
```

Output: `u-boot-sunxi-with-spl.bin` (contains both SPL and U-Boot proper).

### 3.2 Write to SD Card

```bash
# Write combined SPL + U-Boot to SD card at 8KB offset
sudo dd if=u-boot-sunxi-with-spl.bin of=/dev/sdX bs=1024 seek=8 conv=notrunc
sync
```

### 3.3 Create Boot Script

Create a file `boot.cmd`:
```
# Load jupiter.bin from FAT partition 1 into DRAM at 0x41000000
fatload mmc 0:1 0x41000000 jupiter.bin
# Jump to it (go = raw jump, no kernel boot protocol)
go 0x41000000
```

Compile it:
```bash
mkimage -C none -A arm -T script -d boot.cmd boot.scr
```

### 3.4 Prepare SD Card Partitions

```bash
# Create a single FAT32 partition starting at 1MB
sudo fdisk /dev/sdX
# n → p → 1 → 2048 → (default) → t → c (W95 FAT32 LBA) → w

sudo mkfs.vfat /dev/sdX1
sudo mount /dev/sdX1 /mnt
sudo cp jupiter.bin /mnt/
sudo cp boot.scr /mnt/
sudo umount /mnt
```

### 3.5 Boot Sequence (Approach A)

```
Power on
  → BROM loads SPL from SD offset 8KB into SRAM
  → SPL initializes DDR2 (64MB)
  → SPL loads U-Boot proper into DRAM
  → U-Boot runs boot.scr
  → boot.scr loads jupiter.bin to 0x41000000
  → boot.scr executes "go 0x41000000"
  → Jupiter code runs (color bars on LCD)
```

Total boot time: ~2-3 seconds (mostly U-Boot overhead).

---

## 4. Approach B: Patched SPL — Direct Jump

This eliminates U-Boot proper entirely. The SPL initializes DRAM, reads
our payload from a raw SD card offset, and jumps to it.

### 4.1 The Patch

The key insight: the sunxi SPL already reads a "next stage" from a fixed
SD card offset. We just need to:

1. Tell it where our payload lives on the SD card
2. Tell it to load it at `0x41000000`
3. Tell it to jump there without any U-Boot header parsing

Create this file as `patches/0001-jupiter-direct-boot.patch`:

```diff
diff --git a/configs/LicheePi_Zero_defconfig b/configs/LicheePi_Zero_defconfig
index abc1234..def5678 100644
--- a/configs/LicheePi_Zero_defconfig
+++ b/configs/LicheePi_Zero_defconfig
@@ -1,6 +1,14 @@
 CONFIG_ARM=y
 CONFIG_ARCH_SUNXI=y
 CONFIG_MACH_SUN8I_V3S=y
+# Jupiter: SPL loads raw binary, no U-Boot proper
+CONFIG_SPL_RAW_IMAGE_SUPPORT=y
+CONFIG_SPL_LEGACY_IMAGE_SUPPORT=n
+CONFIG_SPL_FIT=n
+CONFIG_SPL_OS_BOOT=n
+# Load address for our payload
+CONFIG_SYS_TEXT_BASE=0x41000000
+CONFIG_SYS_LOAD_ADDR=0x41000000
 CONFIG_DRAM_CLK=408
 CONFIG_DRAM_ZQ=14779
 CONFIG_DEFAULT_DEVICE_TREE="sun8i-v3s-licheepi-zero"
diff --git a/include/configs/sun8i.h b/include/configs/sun8i.h
index abc1234..def5678 100644
--- a/include/configs/sun8i.h
+++ b/include/configs/sun8i.h
@@ -1,6 +1,11 @@
 #ifndef __CONFIG_SUN8I_H
 #define __CONFIG_SUN8I_H
 
+/* Jupiter: Override SPL payload location and load address */
+#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR  80  /* 40KB offset */
+#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_USE_SECTOR
+#define CONFIG_SYS_MONITOR_LEN    (512 * 1024)  /* max payload size */
+
 #include <configs/sunxi-common.h>
 
 #endif
```

### 4.2 The Nuclear Option (Simplest, Most Reliable)

If the patch above doesn't cleanly apply to your U-Boot version, there's
a simpler brute-force approach. After building stock U-Boot, locate the
function `jump_to_image_no_args()` in the SPL framework — it's a `__weak`
symbol that we can override.

Create `board/sunxi/jupiter_spl.c`:

```c
/*
 * Jupiter SPL override — skip U-Boot proper, jump to raw payload.
 *
 * The SPL framework calls jump_to_image_no_args() after loading.
 * This is a __weak symbol we can override.
 */
#include <common.h>
#include <spl.h>

void __noreturn jump_to_image_no_args(struct spl_image_info *spl_image)
{
    typedef void __noreturn (*entry_t)(void);

    /* Force entry point to our fixed address regardless of what
     * the SPL loader thinks it found in the image header */
    entry_t entry = (entry_t)0x41000000;

    /* Clean up: disable interrupts, flush caches */
    disable_interrupts();
    cleanup_before_linux();  /* sunxi helper: flushes caches */

    entry();

    /* Never reached */
    while (1);
}
```

Add it to the SPL build in `board/sunxi/Makefile`:
```makefile
ifdef CONFIG_SPL_BUILD
obj-y += jupiter_spl.o
endif
```

### 4.3 Build the Patched SPL

```bash
cd u-boot

# Apply patch (if using the defconfig approach)
git apply patches/0001-jupiter-direct-boot.patch

# Or manually add jupiter_spl.c (if using the nuclear option)

# Configure and build
make CROSS_COMPILE=arm-linux-gnueabihf- LicheePi_Zero_defconfig
make CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc)
```

You only need `spl/sunxi-spl.bin` from this build (not the full U-Boot).

### 4.4 Write to SD Card (Approach B)

```bash
# Write SPL at 8KB offset
sudo dd if=spl/sunxi-spl.bin of=/dev/sdX bs=1024 seek=8 conv=notrunc

# Write Jupiter payload at 40KB offset (sector 80)
sudo dd if=jupiter.bin of=/dev/sdX bs=1024 seek=40 conv=notrunc

sync
```

No partitions needed. No filesystem. Just two raw blobs on the SD card.

### 4.5 Boot Sequence (Approach B)

```
Power on
  → BROM loads SPL from SD offset 8KB into SRAM (~1ms)
  → SPL initializes DDR2 (~50ms)
  → SPL reads payload from SD offset 40KB into 0x41000000 (~10ms)
  → SPL jumps to 0x41000000
  → Jupiter code runs
```

Total boot time: ~100ms. Power to pixels.

---

## 5. SD Card Flashing Script

Save this as `flash.sh` in your project root:

```bash
#!/bin/bash
# flash.sh — Write SPL + Jupiter payload to SD card
# Usage: ./flash.sh /dev/sdX

set -e

SD="$1"
if [ -z "$SD" ]; then
    echo "Usage: $0 /dev/sdX"
    exit 1
fi

echo "WARNING: This will write to $SD. All data will be lost."
read -p "Continue? (y/N) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    exit 1
fi

SPL="spl/sunxi-spl.bin"
PAYLOAD="build/jupiter.bin"

if [ ! -f "$SPL" ]; then
    echo "Error: $SPL not found. Build U-Boot SPL first."
    exit 1
fi

if [ ! -f "$PAYLOAD" ]; then
    echo "Error: $PAYLOAD not found. Run 'make' first."
    exit 1
fi

echo "Writing SPL to offset 8KB..."
sudo dd if="$SPL" of="$SD" bs=1024 seek=8 conv=notrunc status=progress

echo "Writing payload to offset 40KB..."
sudo dd if="$PAYLOAD" of="$SD" bs=1024 seek=40 conv=notrunc status=progress

sync
echo "Done. Insert SD card and power on."
```

---

## 6. Debugging Tips

**Nothing on screen?**

1. Connect a 3.3V USB-UART adapter to the Lichee Pi Zero's UART0 pins
   (PB8=TX, PB9=RX). Baud: 115200 8N1.
2. The SPL prints DRAM init messages. If you see those, DRAM is working
   and the SPL is running.
3. If the SPL prints "jumping to..." but the screen stays dark, the
   problem is in our display init code (CCU/GPIO/DE2/TCON).
4. If you see NO serial output at all, the SPL isn't loading — check
   the SD card offset (must be exactly 8KB = sector 16).

**Screen shows garbage / rolling image?**

- TCON timing is wrong for your specific panel. Double-check HBP, VBP,
  HSYNC, VSYNC values against the panel datasheet.
- Try toggling IO_POL bits (HSYNC/VSYNC polarity).

**Screen is solid black but backlight is on?**

- DE2 mixer may not be initialized (the zeroing step is critical).
- Framebuffer address may be wrong — verify UI_ADDR(0) matches where
  you wrote the test pattern.
- Check that PLL_VIDEO locked (bit 28 of PLL_VIDEO_CTRL should be 1).

---

## 7. Recommendation

Start with **Approach A** (stock U-Boot + boot.scr). It's zero-risk and
lets you iterate on the display code without rebuilding the SPL every time.
You just swap `jupiter.bin` on the FAT partition.

Once the display is working and you're ready to ship a cartridge, switch
to **Approach B** for the instant-on boot experience the Manifesto demands.
