#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
out=${PINE64_SD_TEST_OUT:-"$repo/build/pine64/sd-tests"}
mkdir -p "$out"
python3 - "$repo/boards/pine64/sdmmc.c" "$out/sdmmc_host.c" <<'PY'
from pathlib import Path
import sys
text = Path(sys.argv[1]).read_text()
barrier = '__asm__ volatile("dsb sy" ::: "memory");'
assert text.count(barrier) == 1, 'Update the harness if driver barrier ownership changes'
Path(sys.argv[2]).write_text(text.replace(barrier, '(void)0; /* test: ARM barrier only */'))
PY
for name in pure timeout; do
    "${HOSTCC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -DJUPITER_BOARD_PINE64 \
        -I"$out" -I"$repo/include" -I"$repo/boards/pine64" \
        "$repo/boards/pine64/tests/sdmmc_${name}_test.c" -o "$out/sdmmc_${name}_test"
    "$out/sdmmc_${name}_test"
done
