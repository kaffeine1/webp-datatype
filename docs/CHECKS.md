# How each lane was checked

Everything below was run on 12 November 2026: first in emulators, then
`dtprobe` on three real machines, from a distance, and MultiView on one
of them. Nobody has yet sat in front of a real machine and looked at a
picture, so read this as what was tried, on what, and what came out.

The yardstick is always the same. libwebp's own `dwebp` decodes a file on
the build machine; the datatype decodes the same file on the Amiga; the
two sets of pixels must be the same bytes.

## The test pictures

`tests/make-pictures.py` draws them and encodes them with the libwebp
tools. They are in `tests/pictures`:

| File                   | What it exercises                                    |
|------------------------|------------------------------------------------------|
| `lossy.webp`           | 320 x 240, lossy (a bare VP8 stream)                 |
| `lossless.webp`        | 256 x 256, lossless (VP8L), 63 colours               |
| `alpha-lossy.webp`     | 512 x 512, lossy with an alpha plane (VP8X, ALPH)    |
| `alpha-lossless.webp`  | 512 x 512, lossless with alpha                       |
| `odd-37x29.webp`       | odd sides, lossy with alpha                          |
| `animated.webp`        | 160 x 160, six lossless frames                       |
| `animated-offset.webp` | 200 x 200, a first frame of 80 x 64 placed at 60, 90 |

The two 512 x 512 pictures are what a chat program receives as a sticker.

## The tools

- `tests/webpcheck.c` is the decoder without any datatype around it. `make
  test` runs it on the host; `make os3` also builds it for the 68020.
- `tools/dtprobe.c` runs on the Amiga. It opens a picture through
  datatypes.library like any program, says which datatype the system
  picked and what the picture's header holds, and prints a checksum of the
  pixels it reads back (FNV-1a, 32 bits, over the RGBA bytes). With a
  second argument it also saves the pixels as a PAM file.
- `tests/pam-checksum.py` prints the same checksum for a PAM file on the
  build machine.

The checksums a correct datatype must give:

| File                   | True colour | 256 colours | Laid on white |
|------------------------|-------------|-------------|---------------|
| `lossy.webp`           | `C00EB56B`  | `82DDEFE1`  | `C00EB56B`    |
| `lossless.webp`        | `01FDA869`  | `4149D1B6`  | `01FDA869`    |
| `alpha-lossy.webp`     | `1BC1C506`  | `86E128E0`  | `5D956474`    |
| `alpha-lossless.webp`  | `89287314`  | `BF32D682`  | `73D29DF9`    |
| `odd-37x29.webp`       | `A80B26F4`  | `9C811B67`  | `0FC67248`    |
| `animated.webp`        | `4AAC2476`  | `31318FCA`  | `E586B382`    |
| `animated-offset.webp` | `B60BB594`  | `A9FD47AF`  | `5A5F19B9`    |

"True colour" is what AmigaOS 4, MorphOS and AmigaOS 3.2 (picture.datatype
47.9 or later) must give. "256 colours" is for a plain AmigaOS 3.0/3.1.
"Laid on white" is for a true colour picture.datatype that does not know
about alpha (V43 to 47.8).

To check an installation, on the Amiga:

    dtprobe lossy.webp

and compare the `checksum` it prints with the table.

## Host (macOS, arm64)

`make test`, and again with `SAN=1` under the address and undefined
behaviour sanitizers:

- the five still pictures decode to the same bytes as `dwebp -pam`;
- both animations decode to their first frame on its canvas, compared with
  the PNG the drawing script saved;
- the pictures with alpha, laid on white, are within one level per channel
  of the same composite made by Pillow;
- the 256 colour version of every picture stays close to the original
  (mean difference per channel under 4 levels after a small blur, the
  transparent pixels in the same places);
- about 12000 cut or corrupted copies of the seven files are decoded or
  refused without a fault.

## AmigaOS 3.2.3, 68020 (WinUAE)

An A1200 profile with a 68EC020, 2 MB chip and 8 MB fast memory, AGA.
Kickstart 47.115, Workbench 47.5, picture.datatype 47.19,
datatypes.library 47.3. The system has no WEBP datatype of its own.

