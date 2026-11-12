/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * True colour down to a palette of at most 256 colours, for a
 * picture.datatype older than V43, which only takes a planar BitMap and a
 * colour table. The palette is made for the picture (median cut) and the
 * rows are dithered (Floyd-Steinberg). Nothing here knows about AmigaOS.
 */
#ifndef WDT_QUANT_H
#define WDT_QUANT_H

#include "wdt_decode.h"

struct wdt_palette {
	unsigned count;            /* entries of rgb[] in use, 1 to 256 */
	int transparent;           /* 0 when entry 0 means "see through", else -1 */
	unsigned char rgb[256][3];

	/* The rest belongs to wdt_quant.c. */
	unsigned char *nearest;    /* palette entry for each 15 bit colour */
	unsigned char *known;      /* one bit per entry of nearest[] */
	short *errors;             /* two rows of what dithering carries over */
	unsigned char by_green[256];   /* opaque entries in order of green */
	unsigned char first_at[256];   /* where in by_green[] a green starts */
	unsigned opaque;           /* how many entries by_green[] holds */
	unsigned long background;  /* 0xRRGGBB behind translucent pixels */
	unsigned long next_row;
};

/*
 * Chooses the palette of a decoded picture. Pixels less than half opaque
 * become entry 0, which is then kept for them alone; the others are mixed
 * with the background colour first. Returns 0 when memory runs out.
 */
int wdt_quant_begin(struct wdt_palette *palette, const struct wdt_picture *pic,
                    unsigned max_colours, unsigned long background);

/*
 * One row of the picture as palette entries, one byte per pixel. Rows must
 * be asked for in order, from 0, because each row hands its rounding errors
 * to the next.
 */
void wdt_quant_row(struct wdt_palette *palette, const struct wdt_picture *pic,
                   unsigned long y, unsigned char *entries);

void wdt_quant_end(struct wdt_palette *palette);

/* How many bit planes hold that many colours (1 to 8). */
unsigned wdt_quant_depth(unsigned colours);

#endif
