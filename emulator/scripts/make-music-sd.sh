#!/bin/bash
# Build the SD image expected by examples/sdmmc/main.c (the WC1 music
# verifier): a 1 GiB image with a FAT16 partition at sector 2048 and the
# WC1 music pack spliced in at LBA 524288 (256 MiB byte offset).
#
# Usage: scripts/make-music-sd.sh [sd.img-path] [wc1_music.img-path]
set -euo pipefail

SD_IMG="${1:-/mnt/c/Users/darek/licheeEmu/docs/sd.img}"
MUSIC_IMG="${2:-/mnt/c/Users/darek/jupiter/combined/build/wc1_music.img}"
SAFE_LBA=524288

if [ ! -f "$MUSIC_IMG" ]; then
    echo "missing $MUSIC_IMG — build wc1_music.img first" >&2
    exit 1
fi

bash "$(dirname "$0")/make-sd-image.sh" "$SD_IMG"

dd if="$MUSIC_IMG" of="$SD_IMG" bs=512 seek="$SAFE_LBA" \
   conv=notrunc status=none

MUSIC_BYTES=$(stat -c %s "$MUSIC_IMG")
echo "spliced $MUSIC_IMG ($((MUSIC_BYTES/1024/1024)) MiB)" \
     "into $SD_IMG at LBA $SAFE_LBA"
