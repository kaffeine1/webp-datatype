#!/usr/bin/env python3
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes the DataTypes descriptor that makes the system recognise WEBP.

    make-descriptor.py <output file> [--version "45.1 (12.11.2026)"]
    make-descriptor.py --selftest

The file goes to DEVS:DataTypes/WebP on AmigaOS 3.x, AmigaOS 4 and MorphOS.
It is an IFF FORM DTYP, big endian on every machine:

    NAME   the descriptor's name
    FVER   a version string, so that the Version command can read it
    DTHD   the DataTypeHeader: offsets (from the start of the chunk) of the
           name, the base name, the name pattern and the mask, then the
           group, the ID, the mask length, the flags and the priority;
           after it the mask words and the three strings

A WEBP file starts with "RIFF", four bytes of size, then "WEBP". The mask
has one word for each byte to compare: the byte's value, or 0xFFFF for a
byte that may be anything. The name pattern is "#?": the first twelve
bytes decide, whatever the file is called, so a sticker saved without an
extension, or one handed over from memory, is still recognised.
"""

import struct
import sys

DTF_BINARY = 0x0000
DTF_CASE = 0x0010
ANY = 0xFFFF


def chunk(chunk_id, data):
    return chunk_id + struct.pack(">I", len(data)) + data + (b"\0" if len(data) & 1 else b"")


def descriptor(name, version, base_name, pattern, mask, group, type_id, flags,
               priority):
    header_size = 32
    mask_at = header_size
    name_at = mask_at + 2 * len(mask)
    base_at = name_at + len(name) + 1
    pattern_at = base_at + len(base_name) + 1
    dthd = struct.pack(">IIII4s4sHHHH", name_at, base_at, pattern_at, mask_at,
                       group, type_id, len(mask), 0, flags, priority)
    dthd += b"".join(struct.pack(">H", word) for word in mask)
    dthd += name + b"\0" + base_name + b"\0" + pattern + b"\0"
    body = b"DTYP" + chunk(b"NAME", name + b"\0")
    if version:
        body += chunk(b"FVER", version + b"\0")
    body += chunk(b"DTHD", dthd)
    return b"FORM" + struct.pack(">I", len(body)) + body


WEBP_MASK = [ord(c) for c in "RIFF"] + [ANY] * 4 + [ord(c) for c in "WEBP"]


def webp(version):
    return descriptor(b"WebP", b"$VER: WebP " + version.encode("ascii"), b"webp",
                      b"#?", WEBP_MASK, b"pict", b"webp", DTF_BINARY | DTF_CASE, 0)


def selftest():
    """The same writer must give, byte for byte, the WebP descriptor that an
    AROS build makes with its own tool from the same fields."""
    aros = bytes.fromhex(
        "464f524d0000008a445459504e414d4500000005576562500000465645520000"
        "001d245645523a20576562502034352e30202831322e30322e32303230290000"
        "445448440000004a000000380000003d00000042000000207069637477656270"
        "000c0000001000010052004900460046ffffffffffffffff0057004500420050"
        "57656250007765627000233f2e7765627000")
    mine = descriptor(b"WebP", b"$VER: WebP 45.0 (12.02.2020)", b"webp",
                      b"#?.webp", WEBP_MASK, b"pict", b"webp",
                      DTF_BINARY | DTF_CASE, 1)
    if mine != aros:
        print("selftest FAILED", file=sys.stderr)
        return 1
    print("selftest passed")
    return 0


def main():
    args = sys.argv[1:]
    if args == ["--selftest"]:
        return selftest()
    version = "45.1 (12.11.2026)"
    if "--version" in args:
        at = args.index("--version")
        version = args[at + 1]
        del args[at:at + 2]
    if len(args) != 1:
        print(__doc__, file=sys.stderr)
        return 2
    with open(args[0], "wb") as f:
        f.write(webp(version))
    return 0


if __name__ == "__main__":
    sys.exit(main())
