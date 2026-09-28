"""The largest image of a Windows .ico as a PNG, with nothing but the standard
library (a CI runner's python3 has no Pillow).

    python3 tools/apple/ico_to_png.py <in.ico> <out.png>

tools/apple/make_app.sh makes the app icons from the original's penumbra.ico
this way: four 32-bit images, 48 px the largest. An entry that is already a
PNG is written as it is; a 32-bit DIB is turned right way up and from BGRA to
RGBA. Other depths are refused (the original has none).
"""

import struct
import sys
import zlib


def png_chunk(tag, payload):
    return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)


def main(argv):
    if len(argv) != 3:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    data = open(argv[1], "rb").read()
    _, kind, count = struct.unpack("<HHH", data[:6])
    if kind != 1 or count == 0:
        print(f"{argv[1]}: not an icon", file=sys.stderr)
        return 1

    best = None
    for i in range(count):
        width, height, _, _, _, bpp, size, offset = struct.unpack("<BBBBHHII", data[6 + 16 * i : 22 + 16 * i])
        width = width or 256
        if best is None or width > best[0]:
            best = (width, bpp, size, offset)
    _, _, size, offset = best
    image = data[offset : offset + size]

    if image[:8] == b"\x89PNG\r\n\x1a\n":
        open(argv[2], "wb").write(image)
        return 0

    header_size, width, double_height, _, bit_count, compression = struct.unpack("<IiiHHI", image[:20])
    if bit_count != 32 or compression != 0:
        print(f"{argv[1]}: only uncompressed 32-bit images are handled, not {bit_count}-bit", file=sys.stderr)
        return 1
    height = double_height // 2   # the colour image, then the AND mask
    stride = width * 4
    pixels = image[header_size : header_size + stride * height]
    # An icon that never used its alpha channel leaves it all zero; opaque then.
    opaque = max(pixels[3::4], default=0) == 0

    rows = []
    for y in range(height):
        source = pixels[(height - 1 - y) * stride : (height - y) * stride]   # stored bottom-up
        row = bytearray(b"\x00")   # PNG filter: none
        for x in range(width):
            b, g, r, a = source[4 * x : 4 * x + 4]
            row += bytes((r, g, b, 255 if opaque else a))
        rows.append(bytes(row))

    png = (b"\x89PNG\r\n\x1a\n"
           + png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
           + png_chunk(b"IDAT", zlib.compress(b"".join(rows), 9))
           + png_chunk(b"IEND", b""))
    open(argv[2], "wb").write(png)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
