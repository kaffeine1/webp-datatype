/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * webp.datatype, a subclass of picture.datatype: what the lanes share.
 *
 * The only method of its own is OM_NEW. It lets picture.datatype build the
 * object, reads the WEBP file the object was made for, decodes it and hands
 * the pixels to picture.datatype, which from then on shows, scales, remaps,
 * prints and saves the picture like any other.
 *
 * How the pixels are handed over depends on the picture.datatype found:
 *
 *   V43 and later (every AmigaOS 4 and MorphOS, AmigaOS 3.5 and later, and
 *   AmigaOS 3.0/3.1 with the picture.datatype of a graphics card system):
 *   PDTM_WRITEPIXELARRAY with RGB, or ARGB when the picture has alpha. If
 *   that picture.datatype knows about alpha the header says mskHasAlpha
 *   and the alpha goes with the pixels; if not, the picture is laid on a
 *   plain background first.
 *
 *   V39 to V42 (plain AmigaOS 3.0/3.1): a planar BitMap of at most 256
 *   colours with its colour table, the way ilbm.datatype does it. The
 *   palette is chosen for the picture and the pixels are dithered.
 */
#include <stdlib.h>
#include <string.h>

#include "wdt_class.h"
#include "wdt_decode.h"
#include "wdt_quant.h"

/* PDTA_AlphaChannel came with picture.datatype 47.9: older headers lack it. */
#ifndef PDTA_AlphaChannel
#define PDTA_AlphaChannel (DTA_Dummy + 256)
#endif
#ifndef mskHasAlpha
#define mskHasAlpha 4
#endif
#ifndef DTST_MEMORY
#define DTST_MEMORY 5
#endif
#ifndef DTA_SourceAddress
#define DTA_SourceAddress (DTA_Dummy + 39)
#define DTA_SourceSize (DTA_Dummy + 40)
#endif

static ULONG get_attr(Class *cl, Object *o, ULONG attr, ULONG *storage)
{
	struct opGet get;

	get.MethodID = OM_GET;
	get.opg_AttrID = attr;
	get.opg_Storage = (APTR)storage;
	return wdt_super(cl, o, (Msg)&get);
}

static void set_attrs(Class *cl, Object *o, struct TagItem *tags)
{
	struct opSet set;

	set.MethodID = OM_SET;
	set.ops_AttrList = tags;
	set.ops_GInfo = NULL;
	wdt_super(cl, o, (Msg)&set);
}

/* --- reading -------------------------------------------------------------- */

/*
 * Reads the whole file behind a handle. A WEBP file says how long it is in
 * its first twelve bytes, so one block of the right size is enough and the
 * handle is only ever read forward, which also suits a source that cannot
 * seek.
 */
static LONG read_handle(BPTR handle, UBYTE **data, ULONG *size)
{
	UBYTE head[12];
	UBYTE *block;
	ULONG total, have;
	LONG got;

	Seek(handle, 0, OFFSET_BEGINNING);
	got = Read(handle, head, sizeof(head));
	if (got < 0)
		return IoErr();
	if (got != (LONG)sizeof(head) || !wdt_is_webp(head, sizeof(head)))
		return DTERROR_INVALID_DATA;

	total = (ULONG)head[4] | ((ULONG)head[5] << 8) | ((ULONG)head[6] << 16) |
	        ((ULONG)head[7] << 24);
	if (total < 4 || total > WDT_MAX_FILE)
		return DTERROR_INVALID_DATA;
	total += 8 + (total & 1);

	block = malloc(total);
	if (block == NULL)
		return ERROR_NO_FREE_STORE;
	memcpy(block, head, sizeof(head));
	have = sizeof(head);
	while (have < total) {
		got = Read(handle, block + have, (LONG)(total - have));
		if (got < 0) {
			LONG error = IoErr();

			free(block);
			return error;
		}
		if (got == 0)
			break;          /* shorter than it says: the decoder decides */
		have += (ULONG)got;
	}
	*data = block;
	*size = have;
	return 0;
}

