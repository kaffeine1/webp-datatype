/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * True colour down to a palette: median cut over a histogram of 15 bit
 * colours, then Floyd-Steinberg dithering row by row.
 */
#include <stdlib.h>
#include <string.h>

#include "wdt_quant.h"

#define CELLS 32768U            /* 5 bits for each of red, green, blue */
#define CELL(r, g, b) ((((unsigned)(r) >> 3) << 10) | (((unsigned)(g) >> 3) << 5) | ((unsigned)(b) >> 3))
#define HALF_OPAQUE 128

struct cell {
	unsigned char gun[3];   /* 0 to 31 */
	unsigned char pad;
	unsigned long count;
};

struct box {
	unsigned first, cells;  /* a run of the cell array */
	unsigned long weight;   /* pixels in it */
	unsigned char low[3], high[3];
};

/* The colour a pixel has once it stands on the background. */
static void pixel_rgb(const struct wdt_picture *pic, const unsigned char *p,
                      unsigned long background, int rgb[3], int *alpha)
{
	if (pic->bytes_per_pixel == 4) {
		int a = p[0];

		*alpha = a;
		if (a == 0xff) {
			rgb[0] = p[1];
			rgb[1] = p[2];
			rgb[2] = p[3];
		} else {
			int back[3], i;

			back[0] = (int)((background >> 16) & 0xff);
			back[1] = (int)((background >> 8) & 0xff);
			back[2] = (int)(background & 0xff);
			for (i = 0; i < 3; i++)
				rgb[i] = (p[1 + i] * a + back[i] * (255 - a) + 127) / 255;
		}
	} else {
		*alpha = 0xff;
		rgb[0] = p[0];
		rgb[1] = p[1];
		rgb[2] = p[2];
	}
}

static void box_fit(struct box *box, const struct cell *cells)
{
	unsigned i, k;

	box->weight = 0;
	for (k = 0; k < 3; k++) {
		box->low[k] = 31;
		box->high[k] = 0;
	}
	for (i = 0; i < box->cells; i++) {
		const struct cell *c = &cells[box->first + i];

		box->weight += c->count;
		for (k = 0; k < 3; k++) {
			if (c->gun[k] < box->low[k])
				box->low[k] = c->gun[k];
			if (c->gun[k] > box->high[k])
				box->high[k] = c->gun[k];
		}
	}
}

static unsigned box_longest(const struct box *box, unsigned *axis)
{
	unsigned k, best = 0, side;

	/* Green first: on a tie the eye's best gun is the one to split. */
	static const unsigned char order[3] = { 1, 0, 2 };

	*axis = 1;
	for (k = 0; k < 3; k++) {
		side = (unsigned)(box->high[order[k]] - box->low[order[k]]);
		if (side > best) {
			best = side;
			*axis = order[k];
		}
	}
	return best;
}

/* Puts a box's cells in order along one gun: a counting sort, 32 values. */
static void box_sort(const struct box *box, struct cell *cells,
                     struct cell *spare, unsigned axis)
{
	unsigned start[33];
	unsigned i;

	memset(start, 0, sizeof(start));
	for (i = 0; i < box->cells; i++)
		start[cells[box->first + i].gun[axis] + 1]++;
	for (i = 1; i < 33; i++)
		start[i] += start[i - 1];
	for (i = 0; i < box->cells; i++) {
		const struct cell *c = &cells[box->first + i];

		spare[start[c->gun[axis]]++] = *c;
	}
	memcpy(&cells[box->first], spare, box->cells * sizeof(struct cell));
}

