#!/usr/bin/env python3
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Draws the test pictures and encodes them as WEBP.

    python3 tests/make-pictures.py

Needs Pillow and the libwebp tools (cwebp, img2webp, webpmux) on the PATH.
Every picture is drawn here, from numbers: nothing is taken from anywhere.
The WEBP files land in tests/pictures, together with the PNG that an
animation's first frame must decode to. The drawing is seeded, so a second
run draws the same pictures (the encoders may still differ between
versions, which is why the files are kept in the repository).
"""

import math
import os
import random
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "pictures")


def run(*args):
    subprocess.run(args, check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)


def landscape(width, height):
    """A sky, a sun, hills and some grain: what a lossy coder is made for."""
    rnd = random.Random(1985)
    img = Image.new("RGB", (width, height))
    px = img.load()
    for y in range(height):
        t = y / (height - 1)
        for x in range(width):
            px[x, y] = (int(40 + 150 * t), int(90 + 110 * t), int(190 - 40 * t))
    draw = ImageDraw.Draw(img)
    cx, cy, r = int(width * 0.72), int(height * 0.3), height // 7
    for i in range(r, 0, -1):
        k = i / r
        draw.ellipse((cx - i, cy - i, cx + i, cy + i),
                     fill=(255, int(250 - 60 * k), int(200 - 150 * k)))
    for layer, (base, colour) in enumerate(((0.62, (60, 110, 70)),
                                            (0.74, (40, 85, 50)),
                                            (0.86, (25, 60, 35)))):
        points = [(0, height)]
        for x in range(0, width + 8, 8):
            y = height * base + math.sin(x / (23.0 + 9 * layer) + layer) * height * 0.06
            points.append((x, y))
        points.append((width, height))
        draw.polygon(points, fill=colour)
    px = img.load()
    for y in range(height):
        for x in range(width):
            n = rnd.randint(-6, 6)
            r_, g_, b_ = px[x, y]
            px[x, y] = (max(0, min(255, r_ + n)), max(0, min(255, g_ + n)),
                        max(0, min(255, b_ + n)))
    return img


def chart(width, height):
    """Flat colours, hard edges and a few thin lines: a lossless subject."""
    img = Image.new("RGB", (width, height), (245, 245, 240))
    draw = ImageDraw.Draw(img)
    colours = [(220, 50, 47), (38, 139, 210), (133, 153, 0), (181, 137, 0),
               (108, 113, 196), (42, 161, 152), (211, 54, 130), (0, 0, 0)]
    cell = width // 8
    for i, colour in enumerate(colours):
        draw.rectangle((i * cell, 0, (i + 1) * cell - 1, height // 4), fill=colour)
    for i in range(16):
        g = i * 17
        draw.rectangle((i * (width // 16), height // 4, (i + 1) * (width // 16) - 1,
                        height // 4 + height // 8), fill=(g, g, g))
    for i in range(0, width, 4):
        draw.line((i, height // 2, i, height * 5 // 8), fill=(0, 0, 0))
    for i in range(0, height // 4, 2):
        draw.line((0, height * 5 // 8 + i, width // 2, height * 5 // 8 + i),
                  fill=(0, 0, 0))
    draw.ellipse((width // 2 + 10, height * 5 // 8 + 6, width - 10, height - 8),
                 outline=(220, 50, 47), width=3)
    draw.text((8, height - 22), "WEBP lossless 256", fill=(0, 0, 0))
    return img


def sticker(size):
    """A face with a white edge and a soft shadow on a transparent ground."""
    scale = 2
    big = size * scale
    shape = Image.new("L", (big, big), 0)
    d = ImageDraw.Draw(shape)
    m = big // 10
    d.ellipse((m, m, big - m, big - m), fill=255)
    edge = shape.filter(ImageFilter.MaxFilter(scale * 12 + 1))
    shadow = edge.filter(ImageFilter.GaussianBlur(scale * 6))

    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    black = Image.new("RGBA", (big, big), (0, 0, 0, 255))
    offset = Image.new("L", (big, big), 0)
    offset.paste(shadow.point(lambda v: v * 110 // 255), (scale * 4, scale * 8))
    img.paste(black, (0, 0), offset)
    img.paste(Image.new("RGBA", (big, big), (255, 255, 255, 255)), (0, 0), edge)
    face = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    fd = ImageDraw.Draw(face)
    for i in range(big // 2 - m, 0, -scale):
        k = i / (big / 2 - m)
        fd.ellipse((big // 2 - i, big // 2 - i, big // 2 + i, big // 2 + i),
                   fill=(255, int(215 - 60 * k), int(70 - 40 * k), 255))
    eye = big // 12
    for ex in (big * 0.36, big * 0.64):
        fd.ellipse((ex - eye, big * 0.38 - eye * 1.3, ex + eye, big * 0.38 + eye * 1.3),
                   fill=(60, 40, 30, 255))
        fd.ellipse((ex - eye * 0.2, big * 0.38 - eye * 0.9, ex + eye * 0.5,
                    big * 0.38 - eye * 0.2), fill=(255, 255, 255, 255))
    fd.arc((big * 0.3, big * 0.4, big * 0.7, big * 0.76), 20, 160,
           fill=(120, 50, 30, 255), width=scale * 7)
    img.paste(face, (0, 0), shape)
    return img.resize((size, size), Image.LANCZOS)


def ball_frame(size, step, steps):
    """One frame of a ball going round, on a transparent ground."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rectangle((0, 0, size - 1, size - 1), outline=(38, 139, 210, 255), width=4)
    a = 2 * math.pi * step / steps
    cx = size / 2 + math.cos(a) * size * 0.3
    cy = size / 2 + math.sin(a) * size * 0.3
    r = size * 0.12
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(220, 50, 47, 255))
    d.text((10, 10), "frame %d" % (step + 1), fill=(0, 0, 0, 255))
    return img


