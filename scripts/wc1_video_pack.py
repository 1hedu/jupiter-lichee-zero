"""Convert a video file (.ogv / .mp4 / etc.) to an all-IDR baseline H.264
stream sized for Jupiter's 480x272 LCD, then pack it into a .vbin
container the cedar_video example can decode frame-by-frame.

Usage:
    wsl python3 scripts/wc1_video_pack.py <in_video> <out.vbin>
        [--width=WIDTH] [--height=HEIGHT] [--qp=25] [--fps=15]

Encoder choices (each one matters for cedar's per-frame decode call):
  - profile=baseline           : no CABAC, no B-frames (cedar is I-frame only)
  - keyint=1, min-keyint=1     : every frame is an IDR -- cedar.c only handles
                                 NAL type 5 (IDR), no P-frames at all
  - bf=0                       : no B-frames
  - qp=N (default 25)          : fixed quantizer; runtime hardcodes the same
  - no-deblock=1               : disable deblocking filter (matches the
                                 disable_deblock=1 arg passed to cedar.c at
                                 runtime)
  - direct-pred=spatial,
    weightp=0, cabac=0,
    8x8dct=0, weightb=0        : keep things simple-baseline so the cedar
                                 register setup matches

Container (.vbin) layout:
    struct vbin_header {
        char     magic[4];     // 'VBIN'
        uint16_t version;      // 1
        uint16_t width;
        uint16_t height;
        uint16_t fps_num;
        uint16_t fps_den;
        uint16_t hdr_bits;     // measured H.264 slice-header bit count
        uint32_t qp;
        uint32_t num_frames;
        uint32_t total_size;   // bytes following the header
        uint8_t  reserved[4];
    };  // 32 bytes

    repeat num_frames times:
        uint32_t nal_size;     // bytes of NAL data (Annex-B start code stripped)
        uint8_t  nal[nal_size]; // raw NAL data, NO Annex-B prefix

For now hdr_bits is hardcoded to 0 -- the runtime example sweeps possible
values and reports which produces a clean decode. Once empirically known
for our fixed encoder config, plumb it back into here.
"""
import argparse
import os
import struct
import subprocess
import sys
from pathlib import Path

VBIN_MAGIC = b'VBIN'
VBIN_VERSION = 3
VBIN_HEADER_SIZE = 48

# x264 encoder options that match cedar.c's decode assumptions.
# direct_8x8_inference=1 is the ffmpeg default for baseline; cedar.c expects it.
X264_PARAMS = ":".join([
    "keyint=1",
    "min-keyint=1",
    "scenecut=0",       # no extra IDRs from scene cuts
    "bframes=0",
    "no-deblock=1",
    "weightp=0",
    "cabac=0",
    "8x8dct=0",
])


def encode_h264(in_path: Path, out_h264: Path, w: int, h: int, qp: int, fps: int):
    """Run ffmpeg to produce an all-IDR baseline H.264 Annex-B stream."""
    cmd = [
        "ffmpeg", "-y",
        "-i", str(in_path),
        "-vf", f"scale={w}:{h}:flags=bicubic",
        "-c:v", "libx264",
        "-profile:v", "baseline",
        "-preset", "slow",
        "-qp", str(qp),
        "-r", str(fps),
        "-bf", "0",
        "-g", "1",
        "-x264-params", X264_PARAMS,
        "-an",                # no audio
        "-f", "h264",
        str(out_h264),
    ]
    print("$", " ".join(cmd))
    subprocess.run(cmd, check=True)


def has_audio_stream(in_path: Path) -> bool:
    """ffprobe — true if `in_path` carries at least one audio stream."""
    cmd = [
        "ffprobe", "-v", "error",
        "-select_streams", "a",
        "-show_entries", "stream=index",
        "-of", "csv=p=0",
        str(in_path),
    ]
    try:
        out = subprocess.run(cmd, check=True, capture_output=True, text=True).stdout
    except subprocess.CalledProcessError:
        return False
    return any(line.strip() for line in out.splitlines())


def get_video_duration_s(in_path: Path) -> float:
    """ffprobe — duration of the input's first video stream in seconds."""
    cmd = [
        "ffprobe", "-v", "error",
        "-select_streams", "v:0",
        "-show_entries", "stream=duration",
        "-of", "csv=p=0",
        str(in_path),
    ]
    out = subprocess.run(cmd, check=True, capture_output=True, text=True).stdout
    try:
        return float(out.strip())
    except ValueError:
        return 0.0


