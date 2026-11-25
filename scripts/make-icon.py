#!/usr/bin/env python3
"""Generates the DiscPuter icon PNG (default 100x100 RGBA) without PIL.

An optical disc with a USB-C plug under it, on the cardputer-zero-tools background.
usage: make-icon.py OUT.png [SIZE]"""
import math, struct, sys, zlib

W = H = int(sys.argv[2]) if len(sys.argv) > 2 else 100
U = W / 100.0
SS = 4
BG = (24, 26, 31); BORDER = (60, 64, 72)
DISC = (196, 202, 212); SHINE = (232, 236, 242); GROOVE = (150, 158, 170)
HUB = (44, 48, 56); ACCENT = (99, 226, 183); PLUG = (230, 196, 90)

def inside_round_rect(x, y, x0, y0, x1, y1, r):
    cx = min(max(x, x0 + r), x1 - r); cy = min(max(y, y0 + r), y1 - r)
    return (x - cx) ** 2 + (y - cy) ** 2 <= r * r

def color_at(x, y):
    if not inside_round_rect(x, y, 0, 0, 100, 100, 18):
        return None
    if not inside_round_rect(x, y, 2, 2, 98, 98, 16):
        return BORDER
    # USB-C plug below the disc
    if inside_round_rect(x, y, 38, 80, 62, 92, 6):
        if inside_round_rect(x, y, 42, 84, 58, 88, 2):
            return PLUG
        return HUB
    cx, cy = 50, 44
    r = math.hypot(x - cx, y - cy)
    if r <= 34:
        if r <= 5:
            return BG
        if r <= 12:
            return ACCENT if r > 9 else HUB
        a = math.atan2(y - cy, x - cx)
        if abs(math.sin(a * 1 + 0.9)) > 0.93 and r > 15:
            return SHINE
        if abs(r - 22) < 0.6 or abs(r - 29) < 0.6:
            return GROOVE
        return DISC
    return BG

rows = []
for py in range(H):
    row = bytearray([0])
    for px in range(W):
        acc = [0, 0, 0, 0]
        for sy in range(SS):
            for sx in range(SS):
                c = color_at((px + (sx + .5) / SS) / U, (py + (sy + .5) / SS) / U)
                if c:
                    acc[0] += c[0]; acc[1] += c[1]; acc[2] += c[2]; acc[3] += 255
        a = acc[3] // (SS * SS)
        row += bytes([acc[0] * 255 // acc[3], acc[1] * 255 // acc[3], acc[2] * 255 // acc[3], a]) if a else bytes(4)
    rows.append(bytes(row))

def chunk(t, d):
    return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)

png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0)) \
    + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b"")
open(sys.argv[1], "wb").write(png)
