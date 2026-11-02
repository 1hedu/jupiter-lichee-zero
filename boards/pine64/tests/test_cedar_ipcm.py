#!/usr/bin/env python3
"""Host verification of the actual portable encoder using FFmpeg's decoder.

Run from any directory. Requires a C compiler and ffmpeg on PATH; no target
hardware or cross compiler is needed. The generated stream must reproduce the
input NV12 samples exactly, including cropped pictures and escaped zero bytes.
"""
import ctypes
from pathlib import Path
import subprocess
import tempfile


SOURCE = Path(__file__).resolve().parents[1] / "cedar_ipcm.c"
U8 = ctypes.c_uint8


def main():
    with tempfile.TemporaryDirectory(prefix="pine64-ipcm-") as temp:
        libpath = Path(temp) / "ipcm.so"
        subprocess.run([
            "cc", "-std=c99", "-O2", "-Wall", "-Wextra", "-Werror",
            "-shared", "-fPIC", str(SOURCE), "-o", str(libpath)
        ], check=True)
        encode = ctypes.CDLL(str(libpath)).pine64_h264_ipcm
        encode.argtypes = [ctypes.POINTER(U8), ctypes.POINTER(U8),
                           ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32,
                           ctypes.c_int, ctypes.POINTER(U8), ctypes.c_size_t]
        encode.restype = ctypes.c_int

        # Real menu atlas sizes, the supported maximum, and a crop case.
        cases = [(384, 80), (480, 64), (512, 480), (448, 928),
                 (512, 1024), (18, 30), (16, 16)]
        for index, (width, height) in enumerate(cases):
            stride, rows = (width + 15) & ~15, (height + 15) & ~15
            # Limited-range patterns fit the board's 1 MiB stream arena.
            y = bytes(16 + ((i * 31 + i // stride * 17) % 220)
                      for i in range(stride * rows))
            uv = bytes(16 + ((i * 23 + i // stride * 7) % 225)
                       for i in range(stride * rows // 2))
            if width == 16:
                # This deliberately requires emulation-prevention escapes.
                y = bytes([0, 0, 0, 1, 0, 0, 2, 3]) * (len(y) // 8)
                uv = bytes(len(uv))
            ybuf = (U8 * len(y)).from_buffer_copy(y)
            uvbuf = (U8 * len(uv)).from_buffer_copy(uv)
            capacity = 1 << 20
            out = (U8 * (capacity + 32))(*([0xA5] * (capacity + 32)))
            qp = [0, 10, 26, 51][index % 4]
            length = encode(ybuf, uvbuf, stride, width, height, qp, out, capacity)
            assert 0 < length <= capacity, (width, height, length)
            assert bytes(out[capacity:]) == bytes([0xA5] * 32)
            encoded = bytes(out[:length])
            decoded = subprocess.run([
                "ffmpeg", "-hide_banner", "-loglevel", "error", "-xerror",
                "-f", "h264", "-i", "pipe:0", "-frames:v", "1",
                "-pix_fmt", "nv12", "-f", "rawvideo", "pipe:1"
            ], input=encoded, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
               check=True).stdout
            expected = b"".join(y[r * stride:r * stride + width]
                                for r in range(height))
            expected += b"".join(uv[r * stride:r * stride + width]
                                 for r in range(height // 2))
            assert decoded == expected, (width, height, "decoded samples differ")
            print(f"PASS {width}x{height}, QP {qp}, {length} bytes: exact NV12")

        # A too-small output must fail without writing past the stated capacity.
        small = (U8 * 96)(*([0xA5] * 96))
        assert encode(ybuf, uvbuf, stride, width, height, 10, small, 64) == -2
        assert bytes(small[64:]) == bytes([0xA5] * 32)
        assert encode(ybuf, uvbuf, stride, 0, height, 10, small, 64) == -1
        assert encode(ybuf, uvbuf, stride, 15, height, 10, small, 64) == -1
        assert encode(ybuf, uvbuf, stride, width, height, 52, small, 64) == -1
        print("PASS invalid inputs and output capacity guard")


if __name__ == "__main__":
    main()