def extract_audio_s16le(in_path: Path, out_pcm: Path, rate: int, channels: int):
    """Extract audio from input as raw s16le PCM at the given rate / channels.
    Some WC1 cinematics (e.g. hfinale.ogv) have no audio stream — synthesize
    silence of the same duration as the video so vbin's audio_size still
    drives the playback-end timer in war1_movie_play_blob."""
    if not has_audio_stream(in_path):
        dur = get_video_duration_s(in_path)
        samples = max(1, int(dur * rate))
        # 2 bytes per sample × channels.
        out_pcm.write_bytes(b'\0' * (samples * 2 * channels))
        print(f"[audio] {in_path.name}: no audio stream — wrote {samples} samples of silence")
        return
    cmd = [
        "ffmpeg", "-y",
        "-i", str(in_path),
        "-ac", str(channels),
        "-ar", str(rate),
        "-f", "s16le",
        "-acodec", "pcm_s16le",
        str(out_pcm),
    ]
    print("$", " ".join(cmd))
    subprocess.run(cmd, check=True)


def rbsp_strip_emulation(nal: bytes) -> bytes:
    """Remove H.264 emulation-prevention bytes (0x03 inserted after 00 00)."""
    out = bytearray()
    i = 0
    while i < len(nal):
        if i + 2 < len(nal) and nal[i] == 0 and nal[i+1] == 0 and nal[i+2] == 3:
            out.append(0); out.append(0); i += 3
        else:
            out.append(nal[i]); i += 1
    return bytes(out)


class BitReader:
    def __init__(self, data: bytes):
        self.data = data
        self.bit_pos = 0

    def u(self, n: int) -> int:
        v = 0
        for _ in range(n):
            byte_i = self.bit_pos >> 3
            bit_i  = 7 - (self.bit_pos & 7)
            v = (v << 1) | ((self.data[byte_i] >> bit_i) & 1)
            self.bit_pos += 1
        return v

    def ue(self) -> int:
        zeros = 0
        while self.u(1) == 0 and zeros < 32:
            zeros += 1
        if zeros == 0:
            return 0
        return (1 << zeros) - 1 + self.u(zeros)

    def se(self) -> int:
        v = self.ue()
        return (v + 1) // 2 if v & 1 else -(v // 2)


def parse_sps(nal: bytes) -> dict:
    """Parse the bits of an H.264 SPS we care about for slice-header sizing."""
    rbsp = rbsp_strip_emulation(nal[1:])  # skip NAL header byte
    r = BitReader(rbsp)
    profile_idc = r.u(8)
    r.u(8)                  # constraint_set + reserved_zero
    level_idc   = r.u(8)
    r.ue()                  # seq_parameter_set_id
    if profile_idc in (100, 110, 122, 244, 44, 83, 86, 118, 128, 138, 139, 134, 135):
        chroma_format_idc = r.ue()
        if chroma_format_idc == 3:
            r.u(1)          # separate_colour_plane_flag
        r.ue()              # bit_depth_luma_minus8
        r.ue()              # bit_depth_chroma_minus8
        r.u(1)              # qpprime_y_zero_transform_bypass
        if r.u(1):          # seq_scaling_matrix_present
            raise RuntimeError("scaling matrix not handled")
    else:
        chroma_format_idc = 1
    log2_max_frame_num_m4 = r.ue()
    pic_order_cnt_type     = r.ue()
    log2_max_poc_lsb_m4    = 0
    delta_poc_zero_only    = False
    if pic_order_cnt_type == 0:
        log2_max_poc_lsb_m4 = r.ue()
    elif pic_order_cnt_type == 1:
        delta_poc_zero_only = bool(r.u(1))
        r.se()              # offset_for_non_ref_pic
        r.se()              # offset_for_top_to_bottom_field
        n = r.ue()
        for _ in range(n):
            r.se()
    max_num_ref_frames = r.ue()
    r.u(1)                  # gaps_in_frame_num_value_allowed
    r.ue()                  # pic_width_in_mbs_minus1
    r.ue()                  # pic_height_in_map_units_minus1
    frame_mbs_only_flag = r.u(1)
    if not frame_mbs_only_flag:
        r.u(1)              # mb_adaptive_frame_field
    direct_8x8_inference = r.u(1)
    return {
        "profile_idc": profile_idc,
        "level_idc": level_idc,
        "chroma_format_idc": chroma_format_idc,
        "log2_max_frame_num_minus4": log2_max_frame_num_m4,
        "pic_order_cnt_type": pic_order_cnt_type,
        "log2_max_poc_lsb_minus4": log2_max_poc_lsb_m4,
        "max_num_ref_frames": max_num_ref_frames,
        "frame_mbs_only_flag": frame_mbs_only_flag,
        "direct_8x8_inference_flag": direct_8x8_inference,
    }


