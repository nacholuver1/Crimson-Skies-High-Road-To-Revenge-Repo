"""Dump the header of an original-Xbox executable (default.xbe).

Usage: python xbe_info.py <default.xbe> [--sha256]

Prints the title ID / name / region from the certificate, the decoded entry point and
kernel-thunk address (retail or debug XOR key), every section, and the XDK library
versions. The library versions decide which XDK function signatures the recompiler /
HLE layer has to match, so record them for each game build.
"""
import hashlib
import struct
import sys

XOR_EP = {"retail": 0xA8FC57AB, "debug": 0x94859D4B}
XOR_KT = {"retail": 0x5B6D40B6, "debug": 0xEFB1F152}
XBE_BASE_HINT = 0x00010000


def u32(b, off):
    return struct.unpack_from("<I", b, off)[0]


def cstr(b, off):
    end = b.find(b"\0", off)
    return b[off:end if end >= 0 else len(b)].decode("ascii", "replace")


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    path = sys.argv[1]
    data = open(path, "rb").read()
    if data[:4] != b"XBEH":
        raise SystemExit("Not an XBE (missing 'XBEH' magic)")

    base = u32(data, 0x104)
    va = lambda a: a - base  # header-area virtual address -> file offset

    ep_raw, kt_raw = u32(data, 0x128), u32(data, 0x158)
    # The right key is the one that lands the entry point inside the image.
    img_size = u32(data, 0x10C)
    kind = next((k for k in ("retail", "debug")
                 if base <= ep_raw ^ XOR_EP[k] < base + img_size), "unknown")
    ep = ep_raw ^ XOR_EP.get(kind, 0)
    kt = kt_raw ^ XOR_KT.get(kind, 0)

    print(f"file            {path} ({len(data):,} bytes)")
    if "--sha256" in sys.argv:
        print(f"sha256          {hashlib.sha256(data).hexdigest()}")
    print(f"base address    0x{base:08X}" + ("" if base == XBE_BASE_HINT else "  (unusual)"))
    print(f"image size      0x{img_size:08X}")
    print(f"timestamp       0x{u32(data, 0x114):08X}")
    print(f"build type      {kind}")
    print(f"entry point     0x{ep:08X}")
    print(f"kernel thunk    0x{kt:08X}")
    print(f"init flags      0x{u32(data, 0x124):08X}")

    cert = va(u32(data, 0x118))
    title_id = u32(data, cert + 0x08)
    name = data[cert + 0x0C:cert + 0x0C + 80].decode("utf-16-le", "replace").split("\0")[0]
    pub = "".join(chr(c) if 32 <= c < 127 else "?" for c in ((title_id >> 24) & 0xFF, (title_id >> 16) & 0xFF))
    print(f"title id        0x{title_id:08X}  ({pub}-{title_id & 0xFFFF:03d})")
    print(f"title name      {name}")
    print(f"allowed media   0x{u32(data, cert + 0x9C):08X}")
    print(f"game region     0x{u32(data, cert + 0xA0):08X}")
    print(f"cert version    0x{u32(data, cert + 0xAC):08X}")

    nsec, sec = u32(data, 0x11C), va(u32(data, 0x120))
    print(f"\nsections ({nsec})")
    print(f"  {'name':<10} {'vaddr':>10} {'vsize':>10} {'raw':>10} {'rawsize':>10}  flags")
    for i in range(nsec):
        h = sec + i * 56
        flags, vaddr, vsize, raw, rsize, name_addr = struct.unpack_from("<6I", data, h)
        print(f"  {cstr(data, va(name_addr)):<10} 0x{vaddr:08X} 0x{vsize:08X} "
              f"0x{raw:08X} 0x{rsize:08X}  0x{flags:X}")

    nlib, lib = u32(data, 0x160), u32(data, 0x164)
    print(f"\nXDK libraries ({nlib})")
    if lib:
        for i in range(nlib):
            o = va(lib) + i * 16
            lname = data[o:o + 8].split(b"\0")[0].decode("ascii", "replace")
            major, minor, build, lflags = struct.unpack_from("<4H", data, o + 8)
            print(f"  {lname:<8} {major}.{minor}.{build}  flags 0x{lflags:04X}")


if __name__ == "__main__":
    main()
