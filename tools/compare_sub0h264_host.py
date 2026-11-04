"""Compare Sub0h264 with FFmpeg frame by frame on the PC, without touching the board.

Needs g++ (C++23) and ffmpeg on PATH. Decoded frames stay in a temporary directory.
Exit code 0 means every frame matched; 1 means at least one differed.
"""
import argparse
import math
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
SOURCES = ROOT / "firmware" / "components" / "sub0h264"


def luma_psnr(left, right, luma_bytes):
    step = 7  # Sampling keeps this quick; the equality check still covers every byte.
    total = sum((a - b) ** 2 for a, b in zip(left[:luma_bytes:step], right[:luma_bytes:step]))
    mean = total / (luma_bytes / step)
    return 99.0 if mean == 0 else 10.0 * math.log10(255.0 * 255.0 / mean)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sample", type=Path)
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=360)
    args = parser.parse_args()
    frame_bytes = args.width * args.height * 3 // 2
    luma_bytes = args.width * args.height
    with tempfile.TemporaryDirectory() as temporary:
        folder = Path(temporary)
        program = folder / "dump.exe"
        subprocess.run(["g++", "-std=c++23", "-O2", "-DNDEBUG", f"-I{SOURCES / 'src'}",
                        f"-I{SOURCES / 'include'}",
                        str(ROOT / "tools" / "sub0h264_host_dump.cpp"), "-o", str(program)], check=True)
        decoded = folder / "sub0h264.yuv"
        reference = folder / "ffmpeg.yuv"
        run = subprocess.run([str(program), str(args.sample), str(decoded), str(args.width), str(args.height)],
                             check=True, capture_output=True, text=True)
        print("sub0h264", run.stdout.strip())
        subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", str(args.sample),
                        "-vf", f"crop={args.width}:{args.height}:0:0", "-f", "rawvideo",
                        "-pix_fmt", "yuv420p", str(reference)], check=True)
        left = decoded.read_bytes()
        right = reference.read_bytes()
    count = min(len(left), len(right)) // frame_bytes
    bad = []
    worst = 99.0
    for index in range(count):
        a = left[index * frame_bytes:(index + 1) * frame_bytes]
        b = right[index * frame_bytes:(index + 1) * frame_bytes]
        if a != b:
            bad.append(index)
            worst = min(worst, luma_psnr(a, b, luma_bytes))
    print(f"sub0h264_frames={len(left) // frame_bytes} ffmpeg_frames={len(right) // frame_bytes}")
    print(f"different={len(bad)}/{count} first={bad[0] if bad else 'none'} worst_luma_psnr={worst:.1f}dB")
    sys.exit(1 if bad or len(left) != len(right) else 0)


if __name__ == "__main__":
    main()
