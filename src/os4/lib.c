/*
 * Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * AmigaOS 4.1: the library around the class.
 *
 * A native library with two interfaces: "__library", which exec uses to
 * open, close and expunge it, and "main", the DTClass interface, whose one
 * function of its own hands out the class. The class lives as long as the
 * library is in memory.
 */
#define __USE_INLINE__

#include <exec/types.h>
#include <exec/exec.h>
#include <exec/exectags.h>
#include <exec/interfaces.h>
#include <exec/libraries.h>
#include <exec/resident.h>
#include <dos/dos.h>
#include <intuition/classes.h>
#include <interfaces/dtclass.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include "../wdt_class.h"
#include "../wdt_version.h"

#define WDT_STACK 131072UL

/* A library is not a program: started as one, it says no and ends. */
int32 _start(void);
int32 _start(void)
{
	return RETURN_FAIL;
}

struct WdtBase {
	struct Library lib;
	BPTR segments;
	Class *class;
};

struct ExecIFace *IExec;
struct Library *DOSBase;
struct DOSIFace *IDOS;
struct Library *GfxBase;
struct GraphicsIFace *IGraphics;
struct Library *IntuitionBase;
struct IntuitionIFace *IIntuition;
struct Library *UtilityBase;
struct UtilityIFace *IUtility;
struct Library *wdt_picture_base;

static const char lib_name[] = WDT_NAME;
static const char lib_id[] = WDT_VSTRING "\r\n";
const char wdt_version_tag[] __attribute__((used)) = WDT_VERSTAG;

/* --- what the class asks of the lane -------------------------------------- */

ULONG wdt_super(Class *cl, Object *o, Msg msg)
{
	return IDoSuperMethodA(cl, o, msg);
}

LONG wdt_with_stack(LONG (*fn)(void *), void *arg)
{
	return NewStackRunTags((APTR)fn,
	                       NSR_StackSize, WDT_STACK,
	                       NSR_MinStackSize, WDT_STACK,
	                       NSR_Arg1, (uint32)arg,
	                       TAG_DONE);
}

static uint32 dispatch(Class *cl, Object *o, Msg msg)
{
	return wdt_dispatch(cl, o, msg);
}

/* --- libraries and the class ----------------------------------------------- */

static struct Interface *open_interface(const char *name, uint32 version,
                                        struct Library **base)
{
	struct Interface *interface = NULL;

	*base = OpenLibrary(name, version);
	if (*base != NULL) {
		interface = GetInterface(*base, "main", 1, NULL);
		if (interface == NULL) {
			CloseLibrary(*base);
			*base = NULL;
		}
	}
	return interface;
}

static void close_interface(struct Interface **interface, struct Library **base)
{
	if (*interface != NULL)
		DropInterface(*interface);
	if (*base != NULL)
		CloseLibrary(*base);
	*interface = NULL;
	*base = NULL;
}

static void close_all(struct WdtBase *base)
{
	if (base->class != NULL) {
		RemoveClass(base->class);
		FreeClass(base->class);
		base->class = NULL;
	}
	if (wdt_picture_base != NULL)
		CloseLibrary(wdt_picture_base);
	wdt_picture_base = NULL;
	close_interface((struct Interface **)&IUtility, &UtilityBase);
	close_interface((struct Interface **)&IIntuition, &IntuitionBase);
	close_interface((struct Interface **)&IGraphics, &GfxBase);
	close_interface((struct Interface **)&IDOS, &DOSBase);
}

static BOOL open_all(struct WdtBase *base)
{
	IDOS = (struct DOSIFace *)open_interface("dos.library", 52, &DOSBase);
	IGraphics = (struct GraphicsIFace *)open_interface("graphics.library", 52, &GfxBase);
	IIntuition = (struct IntuitionIFace *)open_interface("intuition.library", 52, &IntuitionBase);
	IUtility = (struct UtilityIFace *)open_interface("utility.library", 52, &UtilityBase);
	wdt_picture_base = OpenLibrary("datatypes/picture.datatype", 52);
	if (IDOS != NULL && IGraphics != NULL && IIntuition != NULL &&
	    IUtility != NULL && wdt_picture_base != NULL) {
		Class *cl = MakeClass(lib_name, PICTUREDTCLASS, NULL, 0, 0);

		if (cl != NULL) {
			cl->cl_Dispatcher.h_Entry = (HOOKFUNC)dispatch;
			cl->cl_UserData = (uint32)base;
			AddClass(cl);
			base->class = cl;
			return TRUE;
		}
	}
	close_all(base);
	return FALSE;
}

