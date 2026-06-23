"""Pack WC1 music WAVs into a single SD-block image.

Layout:
    music_lba + 0  : 4-sector pack header
    music_lba + 4  : track 0 PCM (sector-aligned)
    music_lba + 4 + ceil(t0_bytes/512) : track 1 PCM
    ...

Header (sector 0 of pack):
    char     magic[4];      // 'WC1M'
    uint16_t version;       // 1
    uint16_t num_tracks;
    uint32_t hdr_sectors;   // 4
    uint32_t reserved;
    track_entry_t tracks[num_tracks];

track_entry_t (32 bytes):
    char     name[16];      // null-terminated, e.g. "music/00"
    uint32_t lba_offset;    // sectors past music_lba where track PCM starts
    uint32_t length_bytes;  // PCM payload size, exact (not sector-rounded)
    uint32_t sample_rate;   // Hz
    uint32_t flags;         // bit0 = mono (1) / stereo (0); rest reserved

Usage:
    python scripts/sd_pack_music.py tools/sc55_render/out/ build/wc1_music.img
    # then dd if=build/wc1_music.img of=/dev/sdX seek=524288
"""

import argparse
import os
import struct
import sys
import wave
from pathlib import Path

SECTOR = 512
HDR_SECTORS = 4

def pack_track_entry(name: str, lba_offset: int, length_bytes: int,
                     sample_rate: int, mono: bool) -> bytes:
    name_b = name.encode('ascii')
    if len(name_b) > 15:
        raise ValueError(f"track name too long (max 15 chars): {name}")
    name_b = name_b + b'\0' * (16 - len(name_b))
    flags = 1 if mono else 0
    return name_b + struct.pack('<IIII', lba_offset, length_bytes, sample_rate, flags)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('in_dir', help='dir of NN.wav files (00..44)')
    ap.add_argument('out_img', help='output raw image file')
    ap.add_argument('--max', type=int, default=64, help='max tracks (default 64)')
    args = ap.parse_args()

    in_dir = Path(args.in_dir)
    wavs = sorted(in_dir.glob('*.wav'))
    if not wavs:
        sys.exit(f"no WAV files in {in_dir}")

    if len(wavs) > args.max:
        sys.exit(f"too many tracks: {len(wavs)} > {args.max}")

    # Read each WAV, strip header, capture PCM.
    tracks = []
    for w in wavs:
        with wave.open(str(w), 'rb') as wf:
            ch = wf.getnchannels()
            sw = wf.getsampwidth()
            sr = wf.getframerate()
            nf = wf.getnframes()
            if sw != 2:
                sys.exit(f"{w.name}: only 16-bit WAVs supported (got sampwidth={sw})")
            if ch not in (1, 2):
                sys.exit(f"{w.name}: only mono/stereo (got channels={ch})")
            pcm = wf.readframes(nf)
        # Track name = file stem prefixed with "music/" so it matches the
        # paths Lua scripts pass to PlayMusic("music/00").
        name = f"music/{w.stem}"
        tracks.append({
            'name': name,
            'pcm':  pcm,
            'rate': sr,
            'mono': ch == 1,
            'len':  len(pcm),
        })
        print(f"  {w.name}: {ch}ch {sr}Hz {nf} frames = {len(pcm)} bytes -> '{name}'")

    # Lay out: header at offset 0, payloads sector-aligned.
    layout_lba = HDR_SECTORS
    for t in tracks:
        t['lba'] = layout_lba
        sectors = (t['len'] + SECTOR - 1) // SECTOR
        layout_lba += sectors

    # Build header bytes.
    header = struct.pack('<4sHHII', b'WC1M', 1, len(tracks), HDR_SECTORS, 0)
    for t in tracks:
        header += pack_track_entry(t['name'], t['lba'], t['len'], t['rate'], t['mono'])
    if len(header) > HDR_SECTORS * SECTOR:
        sys.exit(f"header too large: {len(header)} bytes > {HDR_SECTORS*SECTOR} reserved")
    header += b'\0' * (HDR_SECTORS * SECTOR - len(header))

    # Write image.
    out = Path(args.out_img)
    out.parent.mkdir(parents=True, exist_ok=True)
    total_bytes = 0
    with open(out, 'wb') as f:
        f.write(header)
        total_bytes += len(header)
        for t in tracks:
            f.write(t['pcm'])
            pad = (SECTOR - (t['len'] % SECTOR)) % SECTOR
            if pad:
                f.write(b'\0' * pad)
            total_bytes += t['len'] + pad

    print(f"\nwrote {out} ({total_bytes} bytes = {total_bytes // SECTOR} sectors)")
    print(f"target: dd if={out} of=/dev/sdX seek=<music_lba>")


if __name__ == '__main__':
    main()
