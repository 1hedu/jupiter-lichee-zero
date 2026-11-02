#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
out=${PINE64_CPU_TEST_OUT:-"$repo/build/pine64/cpu-test"}
cross=${CROSS_COMPILE:-arm-none-eabi-}
cross64=${AARCH64_CROSS_COMPILE:-aarch64-linux-gnu-}
qemu=${QEMU_AARCH64:-qemu-system-aarch64}
mkdir -p "$out"
flags=(-mcpu=cortex-a53 -mthumb -mfpu=neon-fp-armv8 -mfloat-abi=hard
       -O2 -g -ffreestanding -fno-builtin -ffunction-sections -fdata-sections
       -DJUPITER_BOARD_PINE64 -DPINE64_CPU_TEST -I"$repo/include" -I"$repo/boards/pine64")
for name in mmu irq; do
    "${cross}gcc" "${flags[@]}" -c "$repo/boards/pine64/$name.c" -o "$out/$name.o"
done
"${cross}gcc" "${flags[@]}" -c "$repo/boards/pine64/start.S" -o "$out/start.o"
"${cross}gcc" "${flags[@]}" -c "$repo/boards/pine64/tests/cpu_smoke.c" -o "$out/test.o"
"${cross}gcc" "${flags[@]}" -c "$repo/boards/pine64/tests/irq_probe.S" -o "$out/irq_probe.o"
"${cross}gcc" "${flags[@]}" -c "$repo/lib/mem.c" -o "$out/mem.o"
"${cross}gcc" "${flags[@]}" -nostdlib -Wl,--gc-sections -T "$repo/boards/pine64/jupiter.ld" \
    -Wl,-Map,"$out/cpu-smoke.map" "$out/start.o" "$out/test.o" "$out/mmu.o" "$out/irq.o" \
    "$out/irq_probe.o" "$out/mem.o" -lgcc -o "$out/cpu-smoke.elf"
"${cross}objcopy" -O binary "$out/cpu-smoke.elf" "$out/cpu-smoke.bin"
"${cross}nm" -n "$out/cpu-smoke.elf" > "$out/symbols.txt"
bss_start=$(awk '$3 == "__bss_start" { print "0x" $1 }' "$out/symbols.txt")
bss_end=$(awk '$3 == "__bss_end" { print "0x" $1 }' "$out/symbols.txt")
usb_start=$(awk '$3 == "__bss_usb_start" { print "0x" $1 }' "$out/symbols.txt")
usb_end=$(awk '$3 == "__bss_usb_end" { print "0x" $1 }' "$out/symbols.txt")
test -n "$bss_start" && test -n "$bss_end"
"${cross64}gcc" -ffreestanding -nostdlib -DBSS_START="$bss_start" -DBSS_END="$bss_end" \
    -DUSB_BSS_START="$usb_start" -DUSB_BSS_END="$usb_end" \
    -Wl,--build-id=none -T "$repo/boards/pine64/tests/enter32.ld" \
    "$repo/boards/pine64/tests/enter32.S" -o "$out/enter32.elf"
timeout 45 "$qemu" -machine virt,virtualization=on,gic-version=2 -cpu cortex-a53 -smp 1 \
    -m 2G -display none -monitor none -serial stdio -nic none -no-reboot \
    -semihosting-config enable=on,target=native \
    -device loader,file="$out/cpu-smoke.bin",addr=0x41000000,force-raw=on \
    -device loader,file="$out/enter32.elf",cpu-num=0 2>&1 | tee "$out/cpu-qemu.log"
rg -q '^CPU smoke PASS:' "$out/cpu-qemu.log"