/* Splits the boxes until there are enough, and averages each into a colour. */
static unsigned median_cut(struct cell *cells, struct cell *spare,
                           unsigned ncells, unsigned wanted,
                           unsigned char rgb[][3])
{
	struct box boxes[256];
	unsigned count = 1, i, k;

	boxes[0].first = 0;
	boxes[0].cells = ncells;
	box_fit(&boxes[0], cells);

	while (count < wanted) {
		unsigned long best_score = 0, half, sum;
		unsigned best = 0, axis = 1, cut;
		struct box *b, *n;

		for (i = 0; i < count; i++) {
			unsigned side, a;
			unsigned long score;

			if (boxes[i].cells < 2)
				continue;
			side = box_longest(&boxes[i], &a);
			score = boxes[i].weight * side;
			if (score > best_score) {
				best_score = score;
				best = i;
				axis = a;
			}
		}
		if (best_score == 0)
			break;          /* every box is one colour already */

		b = &boxes[best];
		box_sort(b, cells, spare, axis);
		half = b->weight / 2;
		sum = 0;
		/* The cells before the cut stay: at least one, and not all. */
		for (cut = 1; cut < b->cells - 1; cut++) {
			sum += cells[b->first + cut - 1].count;
			if (sum >= half)
				break;
		}

		n = &boxes[count++];
		n->first = b->first + cut;
		n->cells = b->cells - cut;
		b->cells = cut;
		box_fit(b, cells);
		box_fit(n, cells);
	}

	for (i = 0; i < count; i++) {
		unsigned long sum[3] = { 0, 0, 0 };
		unsigned j;

		for (j = 0; j < boxes[i].cells; j++) {
			const struct cell *c = &cells[boxes[i].first + j];

			for (k = 0; k < 3; k++)
				sum[k] += c->gun[k] * c->count;
		}
		/* 0..31 to 0..255. The counts were scaled so this cannot overflow. */
		for (k = 0; k < 3; k++)
			rgb[i][k] = (unsigned char)((sum[k] * 255 + boxes[i].weight * 15) /
			                            (boxes[i].weight * 31));
	}
	return count;
}

/* Orders the opaque entries by green, so that a search can stop early. */
static void index_by_green(struct wdt_palette *p, unsigned first)
{
	unsigned i, j, g;

	p->opaque = p->count - first;
	for (i = 0; i < p->opaque; i++) {
		unsigned char entry = (unsigned char)(first + i);

		for (j = i; j > 0 && p->rgb[p->by_green[j - 1]][1] > p->rgb[entry][1]; j--)
			p->by_green[j] = p->by_green[j - 1];
		p->by_green[j] = entry;
	}
	j = 0;
	for (g = 0; g < 256; g++) {
		while (j < p->opaque && p->rgb[p->by_green[j]][1] < g)
			j++;
		p->first_at[g] = (unsigned char)(j < p->opaque ? j : p->opaque - 1);
	}
}

static unsigned long distance(const unsigned char *c, int r, int g, int b)
{
	long dr = c[0] - r, dg = c[1] - g, db = c[2] - b;

	return (unsigned long)(2 * dr * dr + 4 * dg * dg + db * db);
}

/* The closest opaque entry, walking out from the nearest green. */
static unsigned char search(const struct wdt_palette *p, int r, int g, int b)
{
	int up = p->first_at[g], down = up - 1;
	unsigned long best = ~0UL;
	unsigned char found = p->by_green[up];

	while (up < (int)p->opaque || down >= 0) {
		long du = up < (int)p->opaque ? p->rgb[p->by_green[up]][1] - g : 512;
		long dd = down >= 0 ? g - p->rgb[p->by_green[down]][1] : 512;
		unsigned char entry;
		unsigned long d;

		if (du <= dd) {
			if ((unsigned long)(4 * du * du) >= best)
				break;
			entry = p->by_green[up++];
		} else {
			if ((unsigned long)(4 * dd * dd) >= best)
				break;
			entry = p->by_green[down--];
		}
		d = distance(p->rgb[entry], r, g, b);
		if (d < best) {
			best = d;
			found = entry;
		}
	}
	return found;
}

static unsigned char nearest(struct wdt_palette *p, int r, int g, int b)
{
	unsigned cell = CELL(r, g, b);

	if (!(p->known[cell >> 3] & (1 << (cell & 7)))) {
		/* The middle of the cell, not the pixel: the answer is kept. */
		p->nearest[cell] = search(p, (r & 0xf8) | 4, (g & 0xf8) | 4, (b & 0xf8) | 4);
		p->known[cell >> 3] |= (unsigned char)(1 << (cell & 7));
	}
	return p->nearest[cell];
}

unsigned wdt_quant_depth(unsigned colours)
{
	unsigned depth = 1;

	while (depth < 8 && (1U << depth) < colours)
		depth++;
	return depth;
}

void wdt_quant_end(struct wdt_palette *p)
{
	free(p->nearest);
	free(p->known);
	free(p->errors);
	p->nearest = NULL;
	p->known = NULL;
	p->errors = NULL;
}

