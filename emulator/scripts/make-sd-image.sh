#!/bin/bash
# Build a 1 GiB SD card image with the partition layout the war1 / sdmmc
# examples expect:
#
#   LBA 0           MBR
#   LBA 2048..4095  partition #1 — 1 MiB dummy (type 0x83). Real HW has
#                                   the SPL/u-boot here; we don't need it
#                                   in QEMU but FatFs's VolToPart in
#                                   third_party/fatfs/diskio.c expects the
#                                   FAT16 to be the SECOND partition.
#   LBA 4096..524287 partition #2 — ~254 MiB FAT16 (type 0x06).
#                                   This is what 0:/ mounts to.
#   LBA 524288+     free — leave room for `make-music-sd.sh` to splice
#                          the WC1 music pack starting here.
set -euo pipefail

IMG="${1:-/mnt/c/Users/darek/licheeEmu/docs/sd.img}"
TOTAL_MIB="${TOTAL_MIB:-1024}"
DUMMY_LBA_START=2048
DUMMY_SECTORS=2048             # 1 MiB
FAT_LBA_START=4096
FAT_LBA_END=524287
FAT_SECTORS=$((FAT_LBA_END - FAT_LBA_START + 1))
FAT_BYTES=$((FAT_SECTORS * 512))

PART="$(mktemp)"
trap 'rm -f "$PART"' EXIT

# Build the FAT partition image and format it.
dd if=/dev/zero of="$PART" bs=512 count="$FAT_SECTORS" status=none
mkfs.fat -F 16 -n LICHEE "$PART" > /dev/null

# Sparse total image; splice the FAT partition at LBA 4096.
dd if=/dev/zero of="$IMG" bs=1M count=0 seek="$TOTAL_MIB" status=none
dd if="$PART" of="$IMG" bs=512 seek="$FAT_LBA_START" \
   conv=notrunc status=none

# Hand-write the MBR partition table: 2 entries, both bootable=0.
python3 - "$IMG" \
        "$DUMMY_LBA_START" "$DUMMY_SECTORS" \
        "$FAT_LBA_START" "$FAT_SECTORS" <<'PYEOF'
import struct, sys
img_path  = sys.argv[1]
d_lba     = int(sys.argv[2])
d_count   = int(sys.argv[3])
f_lba     = int(sys.argv[4])
f_count   = int(sys.argv[5])
img = open(img_path, "r+b")
# Entry 1 @ 0x1BE: dummy Linux partition (type 0x83) — placeholder for SPL
img.seek(0x1BE)
img.write(struct.pack("<B3sB3sII", 0x00, b"\x00\x02\x03", 0x83,
                      b"\xff\xff\xff", d_lba, d_count))
# Entry 2 @ 0x1CE: FAT16 (type 0x06), bootable
img.write(struct.pack("<B3sB3sII", 0x80, b"\x00\x02\x03", 0x06,
                      b"\xff\xff\xff", f_lba, f_count))
# Boot signature
img.seek(0x1FE)
img.write(b"\x55\xaa")
img.close()
print(f"wrote MBR (p1: linux LBA {d_lba}+{d_count}, "
      f"p2: FAT16 LBA {f_lba}+{f_count}) to {img_path}")
PYEOF

ls -la "$IMG"
echo "--- MBR partition entries ---"
xxd -s 0x1B0 -l 80 "$IMG"
