# webp.datatype

A picture datatype that reads WEBP files on AmigaOS 3.x, AmigaOS 4.1 and
MorphOS. Once it is installed, every program that opens pictures through
datatypes.library (MultiView, the Workbench, image viewers, anything that
asks the system for a picture) opens WEBP files too, without knowing
anything about the format.

It reads:

- lossy pictures (VP8),
- lossless pictures (VP8L),
- pictures with alpha, in both forms (a lossy picture with its ALPH plane,
  and a lossless one with alpha in the pixels),
- the first frame of an animation, placed on the animation's canvas.

It does not write WEBP files, and it shows only the first frame of an
animation.

The decoding is done by [libwebp](https://chromium.googlesource.com/webm/libwebp)
1.6.0, the reference library of the WebM project, built here as plain C
with no changes. The datatype around it is new code.

## Status

This is the first version, 45.1. It has been run in emulators on every
system in the table below and, with a test program, on three real
machines; `docs/CHECKS.md` says what was tried, on what, and what was not.
There is no packaged release yet: build it from the source, as described
under Building.

## Which systems

| System                    | File                           | Needs                          |
|---------------------------|--------------------------------|--------------------------------|
| AmigaOS 3.0 to 3.2 (68k)  | `build/os3/webp.datatype`      | a 68020 or better              |
| AmigaOS 4.1 (PowerPC)     | `build/os4/webp.datatype`      | AmigaOS 4.1                    |
| MorphOS 3.x (PowerPC)     | `build/morphos/webp.datatype`  | MorphOS 3                      |

AROS is not here on purpose: it has a `webp.datatype` of its own in the
system, on i386, x86_64 and ARM alike, and nobody should replace a system
file with this one.

On a 68000 the library refuses to open, so a WEBP file is simply not
recognised as a picture there, as if the datatype were not installed.

## Installing

Two files, on every system:

1. `webp.datatype` goes to `SYS:Classes/DataTypes/`.
2. `WebP` (the descriptor, from `build/descriptor/`) goes to
   `DEVS:DataTypes/`.

Then restart, or tell the system about the new descriptor right away:

    AddDataTypes REFRESH

To remove it, delete the two files and restart.

If another WEBP datatype is installed already, take it out first: two
descriptors for the same format leave the choice between them to the
system.

The descriptor recognises a file by its first twelve bytes ("RIFF", four
bytes of size, "WEBP"), not by its name, so a WEBP file without the
`.webp` extension is recognised as well.

## What each system gets

A datatype hands its pixels to the system's picture.datatype, and what
that one can take differs.

- **AmigaOS 4.1, MorphOS, and AmigaOS 3.2 with picture.datatype 47.9 or
  later**: true colour, and the alpha channel is kept. A program that reads
  the picture back as ARGB gets the alpha; a program that only shows it
  sees the transparent parts as the system draws them.
- **AmigaOS 3.5, 3.9, 3.1.4, an older 3.2, or 3.0/3.1 with the
  picture.datatype of a graphics card system (V43 to 47.8)**: true colour.
  These do not know about alpha, so a picture with alpha is laid on a
  white background first.
- **Plain AmigaOS 3.0/3.1 (picture.datatype V39 to V42)**: these only take
  a planar picture with a colour table. The datatype chooses the best 256
  colours for the picture (median cut) and dithers it (Floyd-Steinberg).
  Pixels that are less than half opaque become colour 0, which is marked
  as the transparent colour in the picture's header.

## Checking an installation

`dtprobe` (built next to the library, one for each system) opens a picture
through datatypes.library, the way any program does, and says what came
back:

    dtprobe pictures/alpha-lossy.webp

    picture.datatype 47.19
    datatype: WebP (base webp)
    opened in 0.24 s
    size 512 x 512, depth 32, masking 4, transparent 0
    PDTA_AlphaChannel: yes
    PDTM_READPIXELARRAY returned 1
    pixels: 1048576 bytes, checksum 1BC1C506

The checksum is of the pixels as they were read back. `docs/CHECKS.md` has
the table of what each test picture must give, taken from what libwebp's
own `dwebp` decodes, and says how each system was checked and on what.
`make kit` puts the library, the descriptor, `dtprobe` and the test
pictures of each built lane in one drawer under `build/kit`.

## Memory and speed

A picture is decoded at its full size, in one go. While it is being opened
the decoded pixels exist twice for a moment (once here, once in
picture.datatype): a 512 x 512 picture with alpha is 1 MB, so 2 MB for
that moment, plus the file. Pictures of more than 8192 x 8192 pixels, and
files of more than 64 MB, are refused.

The decoder runs on a stack of its own (64 KB on 68k, 128 KB on PowerPC),
so the program that opens the picture needs no large stack.

On a 68020 the decoding is not quick: it is the reference decoder in plain
C, with no assembly. On a stock A1200 (14 MHz, emulated cycle exact) a
512 x 512 lossy picture with alpha opens in 17 seconds, a lossless one in
9, a 320 x 240 lossy picture in under 5.

## Building

The host build and its tests need a C compiler, `make`, Python 3 with
Pillow, and the libwebp tools (`dwebp`, and `cwebp`, `img2webp`, `webpmux`
to draw the test pictures again).

    make                 the host test program
    make test            decode every test picture, compare with dwebp
    make test SAN=1      the same under the address and undefined sanitizers
    make descriptor      build/descriptor/WebP

    make os3             needs m68k-amigaos-gcc (the 6.5 "bebbo" toolchain)
    make morphos         needs ppc-morphos-gcc
    make os4             needs ppc-amigaos-gcc

The three libraries are built without a C library and without startup
code: `src/wdt_libc.c` holds the few C functions the decoder calls, on top
of exec.library, and libgcc supplies the 64 bit arithmetic.

## How it is put together

    src/wdt_decode.c    a WEBP file in memory in, RGB or ARGB out (libwebp)
    src/wdt_quant.c     true colour down to 256 colours, for old systems
    src/wdt_class.c     the class: OM_NEW reads, decodes, hands over
    src/wdt_libc.c      malloc, memcpy and friends without a C library
    src/os3/            the AmigaOS 3 library around the class
    src/mos/            the MorphOS one
    src/os4/            the AmigaOS 4 one
    descriptor/         writes the DEVS:DataTypes/WebP descriptor
    tools/dtprobe.c     opens a picture through datatypes.library and
                        saves what came back, to compare with dwebp
    tests/              the test pictures and the host tests
    third_party/libwebp the decoder, unchanged
    docs/CHECKS.md      how each lane was checked, and on what

`src/wdt_decode.c` and `src/wdt_quant.c` know nothing about AmigaOS, which
is what lets the host tests run the very code the datatype is made of:
every still test picture must decode to the same bytes as libwebp's own
`dwebp`, both animations to their first frame, and several thousand cut or
corrupted copies must be refused without a fault.

## Licence

The datatype is free software under the GNU General Public License,
version 3 or (at your option) any later version: see `COPYING`.

libwebp, in `third_party/libwebp`, is Copyright (c) 2010 Google Inc. and
is under its own BSD 3-clause licence, with a patent grant: see
`third_party/libwebp/COPYING` and `third_party/libwebp/PATENTS`.

Copyright (c) 2026 Michele Dipace.