/* --- the "__library" interface --------------------------------------------- */

static struct Library *lib_init(struct Library *library, BPTR segments,
                                struct Interface *exec)
{
	struct WdtBase *base = (struct WdtBase *)library;

	IExec = (struct ExecIFace *)exec;
	library->lib_Node.ln_Type = NT_LIBRARY;
	library->lib_Node.ln_Pri = 0;
	library->lib_Node.ln_Name = (STRPTR)lib_name;
	library->lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
	library->lib_Version = WDT_VERSION;
	library->lib_Revision = WDT_REVISION;
	library->lib_IdString = (STRPTR)lib_id;
	base->segments = segments;
	base->class = NULL;

	if (!open_all(base))
		return NULL;
	return library;
}

static uint32 manager_obtain(struct LibraryManagerInterface *self)
{
	return ++self->Data.RefCount;
}

static uint32 manager_release(struct LibraryManagerInterface *self)
{
	return --self->Data.RefCount;
}

static struct Library *manager_open(struct LibraryManagerInterface *self,
                                    uint32 version)
{
	struct Library *library = self->Data.LibBase;

	(void)version;
	library->lib_OpenCnt++;
	library->lib_Flags &= ~LIBF_DELEXP;
	return library;
}

static BPTR manager_expunge(struct LibraryManagerInterface *self)
{
	struct WdtBase *base = (struct WdtBase *)self->Data.LibBase;
	BPTR segments;

	if (base->lib.lib_OpenCnt != 0) {
		base->lib.lib_Flags |= LIBF_DELEXP;
		return 0;
	}
	segments = base->segments;
	close_all(base);
	Remove(&base->lib.lib_Node);
	DeleteLibrary(&base->lib);
	return segments;
}

static BPTR manager_close(struct LibraryManagerInterface *self)
{
	struct Library *library = self->Data.LibBase;

	if (library->lib_OpenCnt > 0)
		library->lib_OpenCnt--;
	if (library->lib_OpenCnt == 0 && (library->lib_Flags & LIBF_DELEXP))
		return manager_expunge(self);
	return 0;
}

static const APTR manager_vectors[] = {
	(APTR)manager_obtain,
	(APTR)manager_release,
	NULL,
	NULL,
	(APTR)manager_open,
	(APTR)manager_close,
	(APTR)manager_expunge,
	NULL,
	(APTR)-1
};

static const struct TagItem manager_tags[] = {
	{ MIT_Name, (uint32)"__library" },
	{ MIT_VectorTable, (uint32)manager_vectors },
	{ MIT_Version, 1 },
	{ TAG_DONE, 0 }
};

/* --- the "main" interface: DTClass ------------------------------------------ */

static uint32 main_obtain(struct DTClassIFace *self)
{
	return ++self->Data.RefCount;
}

static uint32 main_release(struct DTClassIFace *self)
{
	return --self->Data.RefCount;
}

static Class *main_obtain_engine(struct DTClassIFace *self)
{
	struct WdtBase *base = (struct WdtBase *)self->Data.LibBase;

	return base->class;
}

static const APTR main_vectors[] = {
	(APTR)main_obtain,
	(APTR)main_release,
	NULL,
	NULL,
	(APTR)main_obtain_engine,
	(APTR)-1
};

static const struct TagItem main_tags[] = {
	{ MIT_Name, (uint32)"main" },
	{ MIT_VectorTable, (uint32)main_vectors },
	{ MIT_Version, 1 },
	{ TAG_DONE, 0 }
};

/* --- the resident tag ------------------------------------------------------- */

static const uint32 lib_interfaces[] = {
	(uint32)manager_tags,
	(uint32)main_tags,
	0
};

static const struct TagItem lib_create_tags[] = {
	{ CLT_DataSize, sizeof(struct WdtBase) },
	{ CLT_InitFunc, (uint32)lib_init },
	{ CLT_Interfaces, (uint32)lib_interfaces },
	{ TAG_DONE, 0 }
};

extern const struct Resident wdt_resident;
const struct Resident wdt_resident __attribute__((used)) = {
	RTC_MATCHWORD,
	(struct Resident *)&wdt_resident,
	(APTR)(&wdt_resident + 1),
	RTF_NATIVE | RTF_AUTOINIT,
	WDT_VERSION,
	NT_LIBRARY,
	0,
	(STRPTR)lib_name,
	(STRPTR)lib_id,
	(APTR)lib_create_tags
};
