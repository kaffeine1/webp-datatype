/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A WEBP file in memory in, one picture out, through libwebp.
 */
#include <stdlib.h>
#include <string.h>

#include "src/webp/decode.h"
#include "src/webp/demux.h"

#include "wdt_decode.h"

int wdt_is_webp(const unsigned char *data, size_t size)
{
	return data != NULL && size >= 12 &&
	       memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WEBP", 4) == 0;
}

void wdt_release(struct wdt_picture *pic)
{
	if (pic != NULL) {
		free(pic->pixels);
		pic->pixels = NULL;
	}
}

static int status_to_error(VP8StatusCode status)
{
	if (status == VP8_STATUS_OK)
		return WDT_OK;
	if (status == VP8_STATUS_OUT_OF_MEMORY)
		return WDT_ERR_MEMORY;
	return WDT_ERR_DATA;
}

/* The buffer for the whole picture, or NULL with the reason in *error. */
static unsigned char *alloc_pixels(struct wdt_picture *pic, int clear, int *error)
{
	unsigned long pixels;
	unsigned char *buffer;

	if (pic->width == 0 || pic->height == 0) {
		*error = WDT_ERR_DATA;
		return NULL;
	}
	/* Both sides are below 16384, so the product fits 32 bits. */
	pixels = pic->width * pic->height;
	if (pixels > WDT_MAX_PIXELS) {
		*error = WDT_ERR_TOO_LARGE;
		return NULL;
	}
	pic->stride = pic->width * pic->bytes_per_pixel;
	if (clear)
		buffer = calloc(pixels, pic->bytes_per_pixel);
	else
		buffer = malloc(pixels * pic->bytes_per_pixel);
	if (buffer == NULL)
		*error = WDT_ERR_MEMORY;
	return buffer;
}

/*
 * Decodes one bitstream (a whole still file, or the payload of one frame)
 * into a rectangle of pic->pixels.
 */
static int decode_into(const unsigned char *data, size_t size,
                       struct wdt_picture *pic, unsigned long left,
                       unsigned long top, unsigned long width,
                       unsigned long height)
{
	WebPDecoderConfig config;
	VP8StatusCode status;

	if (!WebPInitDecoderConfig(&config))
		return WDT_ERR_DATA;
	status = WebPGetFeatures(data, size, &config.input);
	if (status != VP8_STATUS_OK)
		return status_to_error(status);
	if ((unsigned long)config.input.width != width ||
	    (unsigned long)config.input.height != height)
		return WDT_ERR_DATA;

	config.output.colorspace = pic->bytes_per_pixel == 4 ? MODE_ARGB : MODE_RGB;
	config.output.is_external_memory = 1;
	config.output.u.RGBA.rgba =
		pic->pixels + top * pic->stride + left * pic->bytes_per_pixel;
	config.output.u.RGBA.stride = (int)pic->stride;
	config.output.u.RGBA.size =
		(size_t)(pic->stride * (height - 1) + width * pic->bytes_per_pixel);

	status = WebPDecode(data, size, &config);
	WebPFreeDecBuffer(&config.output);
	return status_to_error(status);
}

/* A file may say it has alpha and then carry none worth the name. */
static int any_alpha(const struct wdt_picture *pic)
{
	unsigned long x, y;

	if (pic->bytes_per_pixel != 4)
		return 0;
	for (y = 0; y < pic->height; y++) {
		const unsigned char *row = pic->pixels + y * pic->stride;

		for (x = 0; x < pic->width; x++)
			if (row[x * 4] != 0xff)
				return 1;
	}
	return 0;
}

static int decode_still(const unsigned char *data, size_t size,
                        struct wdt_picture *pic)
{
	WebPBitstreamFeatures features;
	VP8StatusCode status;
	int error = WDT_OK;

	status = WebPGetFeatures(data, size, &features);
	if (status != VP8_STATUS_OK)
		return status_to_error(status);

	pic->width = (unsigned long)features.width;
	pic->height = (unsigned long)features.height;
	pic->bytes_per_pixel = features.has_alpha ? 4 : 3;
	pic->lossless = features.format == 2;
	pic->frames = 1;
	pic->pixels = alloc_pixels(pic, 0, &error);
	if (pic->pixels == NULL)
		return error;
	return decode_into(data, size, pic, 0, 0, pic->width, pic->height);
}

static int decode_first_frame(const unsigned char *data, size_t size,
                              struct wdt_picture *pic)
{
	WebPData file;
	WebPDemuxer *demux;
	WebPIterator frame;
	int error = WDT_OK;

	file.bytes = data;
	file.size = size;
	demux = WebPDemux(&file);
	if (demux == NULL)
		return WDT_ERR_DATA;

	pic->width = WebPDemuxGetI(demux, WEBP_FF_CANVAS_WIDTH);
	pic->height = WebPDemuxGetI(demux, WEBP_FF_CANVAS_HEIGHT);
	pic->frames = WebPDemuxGetI(demux, WEBP_FF_FRAME_COUNT);
	pic->bytes_per_pixel = 4;

	memset(&frame, 0, sizeof(frame));
	if (!WebPDemuxGetFrame(demux, 1, &frame)) {
		WebPDemuxDelete(demux);
		return WDT_ERR_DATA;
	}
	if (frame.x_offset < 0 || frame.y_offset < 0 || frame.width <= 0 ||
	    frame.height <= 0 ||
	    (unsigned long)frame.x_offset + (unsigned long)frame.width > pic->width ||
	    (unsigned long)frame.y_offset + (unsigned long)frame.height > pic->height) {
		error = WDT_ERR_DATA;
	} else {
		/* Cleared: the canvas starts transparent, as the browsers show it. */
		pic->pixels = alloc_pixels(pic, 1, &error);
	}
	if (error == WDT_OK) {
		WebPBitstreamFeatures features;

		if (WebPGetFeatures(frame.fragment.bytes, frame.fragment.size,
		                    &features) == VP8_STATUS_OK)
			pic->lossless = features.format == 2;
		error = decode_into(frame.fragment.bytes, frame.fragment.size, pic,
		                    (unsigned long)frame.x_offset,
		                    (unsigned long)frame.y_offset,
		                    (unsigned long)frame.width,
		                    (unsigned long)frame.height);
	}
	WebPDemuxReleaseIterator(&frame);
	WebPDemuxDelete(demux);
	return error;
}

int wdt_decode(const unsigned char *data, size_t size, struct wdt_picture *pic)
{
	WebPBitstreamFeatures features;
	VP8StatusCode status;
	int error;

	memset(pic, 0, sizeof(*pic));
	if (!wdt_is_webp(data, size))
		return WDT_ERR_DATA;

	status = WebPGetFeatures(data, size, &features);
	if (status != VP8_STATUS_OK)
		return status_to_error(status);

	if (features.has_animation)
		error = decode_first_frame(data, size, pic);
	else
		error = decode_still(data, size, pic);

	if (error != WDT_OK) {
		wdt_release(pic);
		return error;
	}
	pic->has_alpha = any_alpha(pic);
	return WDT_OK;
}
