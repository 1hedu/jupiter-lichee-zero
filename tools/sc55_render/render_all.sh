#!/bin/bash
# Render all WC1 MIDIs (00.mid .. 44.mid) through Nuked-SC55 -> 11025 Hz
# mono 16-bit WAV. Output goes to ./out/. Skips tracks already rendered.
set -e
cd "$(dirname "$0")"

MIDI_DIR="/mnt/c/Users/darek/Documents/Stratagus/data.War1gus/music"
OUT_DIR="./out"
RENDER="./build/sc55_render"

mkdir -p "$OUT_DIR"

if [ ! -x "$RENDER" ]; then
    echo "ERROR: $RENDER not found or not executable"
    exit 1
fi

ok=0
skip=0
fail=0
for n in $(seq -w 0 44); do
    in="$MIDI_DIR/$n.mid"
    out="$OUT_DIR/$n.wav"
    if [ ! -f "$in" ]; then
        echo "[$n] missing input: $in"
        fail=$((fail+1))
        continue
    fi
    if [ -f "$out" ] && [ "$out" -nt "$in" ]; then
        echo "[$n] up-to-date, skip"
        skip=$((skip+1))
        continue
    fi
    echo "[$n] rendering -> $out"
    if "$RENDER" "$in" "$out" 11025 600 2>&1 | tail -3; then
        ok=$((ok+1))
    else
        echo "[$n] FAILED"
        fail=$((fail+1))
    fi
done

echo
echo "=== summary: rendered=$ok skipped=$skip failed=$fail ==="
ls -la "$OUT_DIR"/*.wav 2>/dev/null | wc -l
