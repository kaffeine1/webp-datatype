/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MorphOS: the library around the class.
 *
 * A native PowerPC library. Its functions are called through the 68k
 * library offsets, so they take no C arguments and read the 68k registers
 * of the caller; the class dispatcher is reached through a gate for the
 * same reason. The class is made when the library is first opened and
 * freed when it is last closed.
 */
#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/libraries.h>
#include <exec/resident.h>
#include <exec/semaphores.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <intuition/classes.h>
#include <emul/emulinterface.h>
#include <emul/emulregs.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include "../wdt_class.h"
#include "../wdt_version.h"

#define WDT_STACK 131072UL

/* The mark of a native MorphOS binary. It must stay in the symbol table. */
const ULONG __abox__ __attribute__((used)) = 1;

struct WdtBase {
	struct Library lib;
	UWORD pad;
	BPTR segments;
	struct SignalSemaphore lock;
	Class *class;
};

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;
struct Library *UtilityBase;
struct Library *wdt_picture_base;

static const char lib_name[] = WDT_NAME;
static const char lib_id[] = WDT_VSTRING "\r\n";
const char wdt_version_tag[] __attribute__((used)) = WDT_VERSTAG;

/* --- what the class asks of the lane -------------------------------------- */

ULONG wdt_super(Class *cl, Object *o, Msg msg)
{
	return CallHookPkt(&cl->cl_Super->cl_Dispatcher, o, msg);
}

LONG wdt_with_stack(LONG (*fn)(void *), void *arg)
{
	struct StackSwapStruct stack;
	struct PPCStackSwapArgs args;
	UBYTE *memory;
	LONG result;

	memory = AllocVec(WDT_STACK, MEMF_ANY);
	if (memory == NULL)
		return -1;
	stack.stk_Lower = memory;
	stack.stk_Upper = (ULONG)(memory + WDT_STACK);
	stack.stk_Pointer = (APTR)stack.stk_Upper;
	args.Args[0] = (ULONG)arg;
	result = (LONG)NewPPCStackSwap(&stack, (APTR)fn, &args);
	FreeVec(memory);
	return result;
}

static ULONG dispatch(void)
{
	return wdt_dispatch((Class *)REG_A0, (Object *)REG_A2, (Msg)REG_A1);
}

static const struct EmulLibEntry dispatch_gate = {
	TRAP_LIB, 0, (void (*)(void))dispatch
};

/* --- the class comes and goes with the first and the last opener ---------- */

static void close_all(struct WdtBase *base)
{
	if (base->class != NULL) {
		RemoveClass(base->class);
		if (FreeClass(base->class))
			base->class = NULL;
		else
			AddClass(base->class);  /* someone still has an object */
	}
	if (base->class != NULL)
		return;
	if (wdt_picture_base != NULL)
		CloseLibrary(wdt_picture_base);
	if (UtilityBase != NULL)
		CloseLibrary(UtilityBase);
	if (IntuitionBase != NULL)
		CloseLibrary((struct Library *)IntuitionBase);
	if (GfxBase != NULL)
		CloseLibrary((struct Library *)GfxBase);
	if (DOSBase != NULL)
		CloseLibrary((struct Library *)DOSBase);
	wdt_picture_base = NULL;
	UtilityBase = NULL;
	IntuitionBase = NULL;
	GfxBase = NULL;
	DOSBase = NULL;
}

static BOOL open_all(struct WdtBase *base)
{
	if (base->class != NULL)
		return TRUE;

	DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 39);
	GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39);
	IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
	UtilityBase = OpenLibrary("utility.library", 39);
	wdt_picture_base = OpenLibrary("datatypes/picture.datatype", 43);
	if (DOSBase != NULL && GfxBase != NULL && IntuitionBase != NULL &&
	    UtilityBase != NULL && wdt_picture_base != NULL) {
		Class *cl = MakeClass((STRPTR)lib_name, (STRPTR)PICTUREDTCLASS, NULL, 0, 0);

		if (cl != NULL) {
			cl->cl_Dispatcher.h_Entry = (HOOKFUNC)&dispatch_gate;
			cl->cl_UserData = (ULONG)base;
			AddClass(cl);
			base->class = cl;
			return TRUE;
		}
	}
	close_all(base);
	return FALSE;
}

