/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * dtprobe: opens a picture the way any program does, through
 * datatypes.library, says what came back and saves the pixels.
 *
 *   dtprobe <picture> [<out.pam>]
 *
 * It knows nothing about WEBP: it shows what the datatype that the system
 * picked made of the file. The pixels are written as a PAM (RGBA), with
 * the header dwebp -pam writes, so that the file can be compared on
 * another machine with what libwebp's own tools decode.
 *
 * With a picture.datatype V43 or later the pixels are read back with
 * PDTM_READPIXELARRAY as ARGB. With an older one the picture is a planar
 * BitMap with a colour table, and that is what gets converted. Either way
 * it is the picture as the datatype delivered it: nothing is remapped to a
 * screen, so no screen is needed and none is asked for.
 *
 * Builds for AmigaOS 3 (m68k-amigaos-gcc -noixemul), AmigaOS 4 and MorphOS.
 */
#if defined(__amigaos4__) && !defined(__USE_INLINE__)
#define __USE_INLINE__
#endif

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <dos/dos.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <graphics/gfx.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>

/*
 * The base gets a name of its own: with the usual one, the C library of the
 * 68k compiler opens and closes datatypes.library by itself as well.
 */
#define DATATYPES_BASE_NAME ProbeDataTypesBase

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/datatypes.h>
#if defined(__amigaos4__)
#include <proto/intuition.h>
#else
#include <clib/alib_protos.h>
#endif

/* Every line is flushed: if a datatype brings the program down, what was
 * said before stays in the output file. */
#if defined(__amigaos4__)
#define SAY(...) do { Printf(__VA_ARGS__); FFlush(Output()); } while (0)
#else
#define SAY(...) do { Printf(__VA_ARGS__); Flush(Output()); } while (0)
#endif

#ifndef PDTA_AlphaChannel
#define PDTA_AlphaChannel (DTA_Dummy + 256)
#endif

struct Library *ProbeDataTypesBase;
#if defined(__amigaos4__)
struct DataTypesIFace *IDataTypes;
struct Library *ProbeIntuitionBase;
struct IntuitionIFace *IIntuition;
#endif

static const char version[] __attribute__((used)) =
	"$VER: dtprobe 1.0 (12.11.2026)";

/*
 * A method sent straight to the object. DoDTMethodA() is for methods that
 * carry a GadgetInfo: it writes one into the second field of any message,
 * which in a pdtBlitPixelArray is the address of the pixels.
 */
static ULONG do_method(Object *o, Msg msg)
{
#if defined(__amigaos4__)
	return IDoMethodA(o, msg);
#else
	return DoMethodA(o, msg);
#endif
}

static APTR get_memory(ULONG size)
{
#if defined(__amigaos4__)
	return AllocVecTags(size, AVT_Type, MEMF_SHARED, AVT_ClearWithValue, 0,
	                    TAG_DONE);
#else
	return AllocVec(size, MEMF_ANY | MEMF_CLEAR);
#endif
}

static LONG ticks(void)
{
	struct DateStamp now;

	DateStamp(&now);
	return now.ds_Minute * 3000L + now.ds_Tick;
}

static void put_text(BPTR file, const char *text)
{
	LONG length = 0;

	while (text[length] != 0)
		length++;
	Write(file, (APTR)text, length);
}

static void put_number(BPTR file, ULONG value)
{
	char digits[12];
	int at = sizeof(digits);

	digits[--at] = 0;
	do {
		digits[--at] = (char)('0' + value % 10);
		value /= 10;
	} while (value != 0);
	put_text(file, &digits[at]);
}

static BPTR open_pam(const char *name, ULONG width, ULONG height)
{
	BPTR file = Open((STRPTR)name, MODE_NEWFILE);

	if (file != 0) {
		put_text(file, "P7\nWIDTH ");
		put_number(file, width);
		put_text(file, "\nHEIGHT ");
		put_number(file, height);
		put_text(file, "\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n");
	}
	return file;
}

/* ARGB rows to an RGBA file. */
static BOOL save_argb(const char *name, UBYTE *argb, ULONG width, ULONG height,
                      BOOL alpha)
{
	BPTR file = open_pam(name, width, height);
	ULONG x, y;
	BOOL ok = TRUE;

	if (file == 0)
		return FALSE;
	for (y = 0; y < height && ok; y++) {
		UBYTE *p = argb + y * width * 4;

		for (x = 0; x < width; x++, p += 4) {
			UBYTE a = alpha ? p[0] : 0xff;

			p[0] = p[1];
			p[1] = p[2];
			p[2] = p[3];
			p[3] = a;
		}
		p = argb + y * width * 4;
		ok = Write(file, p, (LONG)(width * 4)) == (LONG)(width * 4);
	}
	Close(file);
	return ok;
}

