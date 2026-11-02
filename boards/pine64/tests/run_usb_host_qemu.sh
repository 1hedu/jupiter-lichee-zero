#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
out=${PINE64_USB_QEMU_OUT:-"$repo/build/pine64/usb-qemu"}
cross=${CROSS_COMPILE:-arm-none-eabi-}
cross64=${AARCH64_CROSS_COMPILE:-aarch64-linux-gnu-}
mkdir -p "$out"
flags=(-mcpu=cortex-a53 -mthumb -mfpu=neon-fp-armv8 -mfloat-abi=hard
       -O2 -g -ffreestanding -fno-builtin -ffunction-sections -fdata-sections
       -DJUPITER_BOARD_PINE64 -DPINE64_CPU_TEST -DPINE64_USB_QEMU_TEST
       -I"$repo/include" -I"$repo/boards/pine64" -I"$repo/third_party/tinyusb/src")
sources=(boards/pine64/start.S boards/pine64/mmu.c boards/pine64/irq.c
         boards/pine64/usb_keyboard.c boards/pine64/tests/usb_host_qemu.c
         third_party/tinyusb/src/tusb.c third_party/tinyusb/src/common/tusb_fifo.c
         third_party/tinyusb/src/host/usbh.c third_party/tinyusb/src/host/hub.c
         third_party/tinyusb/src/class/hid/hid_host.c
         third_party/tinyusb/src/portable/ohci/ohci.c)
objects=()
for src in "${sources[@]}"; do
    obj="$out/${src//\//_}.o"
    "${cross}gcc" "${flags[@]}" -c "$repo/$src" -o "$obj"
    objects+=("$obj")
done
"${cross}gcc" "${flags[@]}" -nostdlib -Wl,--gc-sections -T "$repo/boards/pine64/jupiter.ld" \
    -Wl,-Map,"$out/usb-host.map" "${objects[@]}" -Wl,--start-group -lc -lgcc -Wl,--end-group \
    -o "$out/usb-host.elf"
"${cross}objcopy" -O binary "$out/usb-host.elf" "$out/usb-host.bin"
"${cross}nm" -n "$out/usb-host.elf" > "$out/symbols.txt"
bss_start=$(awk '$3 == "__bss_start" { print "0x" $1 }' "$out/symbols.txt")
bss_end=$(awk '$3 == "__bss_end" { print "0x" $1 }' "$out/symbols.txt")
usb_start=$(awk '$3 == "__bss_usb_start" { print "0x" $1 }' "$out/symbols.txt")
usb_end=$(awk '$3 == "__bss_usb_end" { print "0x" $1 }' "$out/symbols.txt")
"${cross64}gcc" -ffreestanding -nostdlib -DBSS_START="$bss_start" -DBSS_END="$bss_end" \
    -DUSB_BSS_START="$usb_start" -DUSB_BSS_END="$usb_end" \
    -Wl,--build-id=none -T "$repo/boards/pine64/tests/enter32.ld" \
    "$repo/boards/pine64/tests/enter32.S" -o "$out/enter32.elf"
timeout 30 python3 "$repo/boards/pine64/tests/usb_qemu_runner.py" "$out"