def measure_slice_header_bits(nal: bytes, sps: dict, pps: dict) -> int:
    """Walk the IDR slice header and return how many bits we consumed
    BEFORE the first_macroblock_data bit. The cedar `hdr_bits` parameter
    must equal this value (cedar skips that many bits then starts CAVLC).
    The 8 NAL header bits ARE counted — cedar's BUF_INPUT starts at the
    NAL header byte."""
    rbsp = rbsp_strip_emulation(nal[1:])  # body after NAL header
    r = BitReader(rbsp)
    r.ue()                                # first_mb_in_slice = 0
    slice_type = r.ue()                   # slice_type — 2 or 7 for I
    r.ue()                                # pic_parameter_set_id
    log2_frame_num = sps["log2_max_frame_num_minus4"] + 4
    r.u(log2_frame_num)                   # frame_num
    if not sps["frame_mbs_only_flag"]:
        if r.u(1):
            r.u(1)
    is_idr = (nal[0] & 0x1F) == 5
    if is_idr:
        r.ue()                            # idr_pic_id
    if sps["pic_order_cnt_type"] == 0:
        log2_poc = sps["log2_max_poc_lsb_minus4"] + 4
        r.u(log2_poc)                     # pic_order_cnt_lsb
        if pps.get("bottom_field_pic_order_in_frame_present", 0) and sps["frame_mbs_only_flag"]:
            r.se()
    elif sps["pic_order_cnt_type"] == 1:
        if not sps.get("delta_poc_zero_only", False):
            r.se()
            if pps.get("bottom_field_pic_order_in_frame_present", 0) and sps["frame_mbs_only_flag"]:
                r.se()
    # I-slice (slice_type 2 or 7) doesn't have ref_pic_list_modification or pred_weight_table
    if is_idr:
        r.u(1)                            # no_output_of_prior_pics_flag
        r.u(1)                            # long_term_reference_flag
    r.se()                                # slice_qp_delta
    if pps.get("deblocking_filter_control_present", 0):
        idc = r.ue()                      # disable_deblocking_filter_idc
        if idc != 1:
            r.se()                        # slice_alpha_c0_offset_div2
            r.se()                        # slice_beta_offset_div2
    # Total bits consumed in the RBSP body, plus the 8 NAL header bits.
    return r.bit_pos + 8


def parse_pps(nal: bytes) -> dict:
    rbsp = rbsp_strip_emulation(nal[1:])
    r = BitReader(rbsp)
    r.ue()                                # pic_parameter_set_id
    r.ue()                                # seq_parameter_set_id
    r.u(1)                                # entropy_coding_mode_flag
    bottom = r.u(1)                       # bottom_field_pic_order_in_frame_present
    num_slice_groups = r.ue()
    if num_slice_groups > 0:
        raise RuntimeError("FMO/slice groups not handled")
    r.ue(); r.ue(); r.u(1); r.u(2)        # num_ref_idx_l0/l1, weighted_pred, weighted_bipred_idc
    r.se(); r.se(); r.se()                # pic_init_qp/qs/chroma_qp_index_offset
    deblocking = r.u(1)                   # deblocking_filter_control_present
    return {
        "bottom_field_pic_order_in_frame_present": bottom,
        "deblocking_filter_control_present": deblocking,
    }