/* A planar BitMap and its colour table to an RGBA file. */
static BOOL save_planar(const char *name, struct BitMap *bm,
                        struct ColorRegister *colours, ULONG ncolours,
                        struct BitMapHeader *bmh)
{
	ULONG width = bmh->bmh_Width, height = bmh->bmh_Height, x, y;
	BPTR file;
	UBYTE *row;
	BOOL ok = TRUE;

	row = get_memory(width * 4);
	if (row == NULL)
		return FALSE;
	file = open_pam(name, width, height);
	if (file == 0) {
		FreeVec(row);
		return FALSE;
	}
	for (y = 0; y < height && ok; y++) {
		for (x = 0; x < width; x++) {
			ULONG entry = 0;
			int plane;

			for (plane = bm->Depth - 1; plane >= 0; plane--) {
				UBYTE *bits = (UBYTE *)bm->Planes[plane] + y * bm->BytesPerRow;

				entry = (entry << 1) | ((bits[x >> 3] >> (7 - (x & 7))) & 1);
			}
			if (entry >= ncolours)
				entry = 0;
			row[x * 4 + 0] = colours[entry].red;
			row[x * 4 + 1] = colours[entry].green;
			row[x * 4 + 2] = colours[entry].blue;
			row[x * 4 + 3] = (bmh->bmh_Masking == mskHasTransparentColor &&
			                  entry == bmh->bmh_Transparent) ? 0 : 0xff;
		}
		ok = Write(file, row, (LONG)(width * 4)) == (LONG)(width * 4);
	}
	Close(file);
	FreeVec(row);
	return ok;
}

