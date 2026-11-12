/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MorphOS: the first code of the library. This file is linked first.
 *
 * A library is not a program: started as one, it says no and ends.
 */
#include <exec/types.h>

LONG wdt_not_a_program(void);

LONG wdt_not_a_program(void)
{
	return -1;
}
