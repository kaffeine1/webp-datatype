#!/bin/sh
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Decodes every test picture with the code the datatype is built from and
# compares the pixels with what libwebp's own dwebp writes for the same
# file; for the animations, with the PNG of the first frame. Then feeds the
# decoder damaged copies of every file.
#
#   tests/run-host-tests.sh <webpcheck binary> <output directory>
#
# Needs dwebp (libwebp tools) and python3 with Pillow.

set -eu

check=${1:?webpcheck binary}
out=${2:?output directory}
here=$(cd "$(dirname "$0")" && pwd)
pictures=$here/pictures
failed=0

mkdir -p "$out"

for name in lossy lossless alpha-lossy alpha-lossless odd-37x29; do
	"$check" decode "$pictures/$name.webp" "$out/$name.pam"
	dwebp -quiet -pam "$pictures/$name.webp" -o "$out/$name.dwebp.pam"
	if cmp -s "$out/$name.pam" "$out/$name.dwebp.pam"; then
		echo "  same pixels as dwebp"
	else
		echo "  DIFFERENT from dwebp" >&2
		failed=1
	fi
done

for name in animated animated-offset; do
	"$check" decode "$pictures/$name.webp" "$out/$name.pam"
	if python3 "$here/compare-pam-png.py" "$out/$name.pam" \
		"$pictures/$name.frame1.png"; then
		echo "  same pixels as the first frame"
	else
		echo "  DIFFERENT from the first frame" >&2
		failed=1
	fi
done

for name in lossy lossless alpha-lossy alpha-lossless odd-37x29 animated \
	animated-offset; do
	"$check" planar "$pictures/$name.webp" "$out/$name.planar.pam"
	if python3 "$here/compare-pam-png.py" --close "$out/$name.planar.pam" \
		"$out/$name.pam"; then
		echo "  close to the true colour picture"
	else
		echo "  the 256 colour picture is too far from the original" >&2
		failed=1
	fi
done

for name in lossy lossless alpha-lossy alpha-lossless odd-37x29 animated \
	animated-offset; do
	"$check" damage "$pictures/$name.webp"
done

if [ "$failed" -ne 0 ]; then
	echo "FAILED" >&2
	exit 1
fi
echo "all host tests passed"