static LONG probe(const char *name, const char *out)
{
	struct Library *picture;
	struct DataType *type = NULL;
	struct BitMapHeader *bmh = NULL;
	Object *o;
	ULONG alpha = 0, known, picture_version = 0, picture_revision = 0;
	LONG start, taken;
	LONG result = RETURN_OK;

	picture = OpenLibrary("datatypes/picture.datatype", 0);
	if (picture != NULL) {
		picture_version = picture->lib_Version;
		picture_revision = picture->lib_Revision;
		CloseLibrary(picture);
	}
	SAY("picture.datatype %ld.%ld\n", picture_version, picture_revision);

	start = ticks();
	o = NewDTObject((APTR)name,
	                DTA_SourceType, DTST_FILE,
	                DTA_GroupID, GID_PICTURE,
	                PDTA_Remap, FALSE,
	                PDTA_DestMode, picture_version >= 43 ? PMODE_V43 : PMODE_V42,
	                TAG_DONE);
	taken = ticks() - start;
	if (o == NULL) {
		SAY("%s: no picture, error %ld\n", (ULONG)name, IoErr());
		return RETURN_ERROR;
	}

	GetDTAttrs(o, DTA_DataType, (ULONG)&type, PDTA_BitMapHeader, (ULONG)&bmh,
	           TAG_DONE);
	if (type != NULL && type->dtn_Header != NULL)
		SAY("datatype: %s (base %s)\n", (ULONG)type->dtn_Header->dth_Name,
		       (ULONG)type->dtn_Header->dth_BaseName);
	SAY("opened in %ld.%02ld s\n", taken / 50, (taken % 50) * 2);
	if (bmh == NULL) {
		SAY("no BitMapHeader\n");
		DisposeDTObject(o);
		return RETURN_ERROR;
	}
	SAY("size %ld x %ld, depth %ld, masking %ld, transparent %ld\n",
	       (ULONG)bmh->bmh_Width, (ULONG)bmh->bmh_Height, (ULONG)bmh->bmh_Depth,
	       (ULONG)bmh->bmh_Masking, (ULONG)bmh->bmh_Transparent);
	known = GetDTAttrs(o, PDTA_AlphaChannel, (ULONG)&alpha, TAG_DONE);
	if (known)
		SAY("PDTA_AlphaChannel: %s\n", (ULONG)(alpha ? "yes" : "no"));
	else
		SAY("PDTA_AlphaChannel: not known to this picture.datatype\n");

	if (out != NULL && picture_version >= 43) {
		ULONG width = bmh->bmh_Width, height = bmh->bmh_Height;
		UBYTE *argb = get_memory(width * height * 4);

		if (argb == NULL) {
			SAY("no memory for the pixels\n");
			result = RETURN_ERROR;
		} else {
			struct pdtBlitPixelArray read;
			ULONG got;

			read.MethodID = PDTM_READPIXELARRAY;
			read.pbpa_PixelData = argb;
			read.pbpa_PixelFormat = PBPAFMT_ARGB;
			read.pbpa_PixelArrayMod = width * 4;
			read.pbpa_Left = 0;
			read.pbpa_Top = 0;
			read.pbpa_Width = width;
			read.pbpa_Height = height;
			got = do_method(o, (Msg)&read);
			SAY("PDTM_READPIXELARRAY returned %ld\n", got);
			if (save_argb(out, argb, width, height,
			              bmh->bmh_Masking == mskHasAlpha || alpha))
				SAY("wrote %s\n", (ULONG)out);
			else {
				SAY("cannot write %s\n", (ULONG)out);
				result = RETURN_ERROR;
			}
			FreeVec(argb);
		}
	} else if (out != NULL) {
		struct BitMap *bm = NULL;
		struct ColorRegister *colours = NULL;
		ULONG ncolours = 0;
		struct gpLayout layout;

		/*
		 * An old picture.datatype gives its BitMap out only once the
		 * picture has been laid out. Nothing is remapped (PDTA_Remap is
		 * off), so no screen is needed for it.
		 */
		layout.MethodID = DTM_PROCLAYOUT;
		layout.gpl_GInfo = NULL;
		layout.gpl_Initial = 1;
		if (!DoDTMethodA(o, NULL, NULL, (Msg)&layout))
			SAY("DTM_PROCLAYOUT failed, error %ld\n", IoErr());
		GetDTAttrs(o, PDTA_BitMap, (ULONG)&bm, PDTA_ColorRegisters,
		           (ULONG)&colours, PDTA_NumColors, (ULONG)&ncolours, TAG_DONE);
		SAY("planar: %ld colours, BitMap %lx, colour table %lx\n", ncolours,
		    (ULONG)bm, (ULONG)colours);
		if (bm != NULL)
			SAY("BitMap: %ld bytes per row, %ld rows, depth %ld\n",
			    (ULONG)bm->BytesPerRow, (ULONG)bm->Rows, (ULONG)bm->Depth);
		if (bm == NULL || colours == NULL) {
			SAY("no BitMap or no colours\n");
			result = RETURN_ERROR;
		} else if (save_planar(out, bm, colours, ncolours, bmh)) {
			SAY("wrote %s\n", (ULONG)out);
		} else {
			SAY("cannot write %s\n", (ULONG)out);
			result = RETURN_ERROR;
		}
	}

	DisposeDTObject(o);
	return result;
}

int main(int argc, char **argv)
{
	LONG result;

	if (argc < 2 || argc > 3) {
		SAY("usage: dtprobe <picture> [<out.pam>]\n");
		return RETURN_FAIL;
	}
	ProbeDataTypesBase = OpenLibrary("datatypes.library", 39);
	if (ProbeDataTypesBase == NULL) {
		SAY("no datatypes.library\n");
		return RETURN_FAIL;
	}
#if defined(__amigaos4__)
	IDataTypes = (struct DataTypesIFace *)GetInterface(ProbeDataTypesBase, "main", 1, NULL);
	ProbeIntuitionBase = OpenLibrary("intuition.library", 50);
	if (ProbeIntuitionBase != NULL)
		IIntuition = (struct IntuitionIFace *)GetInterface(ProbeIntuitionBase, "main", 1, NULL);
	if (IDataTypes == NULL || IIntuition == NULL) {
		SAY("no datatypes or intuition interface\n");
		return RETURN_FAIL;
	}
#endif
	result = probe(argv[1], argc == 3 ? argv[2] : NULL);
#if defined(__amigaos4__)
	DropInterface((struct Interface *)IIntuition);
	CloseLibrary(ProbeIntuitionBase);
	DropInterface((struct Interface *)IDataTypes);
#endif
	CloseLibrary(ProbeDataTypesBase);
	return (int)result;
}
