/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The class, as every lane sees it. wdt_class.c holds what the lanes
 * share; each lane's lib.c is the library around it and supplies the few
 * things that differ.
 */
#ifndef WDT_CLASS_H
#define WDT_CLASS_H

#if defined(__amigaos4__) && !defined(__USE_INLINE__)
#define __USE_INLINE__
#endif

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <utility/tagitem.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>

/* The colour put behind a translucent picture where alpha cannot be kept. */
#ifndef WDT_BACKGROUND
#define WDT_BACKGROUND 0xffffffUL
#endif

/* A file larger than this is refused before any memory is asked for. */
#ifndef WDT_MAX_FILE
#define WDT_MAX_FILE (64UL * 1024UL * 1024UL)
#endif

/* The dispatcher of the class, called by each lane's entry point. */
ULONG wdt_dispatch(Class *cl, Object *o, Msg msg);

/* --- supplied by the lane ------------------------------------------------ */

/* The opened picture.datatype: its version says what it can take. */
extern struct Library *wdt_picture_base;

/* Passes a method on to picture.datatype. */
ULONG wdt_super(Class *cl, Object *o, Msg msg);

/*
 * Runs fn(arg) with enough stack for the decoder. A datatype runs on the
 * stack of whoever opens the picture, and on AmigaOS 3 that can be 4 KB.
 * Returns what fn returns, or -1 when the stack cannot be had.
 */
LONG wdt_with_stack(LONG (*fn)(void *), void *arg);

#endif
