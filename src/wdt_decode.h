/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A WEBP file in memory in, one picture out. Nothing here knows about
 * AmigaOS: the same file is built for the host to test it.
 */
#ifndef WDT_DECODE_H
#define WDT_DECODE_H

#include <stddef.h>

enum {
	WDT_OK = 0,
	WDT_ERR_DATA,      /* not a WEBP file, or a damaged one */
	WDT_ERR_MEMORY,
	WDT_ERR_TOO_LARGE  /* more pixels than WDT_MAX_PIXELS */
};

/*
 * The format allows 16383 x 16383. A picture.datatype keeps its own copy of
 * what it is handed, so the cap is on what a machine can hold twice.
 */
#ifndef WDT_MAX_PIXELS
#define WDT_MAX_PIXELS (8192UL * 8192UL)
#endif

struct wdt_picture {
	unsigned long width;
	unsigned long height;
	unsigned bytes_per_pixel;  /* 3: R G B.  4: A R G B, alpha not premultiplied. */
	unsigned long stride;      /* bytes from one row to the next */
	unsigned char *pixels;     /* height rows; freed by wdt_release() */
	int has_alpha;             /* at least one pixel is not fully opaque */
	int lossless;              /* the picture (or the frame) is a VP8L stream */
	unsigned long frames;      /* 1 for a still picture */
};

/*
 * Decodes a still picture, or the first frame of an animation placed on its
 * canvas (what the canvas does not cover stays transparent). On WDT_OK the
 * caller owns pic->pixels until wdt_release().
 */
int wdt_decode(const unsigned char *data, size_t size, struct wdt_picture *pic);

void wdt_release(struct wdt_picture *pic);

/* 1 when the first bytes are "RIFF", a size and "WEBP". */
int wdt_is_webp(const unsigned char *data, size_t size);

#endif
