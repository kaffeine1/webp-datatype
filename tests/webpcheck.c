/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Host side check of the decoder the datatype is built around.
 *
 *   webpcheck decode <file.webp> <out.pam>   decode, write RGBA as a PAM
 *   webpcheck planar <file.webp> <out.pam>   the picture as an old
 *                                            picture.datatype would get it:
 *                                            256 colours, then back to RGBA
 *   webpcheck damage <file.webp>             feed cut and corrupted copies,
 *                                            which must all fail cleanly
 *
 * The PAM header is written the way dwebp -pam writes it, so that the two
 * files can be compared byte for byte.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wdt_decode.h"
#include "wdt_quant.h"

static unsigned char *read_file(const char *path, size_t *size)
{
	FILE *f = fopen(path, "rb");
	unsigned char *data;
	long length;

	if (f == NULL)
		return NULL;
	fseek(f, 0, SEEK_END);
	length = ftell(f);
	fseek(f, 0, SEEK_SET);
	data = malloc(length > 0 ? (size_t)length : 1);
	if (data != NULL && fread(data, 1, (size_t)length, f) != (size_t)length) {
		free(data);
		data = NULL;
	}
	fclose(f);
	*size = (size_t)length;
	return data;
}

static FILE *open_pam(const char *path, const struct wdt_picture *pic)
{
	FILE *f = fopen(path, "wb");

	if (f != NULL)
		fprintf(f, "P7\nWIDTH %lu\nHEIGHT %lu\nDEPTH 4\nMAXVAL 255\n"
		           "TUPLTYPE RGB_ALPHA\nENDHDR\n", pic->width, pic->height);
	return f;
}

static int write_rgba(const char *path, const struct wdt_picture *pic)
{
	FILE *f = open_pam(path, pic);
	unsigned long x, y;

	if (f == NULL)
		return 0;
	for (y = 0; y < pic->height; y++) {
		const unsigned char *p = pic->pixels + y * pic->stride;

		for (x = 0; x < pic->width; x++, p += pic->bytes_per_pixel) {
			if (pic->bytes_per_pixel == 4) {
				fputc(p[1], f);
				fputc(p[2], f);
				fputc(p[3], f);
				fputc(p[0], f);
			} else {
				fputc(p[0], f);
				fputc(p[1], f);
				fputc(p[2], f);
				fputc(0xff, f);
			}
		}
	}
	return fclose(f) == 0;
}

static int cmd_decode(const char *in, const char *out)
{
	struct wdt_picture pic;
	unsigned char *data;
	size_t size;
	int error;

	data = read_file(in, &size);
	if (data == NULL) {
		fprintf(stderr, "%s: cannot read\n", in);
		return 2;
	}
	error = wdt_decode(data, size, &pic);
	free(data);
	if (error != WDT_OK) {
		fprintf(stderr, "%s: decode error %d\n", in, error);
		return 1;
	}
	printf("%s: %lux%lu %s %s frames=%lu bytes_per_pixel=%u\n", in, pic.width,
	       pic.height, pic.lossless ? "lossless" : "lossy",
	       pic.has_alpha ? "alpha" : "opaque", pic.frames, pic.bytes_per_pixel);
	if (!write_rgba(out, &pic)) {
		fprintf(stderr, "%s: cannot write\n", out);
		error = 2;
	}
	wdt_release(&pic);
	return error;
}

static int cmd_planar(const char *in, const char *out)
{
	struct wdt_picture pic;
	struct wdt_palette palette;
	unsigned char *data, *row;
	size_t size;
	unsigned long x, y;
	FILE *f;
	int error;

	data = read_file(in, &size);
	if (data == NULL)
		return 2;
	error = wdt_decode(data, size, &pic);
	free(data);
	if (error != WDT_OK)
		return 1;

	row = malloc(pic.width);
	if (row == NULL || !wdt_quant_begin(&palette, &pic, 256, 0xffffffUL)) {
		fprintf(stderr, "%s: no memory for the palette\n", in);
		return 2;
	}
	printf("%s: %u colours%s\n", in, palette.count,
	       palette.transparent >= 0 ? ", index 0 transparent" : "");
	f = open_pam(out, &pic);
	if (f == NULL)
		return 2;
	for (y = 0; y < pic.height; y++) {
		wdt_quant_row(&palette, &pic, y, row);
		for (x = 0; x < pic.width; x++) {
			const unsigned char *c = palette.rgb[row[x]];

			fputc(c[0], f);
			fputc(c[1], f);
			fputc(c[2], f);
			fputc(row[x] == palette.transparent ? 0 : 0xff, f);
		}
	}
	fclose(f);
	wdt_quant_end(&palette);
	free(row);
	wdt_release(&pic);
	return 0;
}

/* Every damaged copy must come back as an error, or as a picture: no crash. */
static int cmd_damage(const char *in)
{
	struct wdt_picture pic;
	unsigned char *data, *copy;
	size_t size, cut, i;
	unsigned long seed = 12345, decoded = 0, refused = 0;

	data = read_file(in, &size);
	if (data == NULL)
		return 2;
	copy = malloc(size);
	if (copy == NULL)
		return 2;

	for (cut = 0; cut < size; cut += (size > 4000 ? size / 400 : 1)) {
		memcpy(copy, data, cut);
		if (wdt_decode(copy, cut, &pic) == WDT_OK) {
			decoded++;
			wdt_release(&pic);
		} else {
			refused++;
		}
	}
	for (i = 0; i < 600; i++) {
		size_t at, n, k;

		memcpy(copy, data, size);
		seed = seed * 1103515245UL + 12345UL;
		n = 1 + (seed >> 16) % 4;
		for (k = 0; k < n; k++) {
			seed = seed * 1103515245UL + 12345UL;
			at = (seed >> 8) % size;
			seed = seed * 1103515245UL + 12345UL;
			copy[at] = (unsigned char)(seed >> 16);
		}
		if (wdt_decode(copy, size, &pic) == WDT_OK) {
			decoded++;
			wdt_release(&pic);
		} else {
			refused++;
		}
	}
	printf("%s: %lu damaged copies refused, %lu still decoded\n", in, refused,
	       decoded);
	free(copy);
	free(data);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc == 4 && strcmp(argv[1], "decode") == 0)
		return cmd_decode(argv[2], argv[3]);
	if (argc == 4 && strcmp(argv[1], "planar") == 0)
		return cmd_planar(argv[2], argv[3]);
	if (argc == 3 && strcmp(argv[1], "damage") == 0)
		return cmd_damage(argv[2]);
	fprintf(stderr, "usage: webpcheck decode|planar <file.webp> <out.pam>\n"
	                "       webpcheck damage <file.webp>\n");
	return 2;
}
