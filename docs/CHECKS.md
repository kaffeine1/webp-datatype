# How each lane was checked

Everything below was run on 12 November 2026: first in emulators, then
`dtprobe` on three real machines, from a distance, MultiView on one of
them, and a chat client that installs the datatype itself on the same
three. The run on a MorphOS without the datatype is of the day after, the
runs of the Install script that follows 45.1 of 14 November, the one of
that script on the real MorphOS machine of 16 November, and `dtprobe`
with 45.2 there the day after.
Read this as what was tried, on what, and what came out.

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

## MorphOS without this datatype

MorphOS has WEBP support of its own, and it is not a datatype. This run,
of 13 November 2026, is what it does alone: the emulated MorphOS 3.21 of
above (QEMU, Pegasos 2) after a restart, with nothing of this project in
it but `dtprobe` and `alpha-lossy.webp` in RAM:. No `Assign`, no
`AddDataTypes`.

- `MOSSYS:Classes/Multimedia` holds `webp.demuxer` and `webp.decoder`,
  two classes of Reggae. No WEBP library is in `Classes/DataTypes` and no
  WEBP descriptor in `Devs/DataTypes`, on SYS: or on MOSSYS:.
  datatypes.library 51.4, picture.datatype 50.19.
- `dtprobe RAM:t.webp` says "no picture, error 212" (object of the wrong
  type): `NewDTObject` with DTST_FILE and GID_PICTURE gives no object.
- MultiView 3.17 opens the same file: "512 x 512 pixels, 32 bit depth",
  blended over its window
  (`shots/morphos-3.21-multiview-no-datatype.png`). With this datatype
  installed the same line begins with "WebP picture:", the name its
  descriptor gives.

The Power Mac G5 with MorphOS 3.20 has the same two Reggae classes (both
51.1, of 30 January 2026) and no WEBP datatype among the system's own.
MultiView without this datatype was not tried there.

So on MorphOS this datatype changes nothing for MultiView, which shows
WEBP files anyway. It is for programs that ask datatypes.library for a
picture, and get none without it.

## The release archives of 45.1

`make dist` packs each lane into an LhA archive (`scripts/package-release.sh`).

- The three libraries, the three `dtprobe` and the descriptor were built
  again from an empty directory: they are, byte for byte (MD5), the files
  that went through the checks above.
- Each archive was unpacked on the build machine with another LhA program
  (Lhasa) than the one that made it: the binaries inside are those same
  files, and each archive holds the binary of its own system.
- `Install`, from the unpacked 68k archive, on AmigaOS 3.2.3 in WinUAE, in
  a Shell window:
  - on a system with no WEBP datatype (the two files deleted, the machine
    restarted, `dtprobe` refused a WEBP file): it copies the two files,
    and right after it, without a restart, `dtprobe` opens
    `alpha-lossy.webp` and `lossy.webp` with the checksums of the table;
  - with a webp.datatype installed already: it shows the version and
    asks; "n" leaves everything as it was, "y" replaces the two files;
  - started from another directory: it says so and changes nothing.

Not tried: `Install` on MorphOS and on AmigaOS 4, and unpacking an archive
with LhA on an Amiga. The archives have no icons, so `Install` is run from
a Shell.

## Inside a chat client, on the same real machines

Amigram, a Telegram client for these systems, shows Telegram's stickers
(WEBP pictures, 512 x 512 with alpha) through datatypes.library. Where the
system has no WEBP datatype it offers to fetch this one, release 45.1, and
installs the library and the descriptor itself. That was tried on the
three real machines above (the Power Mac G5 with MorphOS 3.20, and the
two A1200 with AmigaOS 3.2.3, one with a 68080 and one with a PiStorm
and AfA OS), and in the AmigaOS 4.1 emulator:

- The two files the client put into `SYS:Classes/DataTypes` and
  `DEVS:DataTypes` are the files of the release, byte for byte.