- The two files were copied to `SYS:Classes/DataTypes` and
  `DEVS:DataTypes` and the machine restarted: `LIBS:` held only the
  system's own directories, and WEBP files were recognised ("WebP, base
  webp"), also one copied to RAM: without an extension.
- `dtprobe` on the seven pictures: the pixels read back with
  PDTM_READPIXELARRAY are the same bytes as `dwebp`'s. The five pictures
  with alpha come back with depth 32, mskHasAlpha and PDTA_AlphaChannel
  set, the way the system's own png.datatype reports a PNG with alpha.
- The 68020 build of `webpcheck` writes the same files as the host build,
  in true colour and in 256 colours.
- MultiView on a 256 colour Workbench shows the pictures; the transparent
  part of a sticker shows the window behind it
  (`shots/amigaos-3.2.3-multiview-alpha.png`,
  `shots/amigaos-3.2.3-multiview-lossy.png`).
- Five damaged files (cut in half, random bytes changed in a lossy and in a
  lossless file, a header followed by noise, a header that claims 2 GB)
  are refused with error 2008 (DTERROR_INVALID_DATA) and a good picture
  opens right after.
- `Avail FLUSH` before and after a run of opens, good and bad: no memory
  lost from one run to the next, and after the flush the library loads
  again and works.
- On a 68000 (Kickstart 3.1) opening a WEBP file fails with error 2000 and
  the system carries on: the library refused to open.

Times, same system with the CPU emulated cycle exact (a stock A1200, 14
MHz): `lossy.webp` 4.6 s, `lossless.webp` 0.8 s, `alpha-lossy.webp` 17.0 s,
`alpha-lossless.webp` 9.2 s. That is the time NewDTObject takes.

## Kickstart 3.1 and Workbench 3.1, 68020 (WinUAE)

An A1200 profile with a 68EC020, 2 MB chip and 4 MB fast memory, AGA,
booted from the Workbench 3.1 floppy. Kickstart 40.68, Workbench 40.42,
picture.datatype 40.4, datatypes.library 40.6. Here the datatype and the
descriptor stayed outside the system (`Assign LIBS: <dir>/Classes ADD` and
`AddDataTypes <dir>/Devs/DataTypes/WebP`): a floppy has no room.

- This picture.datatype takes no true colour, so the 256 colour path runs.
  `dtprobe` reads the planar BitMap and the colour table back: for all
  seven pictures they are, byte for byte, what the host build of the same
  code makes. Pictures with fewer colours get fewer bit planes (6 for the
  63 colours of `lossless.webp`, 5 for `animated.webp`); pictures with
  alpha are marked mskHasTransparentColor with colour 0.
- MultiView on its own screen shows the picture with its own palette
  (`shots/workbench-3.1-multiview-own-screen.png`,
  `shots/workbench-3.1-multiview-alpha.png`). In a window on a 256 colour
  Workbench the same picture is coarser
  (`shots/workbench-3.1-multiview-window.png`): this picture.datatype
  shares pens between colours that are close, which merges neighbouring
  shades of a palette made of many close shades. That is the system's
  remapping, the same for any 256 colour picture.
- The five damaged files are refused with error 2008; no memory is lost
  over a run of opens.

## MorphOS 3.21 (QEMU, Pegasos 2)

MorphOS 3.21, Ambient 1.51, Kickstart 51.67, picture.datatype 50.19. No
WEBP datatype in the system. Files in RAM:, `Assign LIBS:` and
`AddDataTypes` as above; nothing was written to the system partition.

- `dtprobe` on the seven pictures: all seven checksums as in the table,
  and the PAM files sent back to the build machine are the same bytes as
  `dwebp`'s. Alpha is kept (depth 32, mskHasAlpha, PDTA_AlphaChannel).
- MultiView 3.17 says "WebP picture: 512 x 512 pixels, 32 bit depth" and
  blends the sticker, soft shadow included, over its window
  (`shots/morphos-3.21-multiview-alpha.png`).
