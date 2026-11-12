# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# webp.datatype for AmigaOS 3.x (68020+), AmigaOS 4 and MorphOS, and the
# host program that tests the decoder they share.
#
#   make                the host test program
#   make test           decode every test picture and compare with dwebp
#   make test SAN=1     the same under the address and undefined sanitizers
#   make os3            build/os3/webp.datatype      (m68k-amigaos-gcc)
#   make morphos        build/morphos/webp.datatype  (ppc-morphos-gcc)
#   make os4            build/os4/webp.datatype      (ppc-amigaos-gcc)
#   make descriptor     build/descriptor/WebP        (DEVS:DataTypes/WebP)
#   make kit            build/kit/<lane>: what to copy to a machine to try it
#
# AROS has a webp.datatype of its own in the system: there is no AROS lane.

BUILD ?= build

# --- libwebp, decoder only -------------------------------------------------

WEBP := third_party/libwebp
WEBP_SRC := \
	$(WEBP)/src/dec/alpha_dec.c $(WEBP)/src/dec/buffer_dec.c \
	$(WEBP)/src/dec/frame_dec.c $(WEBP)/src/dec/idec_dec.c \
	$(WEBP)/src/dec/io_dec.c $(WEBP)/src/dec/quant_dec.c \
	$(WEBP)/src/dec/tree_dec.c $(WEBP)/src/dec/vp8_dec.c \
	$(WEBP)/src/dec/vp8l_dec.c $(WEBP)/src/dec/webp_dec.c \
	$(WEBP)/src/demux/demux.c \
	$(WEBP)/src/dsp/alpha_processing.c $(WEBP)/src/dsp/cpu.c \
	$(WEBP)/src/dsp/dec.c $(WEBP)/src/dsp/dec_clip_tables.c \
	$(WEBP)/src/dsp/filters.c $(WEBP)/src/dsp/lossless.c \
	$(WEBP)/src/dsp/rescaler.c $(WEBP)/src/dsp/upsampling.c \
	$(WEBP)/src/dsp/yuv.c \
	$(WEBP)/src/utils/bit_reader_utils.c \
	$(WEBP)/src/utils/color_cache_utils.c \
	$(WEBP)/src/utils/filters_utils.c $(WEBP)/src/utils/huffman_utils.c \
	$(WEBP)/src/utils/palette.c \
	$(WEBP)/src/utils/quant_levels_dec_utils.c \
	$(WEBP)/src/utils/rescaler_utils.c \
	$(WEBP)/src/utils/thread_utils.c $(WEBP)/src/utils/utils.c
WEBP_CPPFLAGS := -DHAVE_CONFIG_H -DNDEBUG -Ilibwebp-config -I$(WEBP)

CORE_SRC := src/wdt_decode.c src/wdt_quant.c src/wdt_webp_random.c
HEADERS := $(wildcard src/*.h src/*/*.h libwebp-config/src/webp/*.h) \
	$(wildcard $(WEBP)/src/*/*.h)

# --- host ------------------------------------------------------------------

HOST_CC ?= cc
HOST_CFLAGS ?= -O2 -g
HOST_WARN := -Wall -Wextra -Werror
ifeq ($(SAN),1)
HOST_DIR := $(BUILD)/host-san
HOST_CFLAGS += -fsanitize=address,undefined -fno-sanitize-recover=all
else
HOST_DIR := $(BUILD)/host
endif

HOST_WEBP_OBJ := $(patsubst %.c,$(HOST_DIR)/%.o,$(WEBP_SRC))
HOST_OBJ := $(patsubst %.c,$(HOST_DIR)/%.o,$(CORE_SRC) tests/webpcheck.c)

.PHONY: all test clean os3 morphos os4 descriptor kit
all: $(HOST_DIR)/webpcheck

$(HOST_WEBP_OBJ): $(HOST_DIR)/%.o: %.c $(HEADERS)
	@mkdir -p "$(dir $@)"
	$(HOST_CC) $(HOST_CFLAGS) $(WEBP_CPPFLAGS) -c -o "$@" "$<"

