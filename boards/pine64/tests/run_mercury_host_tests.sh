#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
out=${PINE64_MERCURY_TEST_OUT:-"$repo/build/pine64/mercury-tests"}
mkdir -p "$out"
"${HOSTCC:-cc}" -std=c11 -O1 -g -Wall -Wextra -Werror -Wshadow \
    -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer \
    -DJUPITER_BOARD_PINE64 -DPINE64_MERCURY_TEST \
    -I"$repo/boards/pine64/tests/mercury_mock" -I"$repo/mercury/test" \
    -I"$repo/include" -I"$repo/boards/pine64" -I"$repo/mercury/include" \
    "$repo/boards/pine64/mercury_host.c" \
    "$repo/mercury/src/mercury_spi.c" \
    "$repo/mercury/src/mercury_raster.c" \
    "$repo/mercury/src/mercury_displaylist.c" \
    "$repo/boards/pine64/tests/mercury_host_test.c" -pthread \
    -o "$out/mercury_host_test"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$out/mercury_host_test"
