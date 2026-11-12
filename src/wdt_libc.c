/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The handful of C library functions the decoder calls.
 *
 * A datatype is a shared library: it has no startup code, so no C runtime
 * is set up for it, and a C library linked in would find its memory pools
 * and its bases missing. These few functions, built on exec.library alone,
 * are the whole of it, the same on AmigaOS 3, AmigaOS 4 and MorphOS.
 *
 * This file is compiled with -fno-builtin and without loop pattern
 * replacement: the compiler must not turn the loop inside memcpy() into a
 * call to memcpy().
 */
#if defined(__amigaos4__) && !defined(__USE_INLINE__)
#define __USE_INLINE__
#endif

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>

#include <stddef.h>

/* Unaligned 32 bit access is fine on a 68020 and on PowerPC. */
typedef unsigned long word_t;

void *malloc(size_t size);
void *calloc(size_t count, size_t size);
void free(void *block);
void *memcpy(void *to, const void *from, size_t n);
void *memmove(void *to, const void *from, size_t n);
void *memset(void *to, int value, size_t n);
int memcmp(const void *a, const void *b, size_t n);
int abs(int value);
void qsort(void *base, size_t count, size_t size,
           int (*compare)(const void *, const void *));

void *malloc(size_t size)
{
	if (size == 0)
		size = 1;
	return AllocVec(size, MEMF_ANY);
}

void *calloc(size_t count, size_t size)
{
	size_t total = count * size;

	if (size != 0 && total / size != count)
		return NULL;
	if (total == 0)
		total = 1;
	return AllocVec(total, MEMF_ANY | MEMF_CLEAR);
}

void free(void *block)
{
	if (block != NULL)
		FreeVec(block);
}

void *memcpy(void *to, const void *from, size_t n)
{
	unsigned char *t = to;
	const unsigned char *f = from;

	while (n >= sizeof(word_t)) {
		*(word_t *)t = *(const word_t *)f;
		t += sizeof(word_t);
		f += sizeof(word_t);
		n -= sizeof(word_t);
	}
	while (n-- > 0)
		*t++ = *f++;
	return to;
}

void *memmove(void *to, const void *from, size_t n)
{
	unsigned char *t = to;
	const unsigned char *f = from;

	if (t <= f || t >= f + n)
		return memcpy(to, from, n);
	t += n;
	f += n;
	while (n-- > 0)
		*--t = *--f;
	return to;
}

void *memset(void *to, int value, size_t n)
{
	unsigned char *t = to;
	word_t fill = (unsigned char)value;

	fill |= fill << 8;
	fill |= fill << 16;
	while (n >= sizeof(word_t)) {
		*(word_t *)t = fill;
		t += sizeof(word_t);
		n -= sizeof(word_t);
	}
	while (n-- > 0)
		*t++ = (unsigned char)value;
	return to;
}

int memcmp(const void *a, const void *b, size_t n)
{
	const unsigned char *x = a, *y = b;

	for (; n > 0; n--, x++, y++)
		if (*x != *y)
			return *x < *y ? -1 : 1;
	return 0;
}

int abs(int value)
{
	return value < 0 ? -value : value;
}

#if defined(__mc68000__)
/*
 * The string.h of libnix, whose headers the 68k build uses, makes
 * memmove() an inline call to this function of its library.
 */
void __bcopz(const void *from, void *to, size_t n);

void __bcopz(const void *from, void *to, size_t n)
{
	memmove(to, from, n);
}
#endif

/*
 * Only the part of libwebp that prepares palettes for the encoder sorts,
 * and the decoder never gets there; the linker wants the name all the
 * same. A plain insertion sort by swapping bytes.
 */
void qsort(void *base, size_t count, size_t size,
           int (*compare)(const void *, const void *))
{
	unsigned char *first = base;
	size_t i, j, k;

	for (i = 1; i < count; i++) {
		for (j = i; j > 0; j--) {
			unsigned char *here = first + j * size;
			unsigned char *before = here - size;

			if (compare(before, here) <= 0)
				break;
			for (k = 0; k < size; k++) {
				unsigned char c = here[k];

				here[k] = before[k];
				before[k] = c;
			}
		}
	}
}
