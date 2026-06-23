"""Batch-convert WC1 .ogv cinematics to .vbin via wc1_video_pack.py.

Walks an input directory of .ogv files and writes one .vbin per file into
the output directory. Skips files whose .vbin already exists and is newer
than the source — re-running is cheap.

Usage:
    wsl python3 scripts/wc1_videos_convert.py \\
        /mnt/c/Users/<you>/Documents/Stratagus/data.War1gus/videos \\
        build/wc1_vbins
"""
import argparse
import os
import subprocess
import sys
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('in_dir',  help='dir of source .ogv files')
    ap.add_argument('out_dir', help='dir to write per-track .vbin files')
    ap.add_argument('--width',  type=int, default=480)
    ap.add_argument('--height', type=int, default=272)
    ap.add_argument('--qp',     type=int, default=25)
    ap.add_argument('--fps',    type=int, default=15)
    args = ap.parse_args()

    in_dir  = Path(args.in_dir)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    ogvs = sorted(in_dir.glob('*.ogv'))
    if not ogvs:
        sys.exit(f"no .ogv in {in_dir}")

    converter = THIS_DIR / 'wc1_video_pack.py'
    if not converter.exists():
        sys.exit(f"missing {converter}")

    for src in ogvs:
        dst = out_dir / (src.stem + '.vbin')
        if dst.exists() and dst.stat().st_mtime >= src.stat().st_mtime:
            print(f"[skip] {dst.name} (up-to-date)")
            continue
        print(f"[conv] {src.name} -> {dst.name}")
        cmd = [sys.executable, str(converter), str(src), str(dst),
               f'--width={args.width}', f'--height={args.height}',
               f'--qp={args.qp}', f'--fps={args.fps}']
        rc = subprocess.run(cmd).returncode
        if rc != 0:
            sys.exit(f"conversion failed for {src}")


if __name__ == '__main__':
    main()
