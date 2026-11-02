#!/usr/bin/env bash
# Register-contract checks against the actual A64 driver, not an HDMI model.
set -euo pipefail
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
out=${PINE64_VIDEO_TEST_OUT:-"$repo/build/pine64/video-tests"}
mkdir -p "$out"
"${HOST_CC:-cc}" -std=c11 -Wall -Wextra -Werror -Wno-int-to-pointer-cast \
    -DJUPITER_BOARD_PINE64 -DPINE64_HOST_TEST -I"$repo/include" \
    "$repo/boards/pine64/video.c" "$repo/boards/pine64/tests/video_test.c" \
    -o "$out/video-test"
"$out/video-test"
