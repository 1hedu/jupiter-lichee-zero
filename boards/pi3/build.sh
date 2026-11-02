#!/bin/sh
set -eu
cd "$(dirname "$0")"
TOOLCHAIN_PREFIX=${TOOLCHAIN_PREFIX:-arm-none-eabi-}
./fetch_vendor.sh
(cd vendor/circle && ./configure -f -r 3 -p "$TOOLCHAIN_PREFIX" --keymap US && ./makeall -j4)
make -C app -j4
cp app/kernel8-32.img boot/kernel8-32.img