- Stickers received in a chat are shown as pictures: with their
  transparency on the G5, on the 68080 machine and in the AmigaOS 4.1
  emulator, laid on white on the PiStorm machine, whose picture.datatype
  does not know about alpha.
- On the PiStorm machine the first version of the client drew the
  stickers as vertical stripes. The cause was in how it read the pixels
  back, not in the datatype: AfA OS's picture.datatype 46.9 answers a
  PDTM_READPIXELARRAY for RGBA pixels with success and writes nothing,
  and it scrambles ARGB pixels asked for in bands of rows. Asked for the
  whole picture as ARGB in one request, as `dtprobe` does, it gives the
  right picture. The README says this for any program that reads the
  pixels.

## The Install script after 45.1

Easy2Install, the package manager of MorphOS, unpacks an archive to the
RAM disk and starts its `Install` script itself. On a real MorphOS
machine it did that with the 45.1 archive of MorphOS-Storage, where the
drawer is named `webp.datatype`: the files were in `RAM:webp.datatype`.
The 45.1 script was written for a person in a Shell: it wanted to be
started from its drawer, and it asked before it replaced any
webp.datatype, its own older version too.

The script that follows 45.1 asks only before it replaces a WEBP datatype
that is not this one. It was run on 14 November 2026 on the emulated
MorphOS 3.21 of above, with a 45.2 library built that day, the files in
`RAM:webp.datatype`, started as `C:Execute RAM:webp.datatype/Install`:

- with no WEBP datatype in the system, started from `SYS:`: it goes to
  its drawer and copies the two files, and `Version` reads 45.2 from the
  installed library;
- with 45.2 installed, started from `SYS:` and from another drawer of
  the RAM disk: it copies again, without a question;
- over the 45.1 library of the release archive: no question, and 45.2 is
  there afterwards;
- over a 45.1 library protected from writing and from deleting: it says
  that the library could not be copied, and stops;
- over a file with another version string ("webp.datatype 45.5", made
  for the test) and over a file with no version string: it shows what
  `Version` says about the file and asks; "n" changes nothing, and "y",
  tried on the first of the two, replaces it;
- nothing is left in `T:`.

Three things came out of the first tries, and are why the script is the
way it is:

- `Version` prints the date the way the system's language wants it
  ("11/12/26" on that English system, "12-Nov-2026" on an Italian one),
  so a text with a date in it cannot be looked for in what it prints.
  From 45.2 on the library has its author's name after the date, and
  that is what the script looks for; 45.1 is looked for by its whole
  version string in the file itself.
- Started from `SYS:` with a datatype installed, the script found
  `Classes/DataTypes/webp.datatype` right there, took `SYS:` for its own
  drawer and copied the two files onto themselves. It now knows its
  drawer by the `Install` file.
- The `Copy` of MorphOS answers 0 also when it could not write (a file
  copied onto itself, a file protected from deleting). After copying,
  the script reads the version of what is in the system.

The same script was run the same day on three more emulated systems,
each with a 45.2 library built that day and the drawer outside the
system disk: AmigaOS 3.2.3 (68020, picture.datatype 47.19), Kickstart
3.1 with Workbench 3.1 (68020, picture.datatype 40.4, `Version` 40.3,
`Search` 40.1; its floppy is full, so `SYS:` and `DEVS:` pointed at the
RAM disk for the run) and AmigaOS 4.1 Final Edition Update 3. On all
three:

- with no WEBP datatype in the system the two files are copied, and on
  Workbench 3.1 the two `DataTypes` drawers are made first;
- over 45.2 and over the 45.1 library of the release archive: no
  question, and 45.2 is there afterwards;
- started from another directory with no `RAM:webp.datatype`: it says
  to run it from its own drawer and changes nothing;
- started from `SYS:` with its files in `RAM:webp.datatype` and 45.1
  installed: it goes there and installs;
- over another datatype under this name (the made-up "webp.datatype
  45.5" file on 3.2.3, the system's ilbm.datatype on 3.1, the system's
  png.datatype on 4.1) and over a file with no version string: it shows
  what `Version` says and asks; "n" changes nothing, and "y", tried on
  the first of the two, replaces it;
