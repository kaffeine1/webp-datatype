#!/bin/sh
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Makes one LhA archive for each lane that has been built, and the Aminet
# readme that goes beside it:
#
#   scripts/package-release.sh [build directory] [output directory]
#
#   dist/webpdt.m68k-amigaos.lha   dist/webpdt.m68k-amigaos.readme
#   dist/webpdt.ppc-morphos.lha    dist/webpdt.ppc-morphos.readme
#   dist/webpdt.ppc-amigaos.lha    dist/webpdt.ppc-amigaos.readme
#
# Every archive holds one drawer, webpdt, with the library and the
# descriptor where they go under SYS: and DEVS:, the Install script, the
# ReadMe, the licences, and dtprobe with the test pictures in Extras.
#
# It publishes nothing. It needs an LhA that can create archives (LHA=, or
# lha on the path; Lhasa only extracts) and a clean work tree, so that the
# commit written into the ReadMe is the source the binaries came from.

set -eu

here=$(cd "$(dirname "$0")/.." && pwd)
build=${1:-"$here/build"}
out=${2:-"$here/dist"}
lha=${LHA:-lha}
name=webpdt

if [ ! -d "$build" ]; then
	echo "no build directory $build" >&2
	exit 1
fi
build=$(cd "$build" && pwd)
mkdir -p "$out"
out=$(cd "$out" && pwd)

cd "$here"

version=$(sed -n 's/^#define WDT_VERSION //p' src/wdt_version.h)
revision=$(sed -n 's/^#define WDT_REVISION //p' src/wdt_version.h)
date=$(sed -n 's/^#define WDT_DATE "\(.*\)"/\1/p' src/wdt_version.h)
full="$version.$revision"

if [ -n "$(git status --porcelain)" ]; then
	echo "the work tree has changes: commit them first" >&2
	exit 1
fi
commit=$(git rev-parse --short=12 HEAD)

if ! "$lha" --version 2>&1 | grep -q 'LHa for UNIX'; then
	echo "$lha cannot create archives: set LHA= to an LhA encoder" >&2
	exit 1
fi

if [ ! -f "$build/descriptor/WebP" ]; then
	echo "no descriptor in $build: run make descriptor" >&2
	exit 1
fi

# lane, Aminet architecture token, what `file` must say, the system in words,
# the Architecture: and Requires: lines of the readme
lanes='os3|m68k-amigaos|AmigaOS loadseg|AmigaOS 3.x on a 68020 or better|m68k-amigaos >= 3.0.0|68020 or better
morphos|ppc-morphos|ELF 32-bit MSB|MorphOS 3|ppc-morphos >= 3.0.0|MorphOS 3
os4|ppc-amigaos|ELF 32-bit MSB|AmigaOS 4.1|ppc-amigaos >= 4.1.0|AmigaOS 4.1'

fill() {
	sed -e "s/@VERSION@/$full/g" -e "s/@DATE@/$date/g" \
		-e "s/@COMMIT@/$commit/g" -e "s/@SYSTEM@/$system/g" "$1"
}

echo "$lanes" | while IFS='|' read -r lane arch magic system archline requires; do
	lib="$build/$lane/webp.datatype"
	probe="$build/$lane/dtprobe"
	if [ ! -f "$lib" ] || [ ! -f "$probe" ]; then
		echo "$lane: not built, skipped"
		continue
	fi

	# The right kind of binary for this archive, and not the other PowerPC
	# one: the MorphOS library carries __abox__, the AmigaOS 4 one does not.
	for binary in "$lib" "$probe"; do
		if ! file "$binary" | grep -q "$magic"; then
			echo "$lane: $binary is not a binary for $system" >&2
			exit 1
		fi
	done
	abox=$(grep -c -a '__abox__' "$lib" || true)
	case "$lane" in
	morphos) [ "$abox" -gt 0 ] || { echo "$lane: not a MorphOS library" >&2; exit 1; } ;;
	os4) [ "$abox" -eq 0 ] || { echo "$lane: a MorphOS library in the OS4 lane" >&2; exit 1; } ;;
	esac
	if ! grep -q -a "webp.datatype $full ($date)" "$lib"; then
		echo "$lane: the library is not version $full ($date)" >&2
		exit 1
	fi

	stage="$out/stage/$lane"
	drawer="$stage/$name"
	rm -rf "$stage"
	mkdir -p "$drawer/Classes/DataTypes" "$drawer/Devs/DataTypes" \
		"$drawer/Extras/pictures"
	cp "$lib" "$drawer/Classes/DataTypes/webp.datatype"
	cp "$build/descriptor/WebP" "$drawer/Devs/DataTypes/WebP"
	cp "$probe" "$drawer/Extras/dtprobe"
	cp tests/pictures/*.webp "$drawer/Extras/pictures/"
	cp COPYING "$drawer/COPYING"
	cp third_party/libwebp/COPYING "$drawer/COPYING.libwebp"
	cp third_party/libwebp/PATENTS "$drawer/PATENTS.libwebp"
	fill packaging/ReadMe > "$drawer/ReadMe"
	fill packaging/Install > "$drawer/Install"
	chmod 644 "$drawer/ReadMe" "$drawer/Install" "$drawer"/COPYING* \
		"$drawer/PATENTS.libwebp" "$drawer/Devs/DataTypes/WebP" \
		"$drawer/Extras/pictures"/*
	chmod 755 "$drawer/Classes/DataTypes/webp.datatype" "$drawer/Extras/dtprobe"

	archive="$out/$name.$arch.lha"
	rm -f "$archive"
	(cd "$stage" && "$lha" aq "$archive" "$name")
	"$lha" tq "$archive" > /dev/null

	{
		echo "Short:        WEBP picture datatype, built on libwebp"
		echo "Uploader:     michele.dipace@kaffeine.net (Michele Dipace)"
		echo "Author:       Michele Dipace <michele.dipace@kaffeine.net>"
		echo "Type:         util/dtype"
		echo "Version:      $full"
		echo "Architecture: $archline"
		echo "Requires:     $requires"
		echo
		fill packaging/ReadMe
	} > "$out/$name.$arch.readme"

	if awk 'length($0) > 78 { bad = 1 } END { exit !bad }' "$out/$name.$arch.readme"; then
		echo "$lane: a line of the readme is longer than 78 columns" >&2
		exit 1
	fi

	rm -rf "$stage"
	echo "$archive"
done
rmdir "$out/stage" 2>/dev/null || true

ls "$out"/$name.*.lha > /dev/null 2>&1 || { echo "nothing was packaged" >&2; exit 1; }
