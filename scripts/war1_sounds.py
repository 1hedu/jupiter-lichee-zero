#!/usr/bin/env python3
"""
war1_sounds.py — gunzip + decode War1gus .wav.gz files into raw signed-16
mono PCM blobs at the WAV's native sample rate, plus a catalog header
mapping Stratagus paths (e.g. "blacksmith.wav") to blob start/end + rate.

The mixer in lib/audio.c (audio_pcm_play_rate) resamples on the fly, so we
keep the source rate (11025 Hz for most WC1 SFX) instead of pre-resampling
to 48 kHz — that would 4x the asset size and blow the boot-partition budget.

Usage:
    scripts/war1_sounds.py /path/to/data.War1gus/sounds/
"""
import sys, re, gzip, struct, pathlib

SCRIPT_DIR = pathlib.Path(__file__).parent
REPO_ROOT  = SCRIPT_DIR.parent
ASSETS_OUT = REPO_ROOT / "examples" / "war1" / "assets" / "sounds"
CATALOG_H  = REPO_ROOT / "examples" / "war1" / "war1_sound_catalog.h"
# Campaign-intro PCMs (long voiced narrations, ~17 MB total) DON'T fit in
# the 40 MB CODE region as static blobs. Output them into a staging dir
# that the user copies (drag-drop) onto the FatFs partition; Mix_LoadWAV
# opens them via the FatFs syscalls at "/intros/<path>.pcm" on demand and
# streams into a single malloc'd buffer (one intro plays at a time).
INTROS_OUT = REPO_ROOT / "examples" / "war1" / "fatfs_payload" / "intros"

# Empty exclude set — embed the full war1gus sound bank for both races,
# all unit voices (selected/acknowledge/annoyed/help/dead), all building
# voices (church/temple/etc), all SFX (building_collapse/fire/etc), and
# end-of-mission cinematics (victory/defeat). Total ~2.7 MB; the libc
# shim heap was shrunk to make room.
EXCLUDE = set()