$(HOST_OBJ): $(HOST_DIR)/%.o: %.c $(HEADERS)
	@mkdir -p "$(dir $@)"
	$(HOST_CC) $(HOST_CFLAGS) $(HOST_WARN) $(WEBP_CPPFLAGS) -Isrc -c -o "$@" "$<"

$(HOST_DIR)/webpcheck: $(HOST_OBJ) $(HOST_WEBP_OBJ)
	$(HOST_CC) $(HOST_CFLAGS) -o "$@" $^

test: $(HOST_DIR)/webpcheck
	sh tests/run-host-tests.sh "$(HOST_DIR)/webpcheck" "$(HOST_DIR)/out"

# --- the descriptor ----------------------------------------------------------

DT_VERSION := $(shell sed -n 's/^\#define WDT_VERSION //p' src/wdt_version.h)
DT_REVISION := $(shell sed -n 's/^\#define WDT_REVISION //p' src/wdt_version.h)
DT_DATE := $(shell sed -n 's/^\#define WDT_DATE "\(.*\)"/\1/p' src/wdt_version.h)

descriptor: $(BUILD)/descriptor/WebP
$(BUILD)/descriptor/WebP: descriptor/make-descriptor.py src/wdt_version.h
	@mkdir -p "$(dir $@)"
	python3 descriptor/make-descriptor.py --selftest
	python3 descriptor/make-descriptor.py "$@" \
		--version "$(DT_VERSION).$(DT_REVISION) ($(DT_DATE))"

# --- what the three Amiga lanes share ----------------------------------------

AMIGA_SRC := src/wdt_class.c $(CORE_SRC)

# --- AmigaOS 3.x, 68020 and better -------------------------------------------
#
# No C library and no startup code: src/wdt_libc.c has what the decoder
# calls, and libgcc the 64 bit arithmetic. start.o must be linked first.
# lib.c alone is 68000 code, so that the library can refuse a 68000.

OS3_CC ?= m68k-amigaos-gcc
OS3_DIR := $(BUILD)/os3
OS3_CFLAGS := -noixemul -m68020 -O2 -fomit-frame-pointer -fno-common \
	-DWDT_PLANAR_FALLBACK=1
# The NDK declares its strings unsigned: plain literals are not an error.
OS3_WARN := -Wall -Wextra -Werror -Wno-pointer-sign
OS3_OBJ := $(patsubst %.c,$(OS3_DIR)/%.o,$(AMIGA_SRC))
OS3_WEBP_OBJ := $(patsubst %.c,$(OS3_DIR)/%.o,$(WEBP_SRC))

os3: $(OS3_DIR)/webp.datatype $(OS3_DIR)/dtprobe $(OS3_DIR)/webpcheck

# The test tool is an ordinary program, with the compiler's C library.
$(OS3_DIR)/dtprobe: tools/dtprobe.c
	@mkdir -p "$(dir $@)"
	$(OS3_CC) -noixemul -m68020 -O2 $(OS3_WARN) -o "$@" "$<"

$(OS3_DIR)/src/os3/start.o: src/os3/start.s
	@mkdir -p "$(dir $@)"
	$(OS3_CC) -m68020 -c -o "$@" "$<"

$(OS3_DIR)/src/os3/lib.o: src/os3/lib.c $(HEADERS)
	@mkdir -p "$(dir $@)"
	$(OS3_CC) $(OS3_CFLAGS) -m68000 $(OS3_WARN) -Isrc -c -o "$@" "$<"

$(OS3_DIR)/src/wdt_libc.o: src/wdt_libc.c
	@mkdir -p "$(dir $@)"
	$(OS3_CC) $(OS3_CFLAGS) $(OS3_WARN) -fno-builtin \
		-fno-tree-loop-distribute-patterns -fno-strict-aliasing -c -o "$@" "$<"

