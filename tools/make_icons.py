#!/usr/bin/env python3
"""Builds the app icons from hptracker_icon.png (macOS only: uses sips/iconutil).

    python3 tools/make_icons.py

hptracker_icon.png is a finished icon: a rounded tile with the artwork on it and
a transparent surround. It is only resized and placed per platform:

  windows/icon/hptracker.ico    16..256 px, tile fills the canvas
  macos/icon/hptracker.icns     16..1024 px, Apple's icon grid (margin + soft shadow)
  windows/icon/hptracker_1024.png, macos/icon/hptracker_1024.png   the masters
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = os.path.join(ROOT, "hptracker_icon.png")
SIZE = 1024

# Tile width in the 1024 canvas, and whether to add a soft drop shadow.
WINDOWS = dict(tile=1000, shadow=False)
MACOS = dict(tile=824, shadow=True)    # Apple's icon grid: 824 of 1024
ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]


def read_png(path):
    """Decodes an 8-bit RGBA PNG into (width, height, rows of RGBA bytes)."""
    data = open(path, "rb").read()
    pos, idat = 8, b""
    while pos < len(data):
        n, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        pos += 12 + n
        if kind == b"IHDR":
            w, h, depth, ctype = struct.unpack(">IIBB", body[:10])
            assert depth == 8 and ctype == 6, "expected an 8-bit RGBA PNG"
        elif kind == b"IDAT":
            idat += body
    raw = zlib.decompress(idat)
    stride, bpp = w * 4, 4
    rows, prev, p = [], bytearray(w * 4), 0
    for _ in range(h):
        f = raw[p]
        line = bytearray(raw[p + 1:p + 1 + stride])
        p += 1 + stride
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 255
        rows.append(line)
        prev = line
    return w, h, rows


def write_png(path, w, h, rows):
    def chunk(kind, body):
        c = struct.pack(">I", len(body)) + kind + body
        return c + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + bytes(r) for r in rows)
    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
    open(path, "wb").write(png)


def tile_bounds(w, h, rows):
    """(x0, y0, x1, y1) of the opaque tile, from its widest row and column."""
    mid = rows[h // 2][3::4]
    xs = [x for x in range(w) if mid[x] > 128]
    ys = [y for y in range(h) if rows[y][(w // 2) * 4 + 3] > 128]
    return xs[0], ys[0], xs[-1] + 1, ys[-1] + 1


def box_blur(plane, w, h, radius):
    """One box-blur pass over a flat list of floats, horizontally then vertically."""
    def blur_line(line):
        n = len(line)
        out = [0.0] * n
        acc = sum(line[:radius + 1])
        count = radius + 1
        for i in range(n):
            out[i] = acc / count
            if i + radius + 1 < n:
                acc += line[i + radius + 1]
                count += 1
            if i - radius >= 0:
                acc -= line[i - radius]
                count -= 1
        return out
    rows = [blur_line(plane[y * w:(y + 1) * w]) for y in range(h)]
    cols = [blur_line([rows[y][x] for y in range(h)]) for x in range(w)]
    out = [0.0] * (w * h)
    for x in range(w):
        col = cols[x]
        for y in range(h):
            out[y * w + x] = col[y]
    return out


def sips_resize(src, size, dst):
    subprocess.run(["sips", "-z", str(size), str(size), src, "--out", dst],
                   check=True, capture_output=True)


def render_master(tmp, name, tile, shadow):
    """Places the tile, `tile` px wide and centered, on a transparent 1024 canvas."""
    sw, sh, srows = read_png(SOURCE)
    x0, y0, x1, y1 = tile_bounds(sw, sh, srows)
    scale = tile / float(x1 - x0)
    art_px = round(sw * scale)
    scaled = os.path.join(tmp, name + "_art.png")
    sips_resize(SOURCE, art_px, scaled)
    aw, ah, art = read_png(scaled)
    bx0, by0, bx1, by1 = tile_bounds(aw, ah, art)
    ox = SIZE // 2 - (bx0 + bx1) // 2
    oy = SIZE // 2 - (by0 + by1) // 2

    canvas = [bytearray(SIZE * 4) for _ in range(SIZE)]
    for y in range(ah):
        dy = y + oy
        if not 0 <= dy < SIZE:
            continue
        lo, hi = max(0, -ox), min(aw, SIZE - ox)
        canvas[dy][(lo + ox) * 4:(hi + ox) * 4] = art[y][lo * 4:hi * 4]

    if shadow:
        # Soft drop shadow: the tile's alpha, shifted down and blurred.
        drop = 16
        alpha = [0.0] * (SIZE * SIZE)
        for y in range(SIZE - drop):
            a = canvas[y][3::4]
            base = (y + drop) * SIZE
            for x in range(SIZE):
                alpha[base + x] = a[x] / 255.0
        for _ in range(3):
            alpha = box_blur(alpha, SIZE, SIZE, 9)
        for y in range(SIZE):
            row = canvas[y]
            base = y * SIZE
            for x in range(SIZE):
                a_s = alpha[base + x] * 0.36
                if a_s <= 0.002:
                    continue
                i = x * 4
                a_t = row[i + 3] / 255.0
                out_a = a_t + a_s * (1.0 - a_t)
                row[i] = row[i + 1] = row[i + 2] = int(row[i] * a_t / out_a)
                row[i + 3] = int(out_a * 255)
    return canvas


def main():
    if not os.path.exists(SOURCE):
        sys.exit("missing " + SOURCE)
    tmp = tempfile.mkdtemp()
    try:
        masters = {}
        for name, cfg in (("windows", WINDOWS), ("macos", MACOS)):
            out_dir = os.path.join(ROOT, name, "icon")
            os.makedirs(out_dir, exist_ok=True)
            rows = render_master(tmp, name, **cfg)
            master = os.path.join(out_dir, "hptracker_1024.png")
            write_png(master, SIZE, SIZE, rows)
            masters[name] = master

        # Windows: ICO of PNG images (Vista and later).
        images = []
        for s in ICO_SIZES:
            p = os.path.join(tmp, "w%d.png" % s)
            sips_resize(masters["windows"], s, p)
            images.append((s, open(p, "rb").read()))
        offset = 6 + 16 * len(images)
        entries, blobs = b"", b""
        for s, blob in images:
            entries += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(blob), offset)
            blobs += blob
            offset += len(blob)
        ico = os.path.join(ROOT, "windows", "icon", "hptracker.ico")
        open(ico, "wb").write(struct.pack("<HHH", 0, 1, len(images)) + entries + blobs)
        print("wrote", os.path.relpath(ico, ROOT))

        # macOS: iconset -> icns.
        iconset = os.path.join(tmp, "hptracker.iconset")
        os.makedirs(iconset)
        for base in (16, 32, 128, 256, 512):
            sips_resize(masters["macos"], base, os.path.join(iconset, "icon_%dx%d.png" % (base, base)))
            sips_resize(masters["macos"], base * 2, os.path.join(iconset, "icon_%dx%d@2x.png" % (base, base)))
        icns = os.path.join(ROOT, "macos", "icon", "hptracker.icns")
        subprocess.run(["iconutil", "-c", "icns", iconset, "-o", icns], check=True)
        print("wrote", os.path.relpath(icns, ROOT))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
