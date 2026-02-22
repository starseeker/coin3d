#!/usr/bin/env python3
"""
rgb_to_png_py.py – Convert an SGI RGB image to PNG using Python stdlib only.

Usage: rgb_to_png_py.py input.rgb output.png

SGI RGB format (simplified, non-RLE, 8-bit per channel):
  Offset  Size  Description
  0       2     Magic number: 0x01DA
  2       1     Storage type: 0 = verbatim, 1 = RLE
  3       1     Bytes per channel (1 or 2)
  4       2     Number of dimensions (1-3)
  6       2     Width (x dimension)
  8       2     Height (y dimension)
  10      2     Depth / number of channels
  12      4     Minimum pixel value
  16      4     Maximum pixel value
  20      4     Reserved
  24      80    Image name (null-padded)
  104     4     Colormap type
  108     404   Reserved

Verbatim (non-RLE) pixel data follows the 512-byte header.
Pixels are stored in planar order: all R rows, then all G rows, then all B.
Rows are stored bottom-to-top (first row in file = bottom of image).

This converter outputs RGB PNG (no alpha) with standard deflate compression.
"""

import sys
import struct
import zlib


def read_sgi_rgb(path):
    with open(path, 'rb') as f:
        header = f.read(512)

    magic     = struct.unpack_from('>H', header, 0)[0]
    storage   = struct.unpack_from('B',  header, 2)[0]
    bpc       = struct.unpack_from('B',  header, 3)[0]
    ndim      = struct.unpack_from('>H', header, 4)[0]
    width     = struct.unpack_from('>H', header, 6)[0]
    height    = struct.unpack_from('>H', header, 8)[0]
    nchannels = struct.unpack_from('>H', header, 10)[0]

    if magic != 0x01DA:
        raise ValueError(f"Not an SGI RGB file (magic={magic:#06x})")
    if bpc != 1:
        raise ValueError(f"Only 8-bit SGI RGB supported (bpc={bpc})")

    with open(path, 'rb') as f:
        f.seek(512)
        raw = f.read()

    if storage == 0:
        # Verbatim storage
        plane_size = width * height
        channels = nchannels if nchannels >= 3 else 3
        planes = []
        for c in range(channels):
            off = c * plane_size
            planes.append(raw[off:off + plane_size])
    elif storage == 1:
        # RLE-compressed storage
        planes = _decode_rle(path, width, height, nchannels)
    else:
        raise ValueError(f"Unknown storage type {storage}")

    # Interleave RGB, flipping rows (SGI is bottom-to-top)
    r_plane = planes[0]
    g_plane = planes[1]
    b_plane = planes[2]
    pixels = bytearray(width * height * 3)
    for row in range(height):
        src_row = height - 1 - row   # flip
        for col in range(width):
            src = src_row * width + col
            dst = (row * width + col) * 3
            pixels[dst]     = r_plane[src]
            pixels[dst + 1] = g_plane[src]
            pixels[dst + 2] = b_plane[src]

    return width, height, bytes(pixels)


def _decode_rle(path, width, height, nchannels):
    """Decode RLE-compressed SGI RGB."""
    with open(path, 'rb') as f:
        f.seek(512)
        raw_header_ext = f.read()

    # RLE offset/length tables
    ntables = height * nchannels
    offsets = struct.unpack_from(f'>{ntables}I', raw_header_ext, 0)
    lengths = struct.unpack_from(f'>{ntables}I', raw_header_ext, ntables * 4)

    # Seek past the offset/length tables in the file
    with open(path, 'rb') as f:
        planes = []
        for c in range(nchannels):
            plane = bytearray(width * height)
            for row in range(height):
                idx = row + c * height
                f.seek(offsets[idx])
                rle_row = f.read(lengths[idx])
                decoded = _rle_decode_row(rle_row, width)
                plane[row * width:(row + 1) * width] = decoded
            planes.append(bytes(plane))
    return planes


def _rle_decode_row(data, width):
    out = bytearray()
    i = 0
    while i < len(data):
        pixel = data[i]; i += 1
        count = pixel & 0x7F
        if count == 0:
            break
        if pixel & 0x80:
            # Uncompressed run
            out.extend(data[i:i + count])
            i += count
        else:
            # Repeated run
            val = data[i]; i += 1
            out.extend([val] * count)
    return bytes(out[:width])


def write_png(path, width, height, rgb_data):
    """Write RGB pixel data as a PNG file using only stdlib."""
    def chunk(tag, data):
        length = struct.pack('>I', len(data))
        crc = struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)
        return length + tag + data + crc

    # PNG signature
    sig = b'\x89PNG\r\n\x1a\n'

    # IHDR: width, height, bit_depth=8, color_type=2 (RGB), ...
    ihdr_data = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)
    ihdr = chunk(b'IHDR', ihdr_data)

    # IDAT: scanlines prefixed by filter byte 0 (None), then deflated
    raw_rows = bytearray()
    row_bytes = width * 3
    for row in range(height):
        raw_rows.append(0)   # filter type None
        raw_rows.extend(rgb_data[row * row_bytes:(row + 1) * row_bytes])
    compressed = zlib.compress(bytes(raw_rows), 9)
    idat = chunk(b'IDAT', compressed)

    iend = chunk(b'IEND', b'')

    with open(path, 'wb') as f:
        f.write(sig + ihdr + idat + iend)


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} input.rgb output.png", file=sys.stderr)
        sys.exit(1)
    inpath, outpath = sys.argv[1], sys.argv[2]
    width, height, pixels = read_sgi_rgb(inpath)
    write_png(outpath, width, height, pixels)
    print(f"Converted {inpath} ({width}x{height}) -> {outpath}")


if __name__ == '__main__':
    main()
