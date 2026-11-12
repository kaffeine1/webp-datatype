| Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
| SPDX-License-Identifier: GPL-3.0-or-later
|
| AmigaOS 3: the first bytes of the library, and the stack switch.
| This file is linked first.

	.text

| A library is not a program: started as one, it says no and ends.
	moveq	#-1,d0
	rts

| LONG wdt_stackcall(struct StackSwapStruct *stack, LONG (*fn)(void *), void *arg)
|
| Calls fn(arg) on the stack described by *stack and comes back on the old
| one. Everything needed across the two StackSwap() calls is kept in
| registers that a called function must preserve, never on a stack.

	.globl	_wdt_stackcall
	.align	2
_wdt_stackcall:
	movem.l	d2/a2-a4/a6,-(sp)
	move.l	24(sp),a3		| stack
	move.l	28(sp),a4		| fn
	move.l	32(sp),a2		| arg
	move.l	4.w,a6			| SysBase
	move.l	a3,a0
	jsr	-732(a6)		| StackSwap()
	move.l	a2,-(sp)
	jsr	(a4)
	addq.l	#4,sp
	move.l	d0,d2
	move.l	a3,a0
	jsr	-732(a6)		| StackSwap()
	move.l	d2,d0
	movem.l	(sp)+,d2/a2-a4/a6
	rts