$(OS3_OBJ): $(OS3_DIR)/%.o: %.c $(HEADERS)
	@mkdir -p "$(dir $@)"
	$(OS3_CC) $(OS3_CFLAGS) $(OS3_WARN) $(WEBP_CPPFLAGS) -Isrc -c -o "$@" "$<"

$(OS3_WEBP_OBJ): $(OS3_DIR)/%.o: %.c $(HEADERS)
	@mkdir -p "$(dir $@)"
	$(OS3_CC) $(OS3_CFLAGS) $(WEBP_CPPFLAGS) -c -o "$@" "$<"

$(OS3_DIR)/webp.datatype: $(OS3_DIR)/src/os3/start.o $(OS3_DIR)/src/os3/lib.o \
		$(OS3_OBJ) $(OS3_DIR)/src/wdt_libc.o $(OS3_WEBP_OBJ)
	$(OS3_CC) -noixemul -m68020 -nostartfiles -nostdlib -s -o "$@" $^ -lgcc
	@strings "$@" | grep -q '^\$$VER: webp.datatype ' || \
		{ echo "$@: no version string" >&2; rm -f "$@"; exit 1; }

# The host test program built for the 68020: run on an Amiga, or in an
# emulator, it writes the same PAM files as the host build, which checks the
# 68k compiler's output of the decoder and of src/wdt_libc.c without any
# datatype in between.
$(OS3_DIR)/webpcheck: tests/webpcheck.c $(OS3_DIR)/src/wdt_decode.o \
		$(OS3_DIR)/src/wdt_quant.o $(OS3_DIR)/src/wdt_webp_random.o \
		$(OS3_DIR)/src/wdt_libc.o $(OS3_WEBP_OBJ)
	$(OS3_CC) -noixemul -m68020 -O2 $(OS3_WARN) -Isrc -o "$@" $^

# --- the two PowerPC lanes ----------------------------------------------------
#
# Both picture.datatypes take true colour, so the 256 colour path stays out.
# Same rule as on 68k: no C library, no startup code.

PPC_SRC := src/wdt_class.c src/wdt_decode.c src/wdt_webp_random.c
PPC_WARN := -Wall -Wextra -Werror -Wno-pointer-sign

# --- MorphOS -------------------------------------------------------------------
#
# start.o is linked first: it is the function that says no to being run as
# a program. The strip keeps the symbol table, where the system looks for
# __abox__ to know a native binary.

MOS_CC ?= ppc-morphos-gcc
MOS_STRIP ?= ppc-morphos-strip
MOS_DIR := $(BUILD)/morphos
MOS_CFLAGS := -noixemul -O2 -fno-common
MOS_OBJ := $(patsubst %.c,$(MOS_DIR)/%.o,src/mos/start.c src/mos/lib.c $(PPC_SRC))
MOS_WEBP_OBJ := $(patsubst %.c,$(MOS_DIR)/%.o,$(WEBP_SRC))

morphos: $(MOS_DIR)/webp.datatype $(MOS_DIR)/dtprobe

$(MOS_DIR)/dtprobe: tools/dtprobe.c
	@mkdir -p "$(dir $@)"
	$(MOS_CC) -noixemul -O2 $(PPC_WARN) -o "$@" "$<"

$(MOS_DIR)/src/wdt_libc.o: src/wdt_libc.c
	@mkdir -p "$(dir $@)"
	$(MOS_CC) $(MOS_CFLAGS) $(PPC_WARN) -fno-builtin \
		-fno-tree-loop-distribute-patterns -fno-strict-aliasing -c -o "$@" "$<"

$(MOS_OBJ): $(MOS_DIR)/%.o: %.c $(HEADERS)
	@mkdir -p "$(dir $@)"
	$(MOS_CC) $(MOS_CFLAGS) $(PPC_WARN) $(WEBP_CPPFLAGS) -Isrc -c -o "$@" "$<"

$(MOS_WEBP_OBJ): $(MOS_DIR)/%.o: %.c $(HEADERS)
	@mkdir -p "$(dir $@)"
	$(MOS_CC) $(MOS_CFLAGS) $(WEBP_CPPFLAGS) -c -o "$@" "$<"