- Two damaged files are refused with error 2008.
- Found here: this picture.datatype answers 0 to PDTM_WRITEPIXELARRAY even
  when it took every pixel (the one of AmigaOS 3.2 answers 1), so the
  class does not read anything into that answer.

## AmigaOS 4.1 Final Edition Update 3 (QEMU, Pegasos 2)

Kickstart 54.57, Workbench 53.21, picture.datatype 53.10. No WEBP datatype
in the system. Files in RAM:, as on MorphOS.

- `dtprobe` on the seven pictures: all seven checksums as in the table;
  `alpha-lossy.webp` also sent back as a PAM, the same bytes as `dwebp`'s.
  Alpha is kept, reported the way the system's png.datatype reports it.
- MultiView shows the pictures; behind the transparent part of a sticker
  it paints black, and it does the same with a PNG with alpha decoded by
  the system (`shots/amigaos-4.1-multiview-alpha.png`).
- Two damaged files are refused. The error number left behind is not a
  usable code on this system (a large number), and it is the same kind of
  number when the system's own datatypes refuse a file.

## Two real 68k machines, from a distance

Both run AmigaOS 3.2.3 (Kickstart 47.115, Workbench 47.5) and neither had
a WEBP datatype. The files went to RAM: only, with `Assign LIBS:
RAM:wdt/Classes ADD` and `AddDataTypes RAM:wdt/WebP`; the library and
`dtprobe` were checked with an MD5 after the copy; `dtprobe` ran from a
script, over a remote shell. Nothing was shown on their screens.

- **An A1200 with a 68080 accelerator**, picture.datatype 47.19: the seven
  checksums of the "true colour" column, alpha kept (depth 32,
  mskHasAlpha). The two damaged files are refused with error 2008.
  `alpha-lossy.webp` opens in 0.92 s, `alpha-lossless.webp` in 0.44 s,
  `lossy.webp` in 0.28 s.
- **An A1200 with a PiStorm, running AfA OS on top of the system**: the
  picture.datatype in memory there is 46.9, although the file in LIBS: is
  47.19. It takes true colour and does not know PDTA_AlphaChannel, so this
  is the path that no emulated system had: the pictures with alpha are
  laid on white, and the seven checksums are those of the "laid on white"
  column (depth 24, no masking). The two damaged files are refused with
  error 2008. `alpha-lossy.webp` opens in 0.18 s.

## A real MorphOS machine, from a distance

A Power Mac G5 with MorphOS 3.20 (Ambient 1.50, Kickstart 51.66,
picture.datatype 50.19), one release older than the emulated one, and no
WEBP datatype in the system. The files went to RAM: only; the library,
the descriptor and `dtprobe` were sent back to the build machine and
compared with an MD5 before anything ran. Then `Assign LIBS:
RAM:wdt/Classes ADD` and `AddDataTypes RAM:wdt/WebP` as above, and
`dtprobe` from a script, over a remote shell.

- The seven checksums of the "true colour" column. The five pictures with
  alpha keep it (depth 32, mskHasAlpha, PDTA_AlphaChannel).
- The two damaged files are refused with error 2008.
- Every picture opens within a tick or two of the clock `dtprobe` reads,
  which counts fiftieths of a second: it shows 0.02 s for
  `alpha-lossy.webp` and 0.00 s for the others.
- MultiView 3.17, started from a distance, says "WebP immagine: 512 x 512
  pixel, 32 bit" and blends the sticker, soft shadow included, over its
  window (`shots/morphos-3.20-g5-multiview-alpha.png`). The picture is the
  window as the machine composed it, read back over the network: nobody
  was at the monitor.

## Not checked

- A picture looked at by a person on the screen of a real machine (the
  one from the G5 above was read back from a distance; the two 68k
  machines showed nothing).
- AmigaOS 4 on a real machine.
- The other true colour picture.datatypes without alpha (AmigaOS 3.5, 3.9,
  3.1.4, an early 3.2, the ones of graphics card systems): one of the kind
  was tried, on the PiStorm machine above.
- AmigaOS 4 before 4.1 Final Edition Update 3, MorphOS before 3.20.
- Running out of memory in the middle of a picture.
- A picture large enough to matter (the largest tried is 512 x 512).