/* --- decoding, on a stack of its own -------------------------------------- */

struct decode_job {
	const UBYTE *data;
	ULONG size;
	struct wdt_picture *picture;
};

static LONG decode_job(void *arg)
{
	struct decode_job *job = arg;

	return wdt_decode(job->data, job->size, job->picture);
}

static LONG decode(const UBYTE *data, ULONG size, struct wdt_picture *picture)
{
	struct decode_job job;

	job.data = data;
	job.size = size;
	job.picture = picture;
	switch (wdt_with_stack(decode_job, &job)) {
	case WDT_OK:
		return 0;
	case WDT_ERR_DATA:
		return DTERROR_INVALID_DATA;
	case WDT_ERR_TOO_LARGE:
		return ERROR_OBJECT_TOO_LARGE;
	default:
		return ERROR_NO_FREE_STORE;
	}
}

/* --- handing the pixels over ---------------------------------------------- */

/* Lays a picture with alpha on a plain colour, in place. */
static void flatten(struct wdt_picture *pic, ULONG background)
{
	LONG back[3];
	ULONG x, y;

	back[0] = (LONG)((background >> 16) & 0xff);
	back[1] = (LONG)((background >> 8) & 0xff);
	back[2] = (LONG)(background & 0xff);
	for (y = 0; y < pic->height; y++) {
		UBYTE *p = pic->pixels + y * pic->stride;

		for (x = 0; x < pic->width; x++, p += 4) {
			LONG a = p[0];

			if (a != 0xff) {
				p[1] = (UBYTE)((p[1] * a + back[0] * (255 - a) + 127) / 255);
				p[2] = (UBYTE)((p[2] * a + back[1] * (255 - a) + 127) / 255);
				p[3] = (UBYTE)((p[3] * a + back[2] * (255 - a) + 127) / 255);
				p[0] = 0xff;
			}
		}
	}
	pic->has_alpha = 0;
}

static void fill_header(struct BitMapHeader *bmh, const struct wdt_picture *pic,
                        UBYTE depth, UBYTE masking)
{
	bmh->bmh_Width = (UWORD)pic->width;
	bmh->bmh_Height = (UWORD)pic->height;
	bmh->bmh_Left = 0;
	bmh->bmh_Top = 0;
	bmh->bmh_Depth = depth;
	bmh->bmh_Masking = masking;
	bmh->bmh_Compression = cmpNone;
	bmh->bmh_Transparent = 0;
	bmh->bmh_XAspect = 1;
	bmh->bmh_YAspect = 1;
	bmh->bmh_PageWidth = (WORD)pic->width;
	bmh->bmh_PageHeight = (WORD)pic->height;
}

/* Does this picture.datatype carry an alpha channel through? */
static int keeps_alpha(Class *cl, Object *o)
{
#if defined(__amigaos4__) || defined(__MORPHOS__)
	(void)cl;
	(void)o;
	return 1;
#else
	ULONG value = 0;

	/* It answers the question only if it knows the attribute (47.9). */
	return get_attr(cl, o, PDTA_AlphaChannel, &value) != 0;
#endif
}

static LONG deliver_truecolour(Class *cl, Object *o, struct BitMapHeader *bmh,
                               struct wdt_picture *pic)
{
	struct pdtBlitPixelArray write;
	struct TagItem tags[5];
	int n = 0;

	if (pic->has_alpha && !keeps_alpha(cl, o))
		flatten(pic, WDT_BACKGROUND);

	fill_header(bmh, pic, pic->has_alpha ? 32 : 24,
	            pic->has_alpha ? mskHasAlpha : mskNone);

