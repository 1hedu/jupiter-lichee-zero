#!/usr/bin/env bash
# Focused checks for the latest SDK fixes used by the PINE menu build.
set -euo pipefail
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
out=${PINE64_RECONCILIATION_TEST_OUT:-"$repo/build/pine64/reconciliation-tests"}
cc=${HOST_CC:-cc}
mkdir -p "$out"
flags=(-std=c11 -O1 -g -Wall -Wextra -Werror -Wno-unused-parameter
       -Wno-sign-compare -Wno-unused-function -fsanitize=address,undefined
       -fno-omit-frame-pointer -DJUPITER_BOARD_PINE64=1
       -I"$repo/include" -I"$repo/lib" -I"$repo/boards/pine64")
export ASAN_OPTIONS=detect_leaks=0
export UBSAN_OPTIONS=halt_on_error=1
"$cc" "${flags[@]}" "$repo/boards/pine64/tests/input_reconciliation_test.c" -o "$out/input"
"$out/input"
"$cc" "${flags[@]}" "$repo/boards/pine64/tests/storage_reconciliation_test.c" \
    "$repo/lib/cpakfs.c" -o "$out/storage"
"$out/storage"
# Extract the exact formatter section so the test does not replace the
# sanitizer runtime's malloc/printf/ctype symbols with a freestanding libc.
python3 - "$repo/lib/libc_shim.c" "$out/formatter.c" <<'PY'
import pathlib, sys
source = pathlib.Path(sys.argv[1]).read_text()
begin = source.index('static int emit(')
end = source.index('/* ctype helpers', begin)
prefix = ('#include <stddef.h>\n#include <stdint.h>\n#include <stdarg.h>\n'
          '#define vsnprintf pine64_test_vsnprintf\n'
          '#define snprintf pine64_test_snprintf\n'
          '#define sprintf pine64_test_sprintf\n')
pathlib.Path(sys.argv[2]).write_text(prefix + source[begin:end])
PY
"$cc" "${flags[@]}" "$repo/boards/pine64/tests/format_reconciliation_test.c" \
    "$out/formatter.c" -o "$out/format"
"$out/format"
"$cc" "${flags[@]}" -ffunction-sections -fdata-sections -Wl,--gc-sections \
    "$repo/boards/pine64/tests/renderer_reconciliation_test.c" \
    "$repo/lib/nes.c" "$repo/lib/gb.c" "$repo/lib/snes.c" "$repo/lib/genesis.c" \
    -o "$out/renderers"
"$out/renderers"
python3 "$repo/boards/pine64/tests/game_relink_test.py"
