/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Stands in for libwebp's src/utils/random_utils.c, which is not built.
 *
 * That file seeds the random numbers behind the decoder's optional
 * dithering, and it does so in floating point. A 68020 without an FPU can
 * only do that through a math library, and a datatype has none. The
 * dithering is off unless a caller asks for it in the decoder options, and
 * this datatype never does, so VP8InitRandom() is linked but not reached.
 * Should it be reached all the same, it leaves the generator silent: the
 * table is all zero, and VP8RandomBits2() then returns the middle value,
 * which is no dithering at all.
 */
#include <string.h>

#include "src/utils/random_utils.h"

void VP8InitRandom(VP8Random* const rg, float dithering)
{
	(void)dithering;
	memset(rg, 0, sizeof(*rg));
	rg->index2 = 31;
}