	tags[n].ti_Tag = DTA_NominalHoriz;
	tags[n++].ti_Data = pic->width;
	tags[n].ti_Tag = DTA_NominalVert;
	tags[n++].ti_Data = pic->height;
	tags[n].ti_Tag = PDTA_SourceMode;
	tags[n++].ti_Data = PMODE_V43;
	if (pic->has_alpha) {
		tags[n].ti_Tag = PDTA_AlphaChannel;
		tags[n++].ti_Data = TRUE;
	}
	tags[n].ti_Tag = TAG_DONE;
	tags[n].ti_Data = 0;
	set_attrs(cl, o, tags);

	write.MethodID = PDTM_WRITEPIXELARRAY;
	write.pbpa_PixelData = pic->pixels;
	write.pbpa_PixelFormat = pic->bytes_per_pixel == 4 ? PBPAFMT_ARGB : PBPAFMT_RGB;
	write.pbpa_PixelArrayMod = pic->stride;
	write.pbpa_Left = 0;
	write.pbpa_Top = 0;
	write.pbpa_Width = pic->width;
	write.pbpa_Height = pic->height;
	if (!wdt_super(cl, o, (Msg)&write))
		return ERROR_NO_FREE_STORE;
	return 0;
}

#if WDT_PLANAR_FALLBACK

/* One row of palette entries into the bit planes. */
static void put_row(struct BitMap *bm, ULONG y, const UBYTE *entries,
                    ULONG width, unsigned depth)
{
	unsigned plane;

	for (plane = 0; plane < depth; plane++) {
		UBYTE *to = (UBYTE *)bm->Planes[plane] + y * bm->BytesPerRow;
		UBYTE bits = 0;
		ULONG x;

		for (x = 0; x < width; x++) {
			bits = (UBYTE)((bits << 1) | ((entries[x] >> plane) & 1));
			if ((x & 7) == 7) {
				*to++ = bits;
				bits = 0;
			}
		}
		if (width & 7)
			*to = (UBYTE)(bits << (8 - (width & 7)));
	}
}

static LONG deliver_planar(Class *cl, Object *o, struct BitMapHeader *bmh,
                           struct wdt_picture *pic)
{
	struct wdt_palette palette;
	struct ColorRegister *registers = NULL;
	ULONG *cregs = NULL;
	struct BitMap *bm = NULL;
	UBYTE *row = NULL;
	struct TagItem tags[5];
	ULONG colours, mode, i, y;
	unsigned depth;
	LONG error = ERROR_NO_FREE_STORE;

	if (!wdt_quant_begin(&palette, pic, 256, WDT_BACKGROUND))
		return ERROR_NO_FREE_STORE;
	depth = wdt_quant_depth(palette.count);
	colours = 1UL << depth;

	fill_header(bmh, pic, (UBYTE)depth,
	            palette.transparent >= 0 ? mskHasTransparentColor : mskNone);

	/* Asking for the colours makes picture.datatype allocate its tables. */
	tags[0].ti_Tag = PDTA_NumColors;
	tags[0].ti_Data = colours;
	tags[1].ti_Tag = TAG_DONE;
	tags[1].ti_Data = 0;
	set_attrs(cl, o, tags);
	get_attr(cl, o, PDTA_ColorRegisters, (ULONG *)&registers);
	get_attr(cl, o, PDTA_CRegs, (ULONG *)&cregs);
	if (registers == NULL || cregs == NULL)
		goto out;
	for (i = 0; i < colours; i++) {
		unsigned k;
		UBYTE gun[3] = { 0, 0, 0 };

		if (i < palette.count)
			memcpy(gun, palette.rgb[i], 3);
		registers[i].red = gun[0];
		registers[i].green = gun[1];
		registers[i].blue = gun[2];
		for (k = 0; k < 3; k++)
			cregs[i * 3 + k] = gun[k] * 0x01010101UL;
	}

	row = malloc(pic->width);
	bm = AllocBitMap(pic->width, pic->height, depth, BMF_CLEAR, NULL);
	if (row == NULL || bm == NULL)
		goto out;
	for (y = 0; y < pic->height; y++) {
		wdt_quant_row(&palette, pic, y, row);
		put_row(bm, y, row, pic->width, depth);
	}