def main():
    os.makedirs(OUT, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix="wdt-pictures-")

    def png(name, img):
        path = os.path.join(tmp, name)
        img.save(path)
        return path

    def out(name):
        return os.path.join(OUT, name)

    # 1. Lossy, no alpha (a bare VP8 stream in a RIFF).
    run("cwebp", "-quiet", "-q", "75", png("landscape.png", landscape(320, 240)),
        "-o", out("lossy.webp"))

    # 2. Lossless, no alpha (VP8L).
    run("cwebp", "-quiet", "-lossless", "-z", "9", png("chart.png", chart(256, 256)),
        "-o", out("lossless.webp"))

    # 3 and 4. What Telegram sends as a sticker: 512 x 512 with alpha.
    #    Lossy colour with a separate alpha plane (VP8X + ALPH + VP8), and
    #    the same picture lossless (VP8L with alpha).
    s = png("sticker.png", sticker(512))
    run("cwebp", "-quiet", "-q", "80", "-alpha_q", "90", s, "-o",
        out("alpha-lossy.webp"))
    run("cwebp", "-quiet", "-lossless", "-exact", s, "-o", out("alpha-lossless.webp"))

    # 5. Odd sides and alpha: the last column and row have half a chroma sample.
    small = sticker(64).crop((3, 9, 40, 38))
    run("cwebp", "-quiet", "-q", "70", png("odd.png", small), "-o",
        out("odd-37x29.webp"))

    # 6. An animation, lossless, six frames filling the canvas.
    frames = [png("ball%d.png" % i, ball_frame(160, i, 6)) for i in range(6)]
    args = ["img2webp", "-loop", "0", "-lossless", "-exact"]
    for f in frames:
        args += ["-d", "120", f]
    run(*args, "-o", out("animated.webp"))
    ball_frame(160, 0, 6).save(out("animated.frame1.png"))

    # 7. An animation whose first frame is smaller than the canvas and does
    #    not start in its corner. Frame offsets are even by format.
    part = sticker(96).crop((8, 8, 88, 72))           # 80 x 64
    whole = ball_frame(200, 2, 6)                     # 200 x 200
    f1 = os.path.join(tmp, "part.webp")
    f2 = os.path.join(tmp, "whole.webp")
    run("cwebp", "-quiet", "-lossless", "-exact", png("part.png", part), "-o", f1)
    run("cwebp", "-quiet", "-lossless", "-exact", png("whole.png", whole), "-o", f2)
    run("webpmux", "-frame", f1, "+150+60+90+0-b", "-frame", f2, "+150+0+0+0-b",
        "-loop", "0", "-bgcolor", "255,255,255,255", "-o",
        out("animated-offset.webp"))
    canvas = Image.new("RGBA", (200, 200), (0, 0, 0, 0))
    canvas.paste(part, (60, 90))
    canvas.save(out("animated-offset.frame1.png"))

    for name in sorted(os.listdir(OUT)):
        print("%8d  %s" % (os.path.getsize(out(name)), name))


if __name__ == "__main__":
    sys.exit(main())
