#!/bin/sh
# Recreate boards/pi3/vendor/ and the Pi boot firmware from pinned
# upstreams (see SOURCE-BASELINES.txt), then apply the local patches.
# The repo carries only the patches, not the 17 MB of vendored trees.
#
#   vendor/circle     rsta2/circle    7a81e1b4 + patches/circle-i2c-timeouts.patch
#   vendor/libcamera  rsta2/libcamera b8c1f2c0 + patches/libcamera-rgb888-bridge.patch
#   boot/{bootcode.bin,start.elf,fixup.dat}
#                     raspberrypi/firmware 0641c5bf (SHA256-checked)
#
# Idempotent: existing checkouts are left alone.
set -eu
cd "$(dirname "$0")"

CIRCLE_URL=https://github.com/rsta2/circle.git
CIRCLE_REV=7a81e1b480fe7d19fc1fc695b89bff0a0848f44b
LIBCAMERA_URL=https://github.com/rsta2/libcamera.git
LIBCAMERA_REV=b8c1f2c040625438d1a0dbdcecb1b271a4cfbb92
FIRMWARE_REV=0641c5bf25d185d5bf25d8dcafd1da6dc8fdbf68

fetch_repo() { # dir url rev patch
    if [ -d "$1/.git" ]; then
        echo "$1: present, leaving as is"
        return
    fi
    git clone -q "$2" "$1"
    git -C "$1" checkout -q "$3"
    git -C "$1" apply "../../patches/$4"
    echo "$1: $3 + $4"
}

mkdir -p vendor
fetch_repo vendor/circle "$CIRCLE_URL" "$CIRCLE_REV" circle-i2c-timeouts.patch
fetch_repo vendor/libcamera "$LIBCAMERA_URL" "$LIBCAMERA_REV" libcamera-rgb888-bridge.patch

# Boot firmware, verified against the hashes of the delivered M2 image
# (bootcode.bin, fixup.dat) and of the pinned firmware commit (start.elf).
fw() { # name sha256
    if [ ! -f "boot/$1" ]; then
        curl -sSfL -o "boot/$1" \
            "https://raw.githubusercontent.com/raspberrypi/firmware/$FIRMWARE_REV/boot/$1"
    fi
    echo "$2  boot/$1" | sha256sum -c -
}
fw bootcode.bin 4948532b1c5b60935388301715154ad8d3ed394727568d830ea883468a3f5ef9
fw start.elf    14d54df30f437e2120dd99f81ae0427ad033a7ed258e20b366a5ffcff5633e35
fw fixup.dat    bf23063135feaba121b2a8340cc1d25425d921551c8cb3446efd21f3aa50f396
