/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef WDT_VERSION_H
#define WDT_VERSION_H

/* The Makefile reads these three lines for the descriptor's version too. */
#define WDT_VERSION 45
#define WDT_REVISION 2
#define WDT_DATE "14.11.2026"

#define WDT_STR2(x) #x
#define WDT_STR(x) WDT_STR2(x)

#define WDT_NAME "webp.datatype"
#define WDT_VSTRING \
	WDT_NAME " " WDT_STR(WDT_VERSION) "." WDT_STR(WDT_REVISION) " (" WDT_DATE ")"
/*
 * The author's name after the date is what the Install script looks for,
 * to tell this datatype from another one with the same file name.
 */
#define WDT_AUTHOR "Michele Dipace"
#define WDT_VERSTAG "$VER: " WDT_VSTRING " " WDT_AUTHOR

#endif
