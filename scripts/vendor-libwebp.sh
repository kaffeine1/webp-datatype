#!/bin/sh
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Copies the decoding half of libwebp into third_party/libwebp.
#
#   scripts/vendor-libwebp.sh <unpacked libwebp-1.6.0 directory>
#
# Only the plain C decoder goes in: no encoder, no mux, none of the SSE,
# NEON, MIPS or MSA files (config.h keeps them switched off, and no lane of
# this datatype has those instruction sets).

set -eu

VERSION=1.6.0
SHA256=e4ab7009bf0629fd11982d4c2aa83964cf244cffba7347ecd39019a9e38c4564

src=${1:?give the unpacked libwebp-$VERSION directory}
here=$(cd "$(dirname "$0")/.." && pwd)
dst=$here/third_party/libwebp

test -f "$src/src/dec/vp8_dec.c" || { echo "$src: not a libwebp tree" >&2; exit 1; }
grep -q "AC_INIT(\[libwebp\], *\[$VERSION\]" "$src/configure.ac" ||
	{ echo "$src: not libwebp $VERSION" >&2; exit 1; }

FILES="
src/dec/alpha_dec.c src/dec/buffer_dec.c src/dec/frame_dec.c
src/dec/idec_dec.c src/dec/io_dec.c src/dec/quant_dec.c src/dec/tree_dec.c
src/dec/vp8_dec.c src/dec/vp8l_dec.c src/dec/webp_dec.c
src/dec/alphai_dec.h src/dec/common_dec.h src/dec/vp8_dec.h
src/dec/vp8i_dec.h src/dec/vp8li_dec.h src/dec/webpi_dec.h
src/demux/demux.c
src/dsp/alpha_processing.c src/dsp/cpu.c src/dsp/dec.c
src/dsp/dec_clip_tables.c src/dsp/filters.c src/dsp/lossless.c
src/dsp/rescaler.c src/dsp/upsampling.c src/dsp/yuv.c
src/dsp/cpu.h src/dsp/dsp.h src/dsp/lossless.h src/dsp/lossless_common.h
src/dsp/yuv.h
src/utils/bit_reader_utils.c src/utils/color_cache_utils.c
src/utils/filters_utils.c src/utils/huffman_utils.c src/utils/palette.c
src/utils/quant_levels_dec_utils.c src/utils/random_utils.c
src/utils/rescaler_utils.c src/utils/thread_utils.c src/utils/utils.c
src/utils/bit_reader_inl_utils.h src/utils/bit_reader_utils.h
src/utils/color_cache_utils.h src/utils/endian_inl_utils.h
src/utils/filters_utils.h src/utils/huffman_utils.h src/utils/palette.h
src/utils/quant_levels_dec_utils.h src/utils/random_utils.h
src/utils/rescaler_utils.h src/utils/thread_utils.h src/utils/utils.h
src/webp/decode.h src/webp/demux.h src/webp/encode.h
src/webp/format_constants.h src/webp/mux.h src/webp/mux_types.h
src/webp/types.h
COPYING PATENTS AUTHORS
"

rm -rf "$dst"
for f in $FILES; do
	mkdir -p "$dst/$(dirname "$f")"
	cp "$src/$f" "$dst/$f"
done

cat > "$dst/README.vendor" <<EOF
libwebp $VERSION, the decoding half only.

Source: libwebp-$VERSION.tar.gz from
https://storage.googleapis.com/downloads.webmproject.org/releases/webp/
SHA256 of that archive: $SHA256

The files are unchanged copies. scripts/vendor-libwebp.sh lists them and
copies them again from an unpacked archive. The build configuration is
src/webp/config.h in this repository's own src directory, not here.

Licence: COPYING (BSD 3-clause), with the patent grant in PATENTS.
EOF

echo "vendored libwebp $VERSION into $dst"