$(MOS_DIR)/webp.datatype: $(MOS_OBJ) $(MOS_DIR)/src/wdt_libc.o $(MOS_WEBP_OBJ)
	$(MOS_CC) -noixemul -nostartfiles -nostdlib -o "$@" $^ -lgcc
	$(MOS_STRIP) --strip-unneeded --remove-section=.comment "$@"
	@strings "$@" | grep -q '^\$$VER: webp.datatype ' || \
		{ echo "$@: no version string" >&2; rm -f "$@"; exit 1; }

# --- AmigaOS 4.1 ----------------------------------------------------------------

OS4_CC ?= ppc-amigaos-gcc
OS4_STRIP ?= ppc-amigaos-strip
OS4_DIR := $(BUILD)/os4
OS4_CFLAGS := -mcrt=newlib -O2 -fno-common
OS4_OBJ := $(patsubst %.c,$(OS4_DIR)/%.o,src/os4/lib.c $(PPC_SRC))
OS4_WEBP_OBJ := $(patsubst %.c,$(OS4_DIR)/%.o,$(WEBP_SRC))

os4: $(OS4_DIR)/webp.datatype $(OS4_DIR)/dtprobe

$(OS4_DIR)/dtprobe: tools/dtprobe.c
	@mkdir -p "$(dir $@)"
	$(OS4_CC) -mcrt=newlib -O2 $(PPC_WARN) -o "$@" "$<"

$(OS4_DIR)/src/wdt_libc.o: src/wdt_libc.c
	@mkdir -p "$(dir $@)"
	$(OS4_CC) $(OS4_CFLAGS) $(PPC_WARN) -fno-builtin \
		-fno-tree-loop-distribute-patterns -fno-strict-aliasing -c -o "$@" "$<"

$(OS4_OBJ): $(OS4_DIR)/%.o: %.c $(HEADERS)
	@mkdir -p "$(dir $@)"
	$(OS4_CC) $(OS4_CFLAGS) $(PPC_WARN) $(WEBP_CPPFLAGS) -Isrc -c -o "$@" "$<"

$(OS4_WEBP_OBJ): $(OS4_DIR)/%.o: %.c $(HEADERS)
	@mkdir -p "$(dir $@)"
	$(OS4_CC) $(OS4_CFLAGS) $(WEBP_CPPFLAGS) -c -o "$@" "$<"

$(OS4_DIR)/webp.datatype: $(OS4_OBJ) $(OS4_DIR)/src/wdt_libc.o $(OS4_WEBP_OBJ)
	$(OS4_CC) -mcrt=newlib -nostartfiles -nostdlib -o "$@" $^ -lgcc
	$(OS4_STRIP) --strip-unneeded --remove-section=.comment "$@"
	@strings "$@" | grep -q '^\$$VER: webp.datatype ' || \
		{ echo "$@: no version string" >&2; rm -f "$@"; exit 1; }

# --- a drawer per lane, for trying it on a machine -----------------------------
#
# The lanes that have been built, each with the library and the descriptor
# where they go under SYS: and DEVS:, the probe tool and the test pictures.

kit: descriptor
	@for lane in os3 morphos os4; do \
		test -f "$(BUILD)/$$lane/webp.datatype" || continue; \
		d="$(BUILD)/kit/$$lane"; \
		rm -rf "$$d"; \
		mkdir -p "$$d/Classes/DataTypes" "$$d/Devs/DataTypes" "$$d/pictures"; \
		cp "$(BUILD)/$$lane/webp.datatype" "$$d/Classes/DataTypes/"; \
		cp "$(BUILD)/descriptor/WebP" "$$d/Devs/DataTypes/"; \
		cp "$(BUILD)/$$lane/dtprobe" "$$d/"; \
		cp tests/pictures/*.webp "$$d/pictures/"; \
		echo "$$d"; \
	done

clean:
	rm -rf "$(BUILD)"