int wdt_quant_begin(struct wdt_palette *p, const struct wdt_picture *pic,
                    unsigned max_colours, unsigned long background)
{
	unsigned long *histogram;
	struct cell *cells = NULL, *spare = NULL;
	unsigned long x, y, opaque_pixels = 0;
	unsigned ncells = 0, i, first, shift = 0;
	int see_through = 0;

	memset(p, 0, sizeof(*p));
	p->transparent = -1;
	p->background = background;
	if (max_colours > 256)
		max_colours = 256;
	if (max_colours < 2)
		max_colours = 2;

	histogram = calloc(CELLS, sizeof(*histogram));
	p->nearest = malloc(CELLS);
	p->known = calloc(CELLS / 8, 1);
	p->errors = calloc((pic->width + 2) * 3 * 2, sizeof(short));
	if (histogram == NULL || p->nearest == NULL || p->known == NULL ||
	    p->errors == NULL)
		goto no_memory;

	for (y = 0; y < pic->height; y++) {
		const unsigned char *px = pic->pixels + y * pic->stride;

		for (x = 0; x < pic->width; x++, px += pic->bytes_per_pixel) {
			int rgb[3], alpha;

			pixel_rgb(pic, px, background, rgb, &alpha);
			if (alpha < HALF_OPAQUE) {
				see_through = 1;
				continue;
			}
			if (histogram[CELL(rgb[0], rgb[1], rgb[2])]++ == 0)
				ncells++;
			opaque_pixels++;
		}
	}

	first = 0;
	if (see_through) {
		/* Entry 0 is the hole, painted in the background colour. */
		p->transparent = 0;
		p->rgb[0][0] = (unsigned char)(background >> 16);
		p->rgb[0][1] = (unsigned char)(background >> 8);
		p->rgb[0][2] = (unsigned char)background;
		first = 1;
	}

	if (ncells == 0) {
		/* Nothing opaque at all: one more entry so that there are two. */
		p->rgb[first][0] = p->rgb[first][1] = p->rgb[first][2] = 0;
		p->count = first + 1;
	} else {
		cells = malloc(ncells * sizeof(*cells));
		spare = malloc(ncells * sizeof(*spare));
		if (cells == NULL || spare == NULL)
			goto no_memory;

		/* Keep the sums of median_cut() inside 32 bits. */
		while ((opaque_pixels >> shift) > 262144UL)
			shift++;
		ncells = 0;
		for (i = 0; i < CELLS; i++) {
			if (histogram[i] == 0)
				continue;
			cells[ncells].gun[0] = (unsigned char)(i >> 10);
			cells[ncells].gun[1] = (unsigned char)((i >> 5) & 31);
			cells[ncells].gun[2] = (unsigned char)(i & 31);
			cells[ncells].pad = 0;
			cells[ncells].count = histogram[i] >> shift;
			if (cells[ncells].count == 0)
				cells[ncells].count = 1;
			ncells++;
		}
		p->count = first + median_cut(cells, spare, ncells, max_colours - first,
		                              &p->rgb[first]);
	}

	index_by_green(p, first);
	free(spare);
	free(cells);
	free(histogram);
	return 1;

no_memory:
	free(spare);
	free(cells);
	free(histogram);
	wdt_quant_end(p);
	return 0;
}

void wdt_quant_row(struct wdt_palette *p, const struct wdt_picture *pic,
                   unsigned long y, unsigned char *entries)
{
	const unsigned char *px = pic->pixels + y * pic->stride;
	unsigned long width = pic->width, x;
	/* Two rows of errors, each with a spare pixel at both ends. */
	short *here = p->errors + (y & 1) * (width + 2) * 3 + 3;
	short *below = p->errors + ((y + 1) & 1) * (width + 2) * 3 + 3;
	int k;

	if (y == 0 || y != p->next_row)
		memset(p->errors, 0, (width + 2) * 3 * 2 * sizeof(short));
	p->next_row = y + 1;
	memset(below - 3, 0, (width + 2) * 3 * sizeof(short));

	for (x = 0; x < width; x++, px += pic->bytes_per_pixel) {
		int rgb[3], want[3], alpha;
		const unsigned char *got;
		unsigned char entry;

		pixel_rgb(pic, px, p->background, rgb, &alpha);
		if (alpha < HALF_OPAQUE) {
			entries[x] = 0;
			continue;       /* a hole takes no error and gives none */
		}
		for (k = 0; k < 3; k++) {
			want[k] = rgb[k] + (here[x * 3 + k] + 8) / 16;
			if (want[k] < 0)
				want[k] = 0;
			if (want[k] > 255)
				want[k] = 255;
		}
		entry = nearest(p, want[0], want[1], want[2]);
		entries[x] = entry;
		got = p->rgb[entry];
		for (k = 0; k < 3; k++) {
			int e = want[k] - got[k];

			here[(x + 1) * 3 + k] += (short)(e * 7);
			below[((long)x - 1) * 3 + k] += (short)(e * 3);
			below[x * 3 + k] += (short)(e * 5);
			below[(x + 1) * 3 + k] += (short)e;
		}
	}
}
