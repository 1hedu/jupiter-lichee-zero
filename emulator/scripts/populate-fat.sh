#!/bin/bash
# Copy the WC1 vbin assets from the Jupiter build tree into the FAT16
# partition of sd.img so the war1 example finds /videos/* and
# /intros/* on its lazy-mount of 0:/.
#
# Layout assumed: scripts/make-sd-image.sh wrote partition #2 starting
# at LBA 4096 (byte offset 0x200000 = 2 MiB).
set -euo pipefail

SD_IMG="${1:-/mnt/c/Users/darek/licheeEmu/docs/sd.img}"
SRC_VIDEOS="${2:-/mnt/c/Users/darek/jupiter/combined/build/videos}"
SRC_INTROS="${3:-/mnt/c/Users/darek/jupiter/combined/build/intros}"
PART_OFFSET=$((4096 * 512))    # 2097152 — must match make-sd-image.sh

[ -f "$SD_IMG" ]      || { echo "missing $SD_IMG"; exit 1; }
[ -d "$SRC_VIDEOS" ]  || { echo "missing $SRC_VIDEOS"; exit 1; }

MNT=$(mktemp -d)
trap 'sudo umount "$MNT" 2>/dev/null || true; rmdir "$MNT" 2>/dev/null || true' EXIT

echo "==> mounting partition 2 of $SD_IMG at $MNT"
sudo mount -o "loop,offset=$PART_OFFSET,uid=$(id -u),gid=$(id -g)" \
           "$SD_IMG" "$MNT"

echo "==> copying $SRC_VIDEOS → /videos"
mkdir -p "$MNT/videos"
cp -v "$SRC_VIDEOS"/*.vbin "$MNT/videos/" 2>&1 | tail -5

if [ -d "$SRC_INTROS" ]; then
    echo "==> copying $SRC_INTROS → /intros"
    mkdir -p "$MNT/intros"
    cp -rv "$SRC_INTROS"/* "$MNT/intros/" 2>&1 | tail -5
fi

sync

echo "==> contents of /videos on the SD image:"
ls "$MNT/videos" | head -8
echo "    (...$(ls "$MNT/videos" | wc -l) files total)"