- over a 45.1 library protected from writing and from deleting: `Copy`
  says why it cannot write, the script says that the library could not
  be copied, and stops;
- nothing of the script is left in `T:`;
- after the last installation `dtprobe` gave the seven checksums of the
  table with the 45.2 library in memory: the "true colour" column on
  3.2.3 and on 4.1, the "256 colours" column on 3.1.

What these runs added:

- `Version` prints the date the system's way on these three too
  ("11/14/2026", "11/14/26"), and the author's name on a line of its
  own.
- When the library cannot be copied, the descriptor has been copied by
  then. The system is left with the old library and the new descriptor,
  which is the old one but for the version and the date written in it.
- On AmigaOS 4.1 `Version` says that it found no version in the Shell
  window, not in what it writes to a file: that line is on the screen
  above "Another WEBP datatype is installed", not below it.
- The `Version` of AmigaOS 4.1 can answer from memory. The test put the
  45.1 library in place and started the script within the same second,
  and the two libraries have the same size. `Copy` answered 0 and the
  file was the new one (sent back to the host: the MD5 of the 45.2
  build), yet `Version` went on answering 45.1 until the file was
  written again, so the script said that the library could not be
  copied. It takes a `Version` of a file and then a new file of the same
  name and size within the same second: with four seconds between the
  two it did not happen, and neither with `Search` in the place of
  `Version`. An installation does not do that; a test must wait.
- The made-up "webp.datatype 45.5" file began with the first four bytes
  of a 68k program and had nothing valid behind them. AmigaOS 3.2 and
  MorphOS read its version. The `Version` of AmigaOS 4.1 crashed on it,
  twice, and the file stayed locked until a restart. That was the test
  file: the system's png.datatype under this name was read without
  trouble.
- When the script goes to `RAM:webp.datatype` by itself, the Shell that
  ran it is left in that drawer.

On 16 November 2026 the script ran on a real machine: the Power Mac G5
with MorphOS 3.20 of above, where Easy2Install had installed the 45.1
archive of MorphOS-Storage. An archive with the 45.2 library and
descriptor for MorphOS (the files of the release archive) was copied to
its RAM disk and checked there with `lha t`, then unpacked and `Install`
run in a Shell by a person at the machine. It installed 45.2 over 45.1
without a question. Read from a distance afterwards, `Version` gives
"webp.datatype 45.2 (14-Nov-2026)" and "Michele Dipace", and the library
and the descriptor in the system have the sizes of the files of the
archive.

On 17 November the MorphOS release archive of 45.2 went to the RAM disk
of the same machine, passed `lha t` there and was unpacked there. Its
`dtprobe` ran from a script (`Stack 1048576`, output to a file, over a
remote shell) on the seven test pictures, through the 45.2 installed the
day before ("datatype: WebP"):

- the seven checksums of the "true colour" column;
- the five pictures with alpha keep it (depth 32, masking 4,
  PDTA_AlphaChannel);
- every picture opens in 0.02 s or less by the clock `dtprobe` reads.

Not tried: Easy2Install itself starting this script, AmigaOS 3.5, 3.9
and 3.1.4, and a real machine with AmigaOS 3 or 4.

## Not checked

- The test pictures looked at by a person on the screen of a real
  machine: there they were checked by their checksums or read back from
  a distance, and what was seen in the client are stickers.
- AmigaOS 4 on a real machine.
- The other true colour picture.datatypes without alpha (AmigaOS 3.5, 3.9,
  3.1.4, an early 3.2, the ones of graphics card systems): one of the kind
  was tried, on the PiStorm machine above.
- AmigaOS 4 before 4.1 Final Edition Update 3, MorphOS before 3.20.
- On MorphOS: MultiView on 3.20 without this datatype, and other programs
  of the system that show pictures.
- Running out of memory in the middle of a picture.
- A picture large enough to matter (the largest tried is 512 x 512).
