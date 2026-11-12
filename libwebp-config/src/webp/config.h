/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Build configuration of the libwebp copy in third_party/libwebp.
 *
 * libwebp includes "src/webp/config.h" when HAVE_CONFIG_H is defined. With
 * that macro set and no WEBP_HAVE_* macro here, cpu.h leaves SSE2, SSE4.1,
 * AVX2 and NEON off, so the plain C code is the only code, on every
 * machine, and the files that hold the other versions need not be here.
 */
#ifndef WEBP_DATATYPE_LIBWEBP_CONFIG_H
#define WEBP_DATATYPE_LIBWEBP_CONFIG_H

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define WORDS_BIGENDIAN 1
#endif

/* No scaling and no cropping while decoding: a datatype never asks. */
#define WEBP_REDUCE_SIZE 1

#endif
