#!/usr/bin/env python3
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compares a PAM (RGBA) with a PNG or with another PAM.

    compare-pam-png.py <a.pam> <b.png|b.pam>            exact
    compare-pam-png.py --close <a.pam> <b.png|b.pam>    a is a 256 colour
                                                        rendering of b

Exact: every pixel that is not fully transparent must be the same, and
alpha must be the same everywhere (what lies under a fully transparent
pixel is nobody's business).

Close: for a picture cut down to a palette and dithered. A pixel is see
through in one exactly when it is less than half opaque in the other, and
over the opaque pixels the average difference per channel, after a small
blur that undoes the dithering, stays under a few levels.
"""

import sys

from PIL import Image, ImageChops, ImageFilter, ImageStat


def load(path):
    if path.endswith(".pam"):
        with open(path, "rb") as f:
            data = f.read()
        head, _, body = data.partition(b"ENDHDR\n")
        fields = dict(line.split(None, 1) for line in head.decode().splitlines()[1:])
        size = (int(fields["WIDTH"]), int(fields["HEIGHT"]))
        return Image.frombytes("RGBA", size, body)
    return Image.open(path).convert("RGBA")


def main():
    args = sys.argv[1:]
    close = args and args[0] == "--close"
    if close:
        args = args[1:]
    a, b = load(args[0]), load(args[1])
    if a.size != b.size:
        print("sizes differ: %s and %s" % (a.size, b.size), file=sys.stderr)
        return 1

    if not close:
        if a.getchannel("A").tobytes() != b.getchannel("A").tobytes():
            print("alpha differs", file=sys.stderr)
            return 1
        clear = Image.new("RGBA", a.size, (0, 0, 0, 0))
        mask = a.getchannel("A").point(lambda v: 255 if v else 0)
        a = Image.composite(a, clear, mask)
        b = Image.composite(b, clear, mask)
        if a.tobytes() != b.tobytes():
            print("colours differ", file=sys.stderr)
            return 1
        return 0

    hole_a = a.getchannel("A").point(lambda v: 255 if v < 128 else 0)
    hole_b = b.getchannel("A").point(lambda v: 255 if v < 128 else 0)
    if hole_a.tobytes() != hole_b.tobytes():
        print("the see through pixels differ", file=sys.stderr)
        return 1
    white = Image.new("RGB", a.size, (255, 255, 255))
    flat_a = Image.composite(a.convert("RGB"), white, a.getchannel("A"))
    flat_b = Image.composite(b.convert("RGB"), white, b.getchannel("A"))
    blur = ImageFilter.GaussianBlur(1.5)
    diff = ImageChops.difference(flat_a.filter(blur), flat_b.filter(blur))
    solid = hole_b.point(lambda v: 255 - v)
    mean = ImageStat.Stat(diff, solid).mean if solid.getbbox() else [0, 0, 0]
    worst = max(mean)
    print("  mean difference per channel %.2f" % worst)
    return 0 if worst < 4.0 else 1


if __name__ == "__main__":
    sys.exit(main())