def parse_wav(data: bytes):
    """Return (rate, channels, bits, samples_le_s16) for a PCM WAV.
    Handles 8-bit unsigned or 16-bit signed mono/stereo."""
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("not a RIFF/WAVE")
    pos = 12
    fmt = None
    pcm = None
    while pos + 8 <= len(data):
        ck_id   = data[pos:pos+4]
        ck_size = struct.unpack("<I", data[pos+4:pos+8])[0]
        body    = data[pos+8:pos+8+ck_size]
        pos += 8 + ck_size + (ck_size & 1)  # word-align
        if ck_id == b"fmt ":
            fmt = struct.unpack("<HHIIHH", body[:16])
            # (audio_format, channels, rate, byte_rate, block_align, bits)
        elif ck_id == b"data":
            pcm = body
            break
    if fmt is None or pcm is None:
        raise ValueError("missing fmt/data chunk")
    audio_fmt, channels, rate, _br, _ba, bits = fmt
    if audio_fmt != 1:
        raise ValueError(f"non-PCM audio format {audio_fmt}")
    if channels not in (1, 2):
        raise ValueError(f"unsupported channels {channels}")
    if bits not in (8, 16):
        raise ValueError(f"unsupported bit depth {bits}")
    # Decode to s16 mono samples
    out = bytearray()
    if bits == 8:
        if channels == 1:
            for b in pcm:
                s = (b - 128) << 8
                out += struct.pack("<h", s)
        else:
            for i in range(0, len(pcm) - 1, 2):
                avg = ((pcm[i] - 128) + (pcm[i+1] - 128)) // 2
                out += struct.pack("<h", avg << 8)
    else:  # bits == 16
        if channels == 1:
            out = bytearray(pcm)
        else:
            for i in range(0, len(pcm) - 3, 4):
                l = struct.unpack("<h", pcm[i:i+2])[0]
                r = struct.unpack("<h", pcm[i+2:i+4])[0]
                out += struct.pack("<h", (l + r) // 2)
    return rate, bytes(out)

def sanitize_sym_path(p: str) -> str:
    return "_binary_" + re.sub(r"[^A-Za-z0-9]", "_", p)

def main():
    if len(sys.argv) != 2:
        print("usage: war1_sounds.py /path/to/data.War1gus/sounds/", file=sys.stderr)
        sys.exit(1)
    src_root = pathlib.Path(sys.argv[1])
    if not src_root.is_dir():
        print(f"not a directory: {src_root}", file=sys.stderr)
        sys.exit(1)

    converted = []  # list of (strat_path, repo_pcm_path, rate)
    total_in = 0
    total_out = 0

    def convert_one(src: pathlib.Path, strat_path: str, out_subdir_rel: str):
        """src=*.wav.gz, strat_path=Stratagus-side path (e.g. "blacksmith.wav"
        or "campaigns/human/01_intro.wav"), out_subdir_rel=where the resulting
        .pcm lives under ASSETS_OUT (e.g. "blacksmith.pcm" / "campaigns/human/
        01_intro.pcm")."""
        nonlocal total_in, total_out
        if strat_path in EXCLUDE:
            return
        out_path = ASSETS_OUT / out_subdir_rel
        out_path.parent.mkdir(parents=True, exist_ok=True)
        if out_path.exists() and out_path.stat().st_mtime > src.stat().st_mtime:
            with gzip.open(src, "rb") as f:
                wav = f.read()
            try:
                rate, _ = parse_wav(wav)
            except Exception as e:
                print(f"[skip] {strat_path}: {e}", file=sys.stderr); return
        else:
            try:
                with gzip.open(src, "rb") as f:
                    wav = f.read()
                rate, pcm = parse_wav(wav)
            except Exception as e:
                print(f"[fail] {strat_path}: {e}", file=sys.stderr); return
            out_path.write_bytes(pcm)
        total_in  += src.stat().st_size
        total_out += out_path.stat().st_size
        repo_rel = out_path.relative_to(REPO_ROOT).as_posix()
        converted.append((strat_path, repo_rel, rate))

    for src in sorted(src_root.rglob("*.wav.gz")):
        rel = src.relative_to(src_root).as_posix()           # "blacksmith.wav.gz"
        strat_path = rel[:-len(".gz")]                       # "blacksmith.wav"
        out_rel = strat_path[:-len(".wav")] + ".pcm"          # "blacksmith.pcm"
        convert_one(src, strat_path, out_rel)

    # campaigns/<race>/NN_intro.wav (and the dialog wavs in the same tree)
    # are too large in aggregate to embed (~17 MB of voiced narration).
    # Decode into INTROS_OUT, drag-drop onto the FAT partition's /intros/
    # at runtime. Each .pcm is prefixed with a 16-byte header carrying
    # the sample rate so Mix_LoadWAV doesn't need a sidecar table:
    #   char     magic[4]    = 'WC1P'  (raw PCM, our header)
    #   uint32_t version     = 1
    #   uint32_t sample_rate
    #   uint32_t length_bytes (PCM payload only, excludes header)
    n_intros = 0
    war1gus_root = src_root.parent  # sounds/ is under data.War1gus/
    campaigns_src = war1gus_root / "campaigns"
    if campaigns_src.is_dir():
        for src in sorted(campaigns_src.rglob("*.wav.gz")):
            rel = src.relative_to(war1gus_root).as_posix()      # "campaigns/human/01_intro.wav.gz"
            strat_path = rel[:-len(".gz")]                       # "campaigns/human/01_intro.wav"
            out_rel = strat_path[:-len(".wav")] + ".pcm"          # "campaigns/human/01_intro.pcm"
            out_path = INTROS_OUT / out_rel
            out_path.parent.mkdir(parents=True, exist_ok=True)
            try:
                with gzip.open(src, "rb") as f:
                    wav = f.read()
                rate, pcm = parse_wav(wav)
            except Exception as e:
                print(f"[fail intro] {strat_path}: {e}", file=sys.stderr); continue
            header = struct.pack("<4sIII", b"WC1P", 1, rate, len(pcm))
            out_path.write_bytes(header + pcm)
            n_intros += 1
    if n_intros:
        print(f"wrote {n_intros} intro .pcm files under {INTROS_OUT} — copy this tree to the FAT partition (drag-drop onto E:\\intros)")

    lines = [
        "/* AUTO-GENERATED by scripts/war1_sounds.py — do not edit. */",
        "#pragma once",
        "#include <stdint.h>",
        "",
        "#ifdef __cplusplus",
        'extern "C" {',
        "#endif",
        "",
    ]
    for _sp, repo_rel, _r in converted:
        s = sanitize_sym_path(repo_rel)
        lines.append(f"extern const uint8_t {s}_start[];")
        lines.append(f"extern const uint8_t {s}_end[];")
    lines += [
        "",
        "typedef struct {",
        "    const char *path;       /* Stratagus-side path (e.g. \"blacksmith.wav\") */",
        "    const uint8_t *data;    /* raw signed-16 mono samples */",
        "    const uint8_t *data_end;",
        "    uint32_t rate;          /* native sample rate in Hz */",
        "} war1_sound_entry_t;",
        "",
        f"#define WAR1_SOUND_COUNT {len(converted)}",
        "static const war1_sound_entry_t WAR1_SOUNDS[] = {",
    ]
    for sp, repo_rel, rate in converted:
        s = sanitize_sym_path(repo_rel)
        lines.append(f'    {{ "{sp}", {s}_start, {s}_end, {rate} }},')
    lines += [
        "};",
        "",
        "#ifdef __cplusplus",
        "}",
        "#endif",
    ]
    CATALOG_H.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {CATALOG_H} with {len(converted)} entries")
    print(f"total WAV.gz in : {total_in/1024:.1f} KB")
    print(f"total PCM out   : {total_out/1024:.1f} KB")

if __name__ == "__main__":
    main()
