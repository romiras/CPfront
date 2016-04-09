/*  Ofront 1.2 -xtspkae */
#include "SYSTEM.h"
#include "Console.h"

typedef
	struct Modules_CmdDesc *Modules_Cmd;

typedef
	void (*Modules_Command)(void);

typedef
	struct Modules_CmdDesc {
		Modules_Cmd next;
		CHAR name[24];
		Modules_Command cmd;
	} Modules_CmdDesc;

typedef
	struct Modules_ModuleDesc *Modules_Module;

typedef
	CHAR Modules_ModuleName[20];

typedef
	struct Modules_ModuleDesc {
		Modules_Module next;
		Modules_ModuleName name;
		LONGINT refcnt;
		Modules_Cmd cmds;
		LONGINT types;
		void (*enumPtrs)(void(*)(LONGINT));
		LONGINT reserved1, reserved2;
	} Modules_ModuleDesc;


export INTEGER Modules_res;
export CHAR Modules_resMsg[256];
export Modules_ModuleName Modules_imported, Modules_importing;

export LONGINT *Modules_ModuleDesc__typ;
export LONGINT *Modules_CmdDesc__typ;

static void Modules_Append (CHAR *a, LONGINT a__len, CHAR *b, LONGINT b__len);
export void Modules_Free (CHAR *name, LONGINT name__len, BOOLEAN all);
export Modules_Command Modules_ThisCommand (Modules_Module mod, CHAR *name, LONGINT name__len);
export Modules_Module Modules_ThisMod (CHAR *name, LONGINT name__len);

#define Modules_modules()	(Modules_Module)SYSTEM_modules
#define Modules_setmodules(m)	SYSTEM_modules = m

/*============================================================================*/

static void Modules_Append (CHAR *a, LONGINT a__len, CHAR *b, LONGINT b__len)
{
	INTEGER i, j;
	__DUP(b, b__len, CHAR);
	i = 0;
	while (a[__X(i, a__len)] != 0x00) {
		i += 1;
	}
	j = 0;
	while (b[__X(j, b__len)] != 0x00) {
		a[__X(i, a__len)] = b[__X(j, b__len)];
		i += 1;
		j += 1;
	}
	a[__X(i, a__len)] = 0x00;
	__DEL(b);
}

Modules_Module Modules_ThisMod (CHAR *name, LONGINT name__len)
{
	Modules_Module m = NIL;
	CHAR bodyname[64];
	Modules_Command body;
	__DUP(name, name__len, CHAR);
	m = Modules_modules();
	while (m != NIL && __STRCMP(m->name, name) != 0) {
		m = m->next;
	}
	if (m != NIL) {
		Modules_res = 0;
		Modules_resMsg[0] = 0x00;
	} else {
		Modules_res = 1;
		__COPY(name, Modules_importing, 20);
		__MOVE(" module \"", Modules_resMsg, 10);
		Modules_Append((void*)Modules_resMsg, 256, name, name__len);
		Modules_Append((void*)Modules_resMsg, 256, (CHAR*)"\" not found", (LONGINT)12);
	}
	__DEL(name);
	return m;
}

/*----------------------------------------------------------------------------*/
Modules_Command Modules_ThisCommand (Modules_Module mod, CHAR *name, LONGINT name__len)
{
	Modules_Cmd c = NIL;
	__DUP(name, name__len, CHAR);
	c = mod->cmds;
	while (c != NIL && __STRCMP(c->name, name) != 0) {
		c = c->next;
	}
	if (c != NIL) {
		Modules_res = 0;
		Modules_resMsg[0] = 0x00;
		__DEL(name);
		return c->cmd;
	} else {
		Modules_res = 2;
		__MOVE(" command \"", Modules_resMsg, 11);
		__COPY(name, Modules_importing, 20);
		Modules_Append((void*)Modules_resMsg, 256, mod->name, 20);
		Modules_Append((void*)Modules_resMsg, 256, (CHAR*)".", (LONGINT)2);
		Modules_Append((void*)Modules_resMsg, 256, name, name__len);
		Modules_Append((void*)Modules_resMsg, 256, (CHAR*)"\" not found", (LONGINT)12);
		__DEL(name);
		return NIL;
	}
	__RETCHK;
}

/*----------------------------------------------------------------------------*/
void Modules_Free (CHAR *name, LONGINT name__len, BOOLEAN all)
{
	Modules_Module m = NIL, p = NIL;
	__DUP(name, name__len, CHAR);
	m = Modules_modules();
	if (all) {
		Modules_res = 1;
		__MOVE("unloading \"all\" not yet supported", Modules_resMsg, 34);
	} else {
		while (m != NIL && __STRCMP(m->name, name) != 0) {
			p = m;
			m = m->next;
		}
		if (m != NIL && m->refcnt == 0) {
			if (m == Modules_modules()) {
				Modules_setmodules(m->next);
			} else {
				p->next = m->next;
			}
			Modules_res = 0;
		} else {
			Modules_res = 1;
			if (m == NIL) {
				__MOVE("module not found", Modules_resMsg, 17);
			} else {
				__MOVE("clients of this module exist", Modules_resMsg, 29);
			}
		}
	}
	__DEL(name);
}

/*----------------------------------------------------------------------------*/
__TDESC(Modules_ModuleDesc__desc, 1, 2) = {__TDFLDS("ModuleDesc", 48), {0, 28, -12}};
__TDESC(Modules_CmdDesc__desc, 1, 1) = {__TDFLDS("CmdDesc", 32), {0, -8}};

export void *Modules__init(void)
{
	__DEFMOD;
	__IMPORT(Console__init);
	__REGMOD("Modules", 0);
	__INITYP(Modules_ModuleDesc, Modules_ModuleDesc, 0);
	__INITYP(Modules_CmdDesc, Modules_CmdDesc, 0);
/* BEGIN */
	__ENDMOD;
}
