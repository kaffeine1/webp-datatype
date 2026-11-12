#!/usr/bin/env python3
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prints the checksum dtprobe prints, for PAM files on this machine.

    pam-checksum.py <file.pam>...

FNV-1a, 32 bits, over the pixel bytes after the PAM header. Run it on what
dwebp -pam decodes (or on what webpcheck planar writes, for a system with an
old picture.datatype) and compare with the line dtprobe prints on the Amiga.
"""

import sys


def checksum(path):
    with open(path, "rb") as f:
        body = f.read().partition(b"ENDHDR\n")[2]
    value = 2166136261
    for byte in body:
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value, len(body)


for name in sys.argv[1:]:
    value, size = checksum(name)
    print("%08X  %8d bytes  %s" % (value, size, name))
