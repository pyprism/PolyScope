#!/usr/bin/env python3
"""Build a multi-size .ico (PNG-in-ICO) from a square PNG using only the stdlib + sips-free resizing.

Usage: make_ico.py SIZE_PNG... OUT.ico   (pass pre-resized PNGs, e.g. 16 32 48 64 128 256)
"""
import struct
import sys
from pathlib import Path


def png_size(data: bytes) -> int:
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    return struct.unpack(">I", data[16:20])[0]


def main() -> None:
    *inputs, out = sys.argv[1:]
    images = [Path(p).read_bytes() for p in inputs]
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries, blobs = b"", b""
    for data in images:
        size = png_size(data)
        entries += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(data), offset)
        blobs += data
        offset += len(data)
    Path(out).write_bytes(header + entries + blobs)


if __name__ == "__main__":
    main()
