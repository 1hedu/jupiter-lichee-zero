#!/bin/bash
# run.sh — boot a Jupiter bare-metal bin on the emulated Lichee Pi Zero.
#
# Usage (from WSL):
#   ./scripts/run.sh [/path/to/jupiter.bin]      # serial-only, no display yet
#   ./scripts/run.sh -g [/path/to/jupiter.bin]   # with SDL display window
#
# Default binary: ../jupiter/combined/build/jupiter.bin relative to the
# project root, i.e. /mnt/c/Users/darek/jupiter/combined/build/jupiter.bin.
set -euo pipefail

QEMU="$HOME/licheeEmu-build/qemu/build/qemu-system-arm"
DEFAULT_BIN="/mnt/c/Users/darek/jupiter/combined/build/jupiter.bin"

WITH_DISPLAY=0
if [ "${1:-}" = "-g" ]; then
    WITH_DISPLAY=1
    shift
fi

BIN="${1:-$DEFAULT_BIN}"

[ -x "$QEMU" ] || { echo "Build QEMU first: scripts/patch-and-build.sh"; exit 1; }
[ -f "$BIN" ] || { echo "Not found: $BIN"; exit 1; }

#
# icount locks QEMU's virtual clock to instruction count, giving the
# sub-microsecond timer granularity the N64 controller's 1µs/3µs
# pulse-width protocol needs. Without it, virtual timer expiries bunch
# up at ~20µs resolution and every protocol-decoded bit times out.
#
ICOUNT_OPTS="-icount shift=auto,align=off,sleep=on"

# PulseAudio via WSLg for the V3s codec. Fall back to `none` if pactl
# can't reach the daemon (e.g. WSL without WSLg / headless SSH).
AUDIO_OPTS="-audio pa,model=allwinner-v3s-codec"
if ! pactl info >/dev/null 2>&1; then
    AUDIO_OPTS="-audio none,model=allwinner-v3s-codec"
fi

# Plug an SD card image if one is sitting next to this script. Real HW
# wants a card present for the sdmmc example to do anything useful.
SD_IMG="$(dirname "$0")/../docs/sd.img"
SD_OPTS=""
if [ -f "$SD_IMG" ]; then
    SD_OPTS="-drive if=sd,file=$SD_IMG,format=raw"
fi

echo "==> Running $BIN on lichee-zero"
if [ "$WITH_DISPLAY" = 1 ]; then
    exec "$QEMU" -M lichee-zero -m 64 -kernel "$BIN" \
                 $ICOUNT_OPTS $AUDIO_OPTS $SD_OPTS \
                 -serial stdio -display sdl
else
    exec "$QEMU" -M lichee-zero -m 64 -kernel "$BIN" \
                 $ICOUNT_OPTS $AUDIO_OPTS $SD_OPTS \
                 -serial stdio -display none
fi