/* --- the library ----------------------------------------------------------- */

static struct Library *lib_init(struct WdtBase *base, BPTR segments,
                                struct ExecBase *exec)
{
	SysBase = exec;
	base->lib.lib_Node.ln_Type = NT_LIBRARY;
	base->lib.lib_Node.ln_Name = (char *)lib_name;
	base->lib.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
	base->lib.lib_Version = WDT_VERSION;
	base->lib.lib_Revision = WDT_REVISION;
	base->lib.lib_IdString = (APTR)lib_id;
	base->segments = segments;
	base->class = NULL;
	InitSemaphore(&base->lock);
	return &base->lib;
}

static BPTR expunge(struct WdtBase *base)
{
	BPTR segments;

	if (base->lib.lib_OpenCnt != 0 || base->class != NULL) {
		base->lib.lib_Flags |= LIBF_DELEXP;
		return 0;
	}
	Remove(&base->lib.lib_Node);
	segments = base->segments;
	FreeMem((UBYTE *)base - base->lib.lib_NegSize,
	        base->lib.lib_NegSize + base->lib.lib_PosSize);
	return segments;
}

static struct Library *lib_open(void)
{
	struct WdtBase *base = (struct WdtBase *)REG_A6;
	BOOL ready;

	base->lib.lib_OpenCnt++;        /* keeps an expunge away while we wait */
	base->lib.lib_Flags &= ~LIBF_DELEXP;

	ObtainSemaphore(&base->lock);
	ready = open_all(base);
	ReleaseSemaphore(&base->lock);

	if (!ready) {
		base->lib.lib_OpenCnt--;
		return NULL;
	}
	return &base->lib;
}

static BPTR lib_close(void)
{
	struct WdtBase *base = (struct WdtBase *)REG_A6;

	ObtainSemaphore(&base->lock);
	if (base->lib.lib_OpenCnt > 0)
		base->lib.lib_OpenCnt--;
	if (base->lib.lib_OpenCnt == 0)
		close_all(base);
	ReleaseSemaphore(&base->lock);

	if (base->lib.lib_OpenCnt == 0 && (base->lib.lib_Flags & LIBF_DELEXP))
		return expunge(base);
	return 0;
}

static BPTR lib_expunge(void)
{
	return expunge((struct WdtBase *)REG_A6);
}

static ULONG lib_reserved(void)
{
	return 0;
}

static Class *lib_obtain_engine(void)
{
	struct WdtBase *base = (struct WdtBase *)REG_A6;

	return base->class;
}

static const APTR lib_functions[] = {
	(APTR)FUNCARRAY_32BIT_NATIVE,
	(APTR)lib_open,
	(APTR)lib_close,
	(APTR)lib_expunge,
	(APTR)lib_reserved,
	(APTR)lib_obtain_engine,
	(APTR)-1
};

static const struct {
	ULONG size;
	const APTR *functions;
	APTR data;
	APTR init;
} lib_auto_init = {
	sizeof(struct WdtBase),
	lib_functions,
	NULL,
	(APTR)lib_init
};

extern const struct Resident wdt_resident;
const struct Resident wdt_resident __attribute__((used)) = {
	RTC_MATCHWORD,
	(struct Resident *)&wdt_resident,
	(APTR)(&wdt_resident + 1),
	RTF_PPC | RTF_EXTENDED | RTF_AUTOINIT,
	WDT_VERSION,
	NT_LIBRARY,
	0,
	(char *)lib_name,
	(char *)lib_id,
	(APTR)&lib_auto_init,
	WDT_REVISION,
	NULL
};
