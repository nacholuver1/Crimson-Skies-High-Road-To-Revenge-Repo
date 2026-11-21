"""Minimal XDVDFS (Xbox / Xbox 360 game disc) extractor.

Usage: python extract_xiso.py <image.iso> <out_dir> [--list]
"""
import os
import struct
import sys

SECTOR = 0x800
MAGIC = b"MICROSOFT*XBOX*MEDIA"
# Known game-partition offsets: plain xiso, XGD1 (original Xbox redump), XGD2, XGD3
PARTITION_OFFSETS = [0x0, 0x18300000, 0xFD90000, 0x89D80000, 0x2080000]


def find_partition(f):
    for off in PARTITION_OFFSETS:
        f.seek(off + 0x10000)
        if f.read(len(MAGIC)) == MAGIC:
            return off
    raise SystemExit("No XDVDFS volume descriptor found")


def walk(f, base, dir_sector, dir_size, path, out):
    data_off = base + dir_sector * SECTOR
    f.seek(data_off)
    table = f.read(dir_size)
    seen = set()
    stack = [0]
    while stack:
        off = stack.pop()
        if off in seen or off + 14 > len(table):
            continue
        seen.add(off)
        left, right, sector, size, attr, nlen = struct.unpack_from("<HHIIBB", table, off)
        if left == 0xFFFF:  # padding / empty slot
            continue
        name = table[off + 14:off + 14 + nlen].decode("ascii", "replace")
        if left:
            stack.append(left * 4)
        if right:
            stack.append(right * 4)
        full = os.path.join(path, name)
        if attr & 0x10:
            out.append((full, None, None))
            if size:
                walk(f, base, sector, size, full, out)
        else:
            out.append((full, base + sector * SECTOR, size))


def main():
    iso, out_dir = sys.argv[1], sys.argv[2]
    list_only = "--list" in sys.argv
    with open(iso, "rb") as f:
        base = find_partition(f)
        f.seek(base + 0x10000 + 0x14)
        root_sector, root_size = struct.unpack("<II", f.read(8))
        entries = []
        walk(f, base, root_sector, root_size, "", entries)
        for rel, off, size in sorted(entries):
            if off is None:
                if not list_only:
                    os.makedirs(os.path.join(out_dir, rel), exist_ok=True)
                continue
            print(f"{size:>12}  {rel}")
            if list_only:
                continue
            dst = os.path.join(out_dir, rel)
            os.makedirs(os.path.dirname(dst) or out_dir, exist_ok=True)
            f.seek(off)
            remaining = size
            with open(dst, "wb") as o:
                while remaining:
                    chunk = f.read(min(remaining, 1 << 24))
                    if not chunk:
                        raise SystemExit(f"Short read on {rel}")
                    o.write(chunk)
                    remaining -= len(chunk)


if __name__ == "__main__":
    main()
