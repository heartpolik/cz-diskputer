#!/usr/bin/env python3
"""Sparse fake disc images for the README screenshots: real headers, no data.

usage: make-demo-images.py DIR"""
import os, sys

SECTOR = 2048

def iso(path, size, label, boot=True, mbr=False, gpt=False, udf=False):
    head = bytearray(64 * 1024)
    s = 16
    pvd = bytearray(SECTOR); pvd[0] = 1; pvd[1:6] = b"CD001"
    pvd[40:72] = label.encode().ljust(32)
    head[s * SECTOR:(s + 1) * SECTOR] = pvd; s += 1
    if boot:
        br = bytearray(SECTOR); br[0] = 0; br[1:6] = b"CD001"; br[7:30] = b"EL TORITO SPECIFICATION"
        head[s * SECTOR:(s + 1) * SECTOR] = br; s += 1
    t = bytearray(SECTOR); t[0] = 255; t[1:6] = b"CD001"
    head[s * SECTOR:(s + 1) * SECTOR] = t; s += 1
    if udf:
        for tag in (b"BEA01", b"NSR02", b"TEA01"):
            head[s * SECTOR + 1:s * SECTOR + 6] = tag; s += 1
    if mbr:
        head[510:512] = b"\x55\xaa"; head[446 + 4] = 0x17
    if gpt:
        head[512:520] = b"EFI PART"
    write(path, size, head)

def disk(path, size):
    head = bytearray(4096)
    head[510:512] = b"\x55\xaa"; head[446 + 4] = 0x0c
    write(path, size, head)

def write(path, size, head):
    with open(path, "wb") as f:
        f.write(head)
        f.truncate(size)  # sparse

d = sys.argv[1]
os.makedirs(d, exist_ok=True)
iso(f"{d}/debian-13.2.0-amd64-netinst.iso", 784_000_000, "Debian 13.2.0 amd64 n", mbr=True, gpt=True)
iso(f"{d}/Win11_24H2_English_x64.iso", 5_840_000_000, "CCCOMA_X64FRE_EN-US_DV9", udf=True)
iso(f"{d}/ubuntu-24.04.3-desktop-amd64.iso", 6_200_000_000, "Ubuntu 24.04.3 LTS amd64", mbr=True, gpt=True)
iso(f"{d}/FD14LIVE.iso", 395_000_000, "FD14-LIVECD")
iso(f"{d}/drivers.iso", 120_000_000, "DRIVERS", boot=False)
disk(f"{d}/raspios-trixie-arm64-lite.img", 2_900_000_000)
open(f"{d}/quake2.cue", "w").write('FILE "quake2.bin" BINARY\n')
