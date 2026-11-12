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
#
# AROS has a webp.datatype of its own in the system: there is no AROS lane.

BUILD ?= build
VERSION := 1
REVISION := 0

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
	$(WEBP)/src/utils/random_utils.c $(WEBP)/src/utils/rescaler_utils.c \
	$(WEBP)/src/utils/thread_utils.c $(WEBP)/src/utils/utils.c
WEBP_CPPFLAGS := -DHAVE_CONFIG_H -DNDEBUG -Ilibwebp-config -I$(WEBP)

CORE_SRC := src/wdt_decode.c src/wdt_quant.c
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

.PHONY: all test clean
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

clean:
	rm -rf "$(BUILD)"