def parse_annexb(stream: bytes):
    """Yield (nal_type, nal_payload) for each NAL in an Annex-B stream.
    payload includes the NAL header byte (NAL type is in the low 5 bits).
    Annex-B start codes (00 00 01 / 00 00 00 01) are stripped."""
    i = 0
    n = len(stream)
    starts = []
    while i < n - 2:
        if stream[i] == 0 and stream[i+1] == 0:
            if stream[i+2] == 1:
                starts.append(i + 3)
                i += 3
                continue
            if i + 3 < n and stream[i+2] == 0 and stream[i+3] == 1:
                starts.append(i + 4)
                i += 4
                continue
        i += 1
    for k, s in enumerate(starts):
        e = starts[k+1] - (4 if stream[starts[k+1]-4:starts[k+1]] == b'\x00\x00\x00\x01'
                             else 3) if k + 1 < len(starts) else n
        nal = stream[s:e]
        if not nal:
            continue
        nal_type = nal[0] & 0x1F
        yield nal_type, nal


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("output")
    ap.add_argument("--width",     type=int, default=480)
    ap.add_argument("--height",    type=int, default=272)
    ap.add_argument("--qp",        type=int, default=25)
    ap.add_argument("--fps",       type=int, default=15)
    ap.add_argument("--audio-rate",     type=int, default=22050)
    ap.add_argument("--audio-channels", type=int, default=1, choices=[1, 2])
    ap.add_argument("--no-audio",  action="store_true")
    args = ap.parse_args()

    in_path  = Path(args.input)
    out_path = Path(args.output)
    if not in_path.exists():
        sys.exit(f"input not found: {in_path}")

    # Cedar's NV12 buffers are sized for up to 512x480; macroblocks are 16x16.
    # Round dimensions to nearest 16 multiple so cedar's mb_w/mb_h math is clean.
    w = (args.width  + 15) & ~15
    h = (args.height + 15) & ~15
    if (w, h) != (args.width, args.height):
        print(f"NOTE: rounding {args.width}x{args.height} -> {w}x{h} (16-aligned)")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    h264_tmp = out_path.with_suffix(".h264")
    encode_h264(in_path, h264_tmp, w, h, args.qp, args.fps)

    audio_bytes = b''
    if not args.no_audio:
        pcm_tmp = out_path.with_suffix(".pcm")
        extract_audio_s16le(in_path, pcm_tmp, args.audio_rate, args.audio_channels)
        audio_bytes = pcm_tmp.read_bytes()
        print(f"  audio: {len(audio_bytes)} bytes ({args.audio_channels}ch "
              f"{args.audio_rate} Hz s16le, "
              f"{len(audio_bytes) // (2 * args.audio_channels)} frames)")

    stream = h264_tmp.read_bytes()
    nals = []
    sps = None
    pps = None
    for nal_type, nal in parse_annexb(stream):
        if nal_type == 7:
            sps = parse_sps(nal)
            print(f"  SPS: {sps}")
        elif nal_type == 8:
            pps = parse_pps(nal)
            print(f"  PPS: {pps}")
        elif nal_type == 5:
            nals.append(nal)
        else:
            print(f"  (unexpected NAL type {nal_type}, dropping)")

    if sps is None or pps is None:
        sys.exit("missing SPS or PPS in encoded stream")

    if not nals:
        sys.exit("no IDR NALs found in the encoded stream")

    print(f"got {len(nals)} IDR frames; size avg = "
          f"{sum(len(n) for n in nals) // len(nals)} bytes/frame")

    # Compute exact slice-header bit count for EACH frame via SPS-aware
    # parsing. x264 varies slice_qp_delta per-frame even at fixed -qp,
    # so the slice header length (which encodes qp_delta as se(...))
    # changes from frame to frame. Cedar's decoder needs the right
    # value per frame -- use a constant and you get per-frame flicker
    # because every other frame is 2 bits off.
    per_frame_hdr_bits = [measure_slice_header_bits(nal, sps, pps)
                          for nal in nals]
    unique = sorted(set(per_frame_hdr_bits))
    print(f"  per-frame hdr_bits distribution: "
          f"{[(v, per_frame_hdr_bits.count(v)) for v in unique]}")

    # Build the container. Per-frame entry layout:
    #   uint32 nal_size
    #   uint16 hdr_bits
    #   uint16 reserved
    #   uint8  nal[nal_size]
    payload = bytearray()
    for nal, hb in zip(nals, per_frame_hdr_bits):
        payload += struct.pack("<IHH", len(nal), hb, 0)
        payload += nal

    # Layout (32 bytes total):
    #   magic(4) ver(2) w(2) h(2) fps_num(2) fps_den(2) hdr_bits(2)
    #   qp(4) num_frames(4) total_size(4) reserved(4)
    # vbin v3 header (48 bytes):
    #   magic(4) version(2) reserved(2)
    #   width(2) height(2) fps_num(2) fps_den(2)
    #   qp(4) num_frames(4)
    #   video_size(4) audio_offset(4) audio_size(4)
    #   audio_rate(2) audio_channels(1) audio_bps(1)
    #   reserved(8)
    fmt = "<4sHHHHHHIIIIIHBB8s"
    assert struct.calcsize(fmt) == VBIN_HEADER_SIZE, struct.calcsize(fmt)
    audio_offset = len(payload)   # bytes from end-of-header to audio data
    header = struct.pack(fmt,
                         VBIN_MAGIC, VBIN_VERSION, 0,
                         w, h,
                         args.fps, 1,
                         args.qp, len(nals),
                         len(payload),         # video_size
                         audio_offset,
                         len(audio_bytes),
                         args.audio_rate,
                         args.audio_channels,
                         16,                   # bits per sample
                         b'\0' * 8)

    with open(out_path, "wb") as f:
        f.write(header)
        f.write(payload)
        f.write(audio_bytes)
    if not args.no_audio:
        pcm_tmp.unlink(missing_ok=True)

    print(f"wrote {out_path} ({VBIN_HEADER_SIZE + len(payload)} bytes total, "
          f"{w}x{h} @ {args.fps} fps, qp={args.qp}, {len(nals)} frames)")
    print(f"kept intermediate {h264_tmp} for playback on PC (e.g. ffplay)")


if __name__ == "__main__":
    main()