	mode = BestModeID(BIDTAG_NominalWidth, pic->width,
	                  BIDTAG_NominalHeight, pic->height,
	                  BIDTAG_Depth, depth, TAG_DONE);
	if (mode == (ULONG)INVALID_ID)
		mode = 0;

	tags[0].ti_Tag = DTA_NominalHoriz;
	tags[0].ti_Data = pic->width;
	tags[1].ti_Tag = DTA_NominalVert;
	tags[1].ti_Data = pic->height;
	tags[2].ti_Tag = PDTA_ModeID;
	tags[2].ti_Data = mode;
	tags[3].ti_Tag = PDTA_BitMap;      /* picture.datatype frees it from now on */
	tags[3].ti_Data = (ULONG)bm;
	tags[4].ti_Tag = TAG_DONE;
	tags[4].ti_Data = 0;
	set_attrs(cl, o, tags);
	bm = NULL;
	error = 0;

out:
	if (bm != NULL)
		FreeBitMap(bm);
	free(row);
	wdt_quant_end(&palette);
	return error;
}

#endif /* WDT_PLANAR_FALLBACK */

/* --- OM_NEW ---------------------------------------------------------------- */

static LONG load(Class *cl, Object *o)
{
	struct BitMapHeader *bmh = NULL;
	struct wdt_picture picture;
	ULONG source = DTST_FILE, handle = 0;
	UBYTE *file = NULL;
	const UBYTE *data = NULL;
	ULONG size = 0;
	LONG error;

	get_attr(cl, o, DTA_SourceType, &source);
	if (source == DTST_RAM)
		return 0;       /* an empty picture, made to be filled by someone */

	get_attr(cl, o, PDTA_BitMapHeader, (ULONG *)&bmh);
	if (bmh == NULL)
		return ERROR_OBJECT_WRONG_TYPE;

	get_attr(cl, o, DTA_Handle, &handle);
	if (source == DTST_FILE && handle != 0) {
		error = read_handle((BPTR)handle, &file, &size);
		if (error != 0)
			return error;
		data = file;
	} else if (source == DTST_MEMORY) {
		/* Where the system does not dress memory up as a file. */
		ULONG address = 0;

		get_attr(cl, o, DTA_SourceAddress, &address);
		get_attr(cl, o, DTA_SourceSize, &size);
		data = (const UBYTE *)address;
		if (data == NULL || size == 0)
			return ERROR_REQUIRED_ARG_MISSING;
	} else {
		return ERROR_OBJECT_WRONG_TYPE;
	}

	error = decode(data, size, &picture);
	free(file);
	if (error != 0)
		return error;

	if (picture.width > 0xffffUL || picture.height > 0xffffUL) {
		error = ERROR_OBJECT_TOO_LARGE;
	} else {
#if WDT_PLANAR_FALLBACK
		if (wdt_picture_base->lib_Version < 43)
			error = deliver_planar(cl, o, bmh, &picture);
		else
#endif
			error = deliver_truecolour(cl, o, bmh, &picture);
	}
	wdt_release(&picture);
	return error;
}

static ULONG new_object(Class *cl, Object *o, Msg msg)
{
	Object *object = (Object *)wdt_super(cl, o, msg);

	if (object != NULL) {
		LONG error = load(cl, object);

		if (error != 0) {
			ULONG dispose = OM_DISPOSE;

			/* This class keeps nothing of its own: the rest is picture.datatype's. */
			wdt_super(cl, object, (Msg)&dispose);
			SetIoErr(error);
			object = NULL;
		}
	}
	return (ULONG)object;
}

ULONG wdt_dispatch(Class *cl, Object *o, Msg msg)
{
	if (msg->MethodID == OM_NEW)
		return new_object(cl, o, msg);
	return wdt_super(cl, o, msg);
}
