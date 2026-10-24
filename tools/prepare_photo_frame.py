"""Convert one JPG/PNG into the bounded NPFP RGB565 photo-frame format.

Requires Pillow. The firmware reads np2/photos/photo001.npf .. photo064.npf
from the SD card. This tool never mounts, formats or erases a card.
"""
import argparse
import array
from pathlib import Path
import struct
import sys
import zlib

from PIL import Image, ImageOps

WIDTH, HEIGHT = 1024, 600
HEADER = struct.Struct("<4sHHHHIII")


def prepare(source: Path, destination: Path) -> None:
    if destination.exists():
        raise FileExistsError(f"Destination already exists: {destination}")
    with Image.open(source) as original:
        image = ImageOps.fit(ImageOps.exif_transpose(original).convert("RGB"),
                             (WIDTH, HEIGHT), Image.Resampling.LANCZOS)
        rgb = image.tobytes()
    pixels = array.array("H", (
        ((rgb[i] >> 3) << 11) | ((rgb[i + 1] >> 2) << 5) | (rgb[i + 2] >> 3)
        for i in range(0, len(rgb), 3)
    ))
    if sys.byteorder != "little":
        pixels.byteswap()
    payload = pixels.tobytes()
    assert len(payload) == WIDTH * HEIGHT * 2
    header = HEADER.pack(b"NPFP", 1, WIDTH, HEIGHT, 1, 0, len(payload),
                         zlib.crc32(payload))
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("xb") as output:
        output.write(header)
        output.write(payload)
    # Verify the saved bytes, not only the in-memory conversion.
    saved = destination.read_bytes()
    fields = HEADER.unpack(saved[:HEADER.size])
    if (len(saved) != HEADER.size + WIDTH * HEIGHT * 2 or
            fields[:7] != (b"NPFP", 1, WIDTH, HEIGHT, 1, 0, len(payload)) or
            zlib.crc32(saved[HEADER.size:]) != fields[7]):
        raise ValueError("Saved photo failed header/size/CRC verification")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    prepare(args.source, args.destination)
    print(f"Prepared and verified {args.destination} ({args.destination.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
