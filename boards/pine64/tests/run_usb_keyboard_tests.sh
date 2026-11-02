#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
out=${PINE64_USB_TEST_OUT:-"$repo/build/pine64/usb-tests"}
mkdir -p "$out"
"${HOSTCC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -DJUPITER_BOARD_PINE64 \
    -I"$repo/include" -I"$repo/boards/pine64" -I"$repo/third_party/tinyusb/src" \
    "$repo/boards/pine64/usb_keyboard.c" "$repo/boards/pine64/tests/usb_keyboard_test.c" \
    -o "$out/usb_keyboard_test"
"$out/usb_keyboard_test"
