MODULE OfrontCPC;
(*
	C source code generator
	based on OfrontOPC by J. Templ
	8.12.99 bh

	todo:
*)

	IMPORT OPG := OfrontCPG, OPM := DevCPM, OPT := DevCPT;

	CONST
		fullExportList = TRUE;
		longStringMacro = FALSE;
		openArrayFields = FALSE;
		dynamicLink  = TRUE;
		initOutPointer = TRUE;

	CONST
		(* structure forms *)
		Undef = 0; Byte = 1; Bool = 2; Char8 = 3; Int8 = 4; Int16 = 5; Int32 = 6;
		Real32 = 7; Real64 = 8; Set = 9; String8 = 10; NilTyp = 11; NoTyp = 12;
		Pointer = 13; ProcTyp = 14; Comp = 15;
		Char16 = 16; String16 = 17; Int64 = 18; Guid = 23;

		(* composite structure forms *)
		Basic = 1; Array = 2; DynArr = 3; Record = 4;

		(* object history *)
		removed = 4;

		(* object modes *)
		Var = 1; VarPar = 2; Con = 3; Fld = 4; Typ = 5; LProc = 6; XProc = 7;
		CProc = 9; Mod = 11; TProc = 13;

		(* symbol values and ops *)
		eql = 9; neq = 10; lss = 11; leq = 12; gtr = 13; geq = 14;

		(* nodes classes *)
		Ninittd = 14;

		(* module visibility of objects *)
		internal = 0; external = 1; externalR = 2; inPar = 3; outPar = 4;

		(* attribute flags (attr.adr, struct.attribute, proc.conval.setval)*)
		newAttr = 16; absAttr = 17; limAttr = 18; empAttr = 19; extAttr = 20;

		(* sysflag *)
		untagged = 1; callback = 2; noAlign = 3; union = 7; interface = 10;

		UndefinedType = 0; (* named type not yet defined *)
		ProcessingType = 1; (* pointer type is being processed *)
		PredefinedType = 2; (* for all predefined types *)
		DefinedInHdr = 3+OPG.HeaderFile; (* named type has been defined in header file *)
		DefinedInBdy = 3+OPG.BodyFile; (* named type has been defined in body file *)

		MaxNameTab = 800000H;

		HeaderMsg = " Ofront 1.2 -";
		BasicIncludeFile = "SYSTEM";
		Static = "static ";
		Export = "export ";	(* particularily introduced for VC++ declspec() *)
		Extern = "import ";	(* particularily introduced for VC++ declspec() *)
		Struct = "struct ";
		LocalScope = "_s"; (* name of a local intermediate scope (variable name) *)
		GlobalScope = "_s"; (* pointer to current scope extension *)
		LinkName = "lnk"; (* pointer to previous scope field *)
		FlagExt = "__h";
		LenExt = "__len";
		DynTypExt = "__typ";
		TagExt = "__typ";

		OpenParen = "(";
		CloseParen = ")";
		OpenBrace = "{";
		CloseBrace = "}";
		OpenBracket = "[";
		CloseBracket = "]";
		Underscore = "_";
		Quotes = 22X;
		SingleQuote = 27X;
		Tab = 9X;
		Colon = ": ";
		Semicolon = ";";
		Comma = ", ";
		Becomes = " = ";
		Star = "*";
		Blank = " ";
		Dot = ".";

		DupFunc = "__DUP("; (* duplication of dynamic arrays *)
		DupArrFunc = "__DUPARR("; (* duplication of fixed size arrays *)
		DelFunc = "__DEL("; (* removal of dynamic arrays *)

		NilConst = "NIL";

		VoidType = "void";
		CaseStat = "case ";

	VAR
		indentLevel: SHORTINT;
		ptrinit, mainprog, ansi, windows: BOOLEAN;
		inxchk, ovflchk, ranchk, typchk: BOOLEAN;
		hashtab: ARRAY 105 OF BYTE;
		keytab: ARRAY 36, 9 OF SHORTCHAR;
		GlbPtrs: BOOLEAN;
		BodyNameExt: ARRAY 13 OF SHORTCHAR;
		options: SET;
		namex: INTEGER;
		nameList: OPT.Object;
		done1, done2, done3: OPT.String;

		arrayTypes: ARRAY 1024 OF OPT.Struct;	(* table of all unnamed heap allocated array types *)
		arrayIndex: INTEGER;


	PROCEDURE Init* (opt: SET);
		CONST chk = 0; achk = 1; hint = 29;
	BEGIN
		options := opt * {0..15};
		indentLevel := 0;
		inxchk := chk IN opt;
		ovflchk := achk IN opt;
		ranchk := achk IN opt;
		typchk := chk IN opt;
		ptrinit := chk IN opt;
		mainprog := OPG.mainprog IN OPG.opt;
		ansi := OPG.ansi IN OPG.opt;
		windows := FALSE;
		namex := 1;
		nameList := NIL;
		arrayIndex := 0
	END Init;

	PROCEDURE Indent* (count: SHORTINT);
	BEGIN INC(indentLevel, count)
	END Indent;

	PROCEDURE BegStat*;
		VAR i: SHORTINT;
	BEGIN i := indentLevel;
		WHILE i > 0 DO OPG.Write(Tab); DEC (i) END
	END BegStat;

	PROCEDURE EndStat*;
	BEGIN OPG.Write(Semicolon); OPG.WriteLn
	END EndStat;

	PROCEDURE BegBlk*;
	BEGIN OPG.Write(OpenBrace); OPG.WriteLn; INC(indentLevel)
	END BegBlk;

	PROCEDURE EndBlk*;
	BEGIN DEC(indentLevel); BegStat; OPG.Write(CloseBrace); OPG.WriteLn
	END EndBlk;

	PROCEDURE EndBlk0*;
	BEGIN DEC(indentLevel); BegStat; OPG.Write(CloseBrace)
	END EndBlk0;

	PROCEDURE Str1(s: ARRAY OF SHORTCHAR; x: INTEGER);
		VAR ch: SHORTCHAR; i: SHORTINT;
	BEGIN ch := s[0]; i := 0;
		WHILE ch # 0X DO
			IF ch = "#" THEN OPG.WriteInt(x)
			ELSE OPG.Write(ch);
			END ;
			INC(i); ch := s[i]
		END
	END Str1;

	PROCEDURE Length(VAR s: ARRAY OF SHORTCHAR): SHORTINT;
		VAR i: SHORTINT;
	BEGIN i := 0;
		WHILE s[i] # 0X DO INC(i) END ;
		RETURN i
	END Length;

	PROCEDURE PerfectHash (VAR s: ARRAY OF SHORTCHAR): SHORTINT;
		VAR i, h: SHORTINT;
	BEGIN i := 0; h := 0;
		WHILE (s[i] # 0X) & (i < 5) DO h := SHORT(3*h + ORD(s[i])); INC(i) END;
		RETURN SHORT(h MOD 105)
	END PerfectHash;

	PROCEDURE Ident* (obj: OPT.Object);
		VAR mode, level, h: SHORTINT; mod: OPT.Object;
	BEGIN
		mode := obj^.mode; level := obj^.mnolev;
		IF (mode = Typ) & (obj.typ.form = Pointer) & (obj.typ.BaseTyp.strobj # NIL)
				& (obj.typ.BaseTyp.strobj.name^ # "") & (OPT.GlbMod[obj.typ.mno].library # NIL) THEN
			Ident(obj.typ.BaseTyp.strobj); OPG.Write("*")
		ELSIF (obj^.entry # NIL) & (level <= 0) THEN
			OPG.WriteOrigString(obj^.entry^);
			IF (OPG.currFile = OPG.HeaderFile) & (level < 0) THEN OPT.GlbMod[-level].vis := 1 END
		ELSIF (mode IN {Var, Typ, LProc}) & (level > 0) OR (mode IN {Fld, VarPar}) THEN
			OPG.WriteString(obj^.name);
			h := PerfectHash(obj^.name);
			IF hashtab[h] >= 0 THEN
				IF keytab[hashtab[h]] = obj^.name^ THEN OPG.Write(Underscore) END
			END
		ELSE
			IF (mode # Typ) OR (obj^.linkadr # PredefinedType) THEN
				IF mode = TProc THEN
					Ident(obj^.link^.typ^.strobj); OPG.Write(Underscore)
				ELSIF level < 0 THEN (* use unaliased module name *)
					mod := OPT.GlbMod[-level];
					IF mod.library = NIL THEN
						OPG.WriteString(mod.name); OPG.Write(Underscore)
					END;
					IF OPG.currFile = OPG.HeaderFile THEN mod.vis := 1 (*include in header file*) END ;
				ELSE
					OPG.WriteString(OPG.modName); OPG.Write(Underscore)
				END
			ELSIF (obj = OPT.sysptrtyp^.strobj) THEN
				OPG.WriteString("SYSTEM_")
			END ;
			OPG.WriteString(obj^.name)
		END
	END Ident;

	PROCEDURE CompleteIdent*(obj: OPT.Object);
		VAR comp, level: SHORTINT;
	BEGIN
		(* obj^.mode IN {Var, VarPar} *)
		level := obj^.mnolev;
		IF obj^.adr = 1 THEN	(* WITH-variable *)
			IF obj^.typ^.comp = Record THEN Ident(obj); OPG.WriteString("__")
			ELSE (* cast with guard pointer type *)
				OPG.WriteString("(("); Ident(obj^.typ^.strobj);
				IF obj.mode = VarPar THEN OPG.Write("*") END;
				OPG.Write(")"); Ident(obj); OPG.Write(")")
			END
		ELSIF (level # OPG.level) & (level > 0) THEN (* intermediate var *)
			comp := obj^.typ^.comp;
			IF (comp = Array) OR (obj^.mode # VarPar) & (comp # DynArr) THEN OPG.WriteString("(*") END;
			OPG.WriteString(obj^.scope^.name); OPG.WriteString(GlobalScope);
			OPG.WriteString("->"); Ident(obj);
			IF (comp = Array) OR (obj^.mode # VarPar) & (comp # DynArr) THEN OPG.Write(")") END;
		ELSE
			Ident(obj)
		END
	END CompleteIdent;

	PROCEDURE Stars (typ: OPT.Struct; VAR openClause: BOOLEAN);
		VAR pointers: SHORTINT; t0: OPT.Struct;
	BEGIN
		openClause := FALSE;
		IF (typ^.comp # Record) & ((typ^.strobj = NIL) OR (typ^.strobj^.name^ = "")) THEN
			IF typ^.comp IN {Array, DynArr} THEN
				Stars (typ^.BaseTyp, openClause);
				openClause := (typ^.comp = Array)
			ELSIF typ^.form = ProcTyp THEN
				Stars(typ.BaseTyp, openClause);
				OPG.Write(OpenParen); OPG.Write(Star)
			ELSIF typ.form = Pointer THEN
				pointers := 0; t0 := typ;
				WHILE ((typ^.strobj = NIL) OR (typ.strobj.name^ = "")) & (typ^.form = Pointer) DO
					INC (pointers); typ := typ^.BaseTyp
				END ;
				IF typ^.comp # DynArr THEN ASSERT(t0 # typ); Stars (typ, openClause) END ;
				IF pointers > 0 THEN
					IF openClause THEN OPG.Write(OpenParen); openClause := FALSE END ;
					WHILE pointers > 0 DO OPG.Write(Star); DEC (pointers) END
				END
			END
		END
	END Stars;

	PROCEDURE ^AnsiParamList (obj: OPT.Object; showParamNames: BOOLEAN);
	PROCEDURE^ DeclareReturnType (retTyp: OPT.Struct);

	PROCEDURE DeclareObj(dcl: OPT.Object; vis: INTEGER);
		VAR
			typ: OPT.Struct;
			varPar, openClause: BOOLEAN; form, comp: SHORTINT;
	BEGIN
		typ := dcl^.typ;
		varPar := ((dcl^.mode = VarPar) & (typ^.comp # Array)) OR (typ^.comp = DynArr) OR (vis = 3);
		Stars(typ, openClause);
		IF varPar THEN
			IF openClause THEN OPG.Write(OpenParen) END ;
			OPG.Write(Star)
		END ;
		IF dcl.name^ # "" THEN
			Ident(dcl);
			IF vis = 4 THEN OPG.WriteString("__copy") END
		END ;
		IF varPar & openClause THEN OPG.Write(CloseParen) END ;
		openClause := FALSE;
		LOOP
			form := typ^.form;
			comp := typ^.comp;
			IF ((typ^.strobj # NIL) & (typ^.strobj^.name^ # "")) OR (form = NoTyp) OR (comp = Record) THEN EXIT
			ELSIF (form = Pointer) & (typ^.BaseTyp^.comp = Array) THEN
				IF (typ^.BaseTyp^.strobj = NIL) OR (typ^.BaseTyp^.strobj^.name^ = "") THEN OPG.Write(CloseParen) END;
				EXIT
			ELSIF (form = Pointer) & (typ^.BaseTyp^.comp # DynArr) THEN
				openClause := TRUE;
			ELSIF (form = ProcTyp) OR (comp IN {Array, DynArr}) THEN
				IF openClause THEN OPG.Write(CloseParen); openClause := FALSE END ;
				IF form = ProcTyp THEN
					IF ansi THEN OPG.Write(")"); AnsiParamList(typ^.link, FALSE)
					ELSE OPG.WriteString(")()")
					END ;
					DeclareReturnType(typ.BaseTyp);
					EXIT
				ELSIF comp = Array THEN
					OPG.Write(OpenBracket); OPG.WriteInt(typ^.n); OPG.Write(CloseBracket)
				END
			ELSE
				EXIT
			END ;
			typ := typ^.BaseTyp
		END
	END DeclareObj;

	PROCEDURE Andent*(typ: OPT.Struct);	(* ident of possibly anonymous record type *)
		VAR mod: OPT.Object;
	BEGIN
		IF (typ^.strobj = NIL) OR (typ^.align >= 10000H) THEN
			IF typ.mno = 0 THEN OPG.WriteString(OPG.modName)
			ELSE
				mod := OPT.GlbMod[typ.mno];
				OPG.WriteString(mod.name);
				IF OPG.currFile = OPG.HeaderFile THEN mod.vis := 1 (*include in header file*) END
			END;
			Str1("__#", typ^.align DIV 10000H)
		ELSE Ident(typ^.strobj)
		END
	END Andent;

	PROCEDURE Undefined(obj: OPT.Object): BOOLEAN;
	BEGIN
		(* imported anonymous types have obj^.name = ""; used e.g. for repeating inherited fields *)
		RETURN (obj^.mnolev >= 0) & (obj^.linkadr # 3+OPG.currFile ) & (obj^.linkadr # PredefinedType) OR (obj^.name^ = "")
	END Undefined;

	PROCEDURE^ FieldList (typ: OPT.Struct; last: BOOLEAN; VAR off, n, curAlign: INTEGER);
	PROCEDURE^ Universal (typ: OPT.Struct): BOOLEAN;
	PROCEDURE^ UniversalArrayName (typ: OPT.Struct);

	PROCEDURE DeclareBase(dcl: OPT.Object); (* declare the specifier of object dcl*)
		VAR typ, prev: OPT.Struct; obj: OPT.Object; nofdims: SHORTINT; off, n, dummy: INTEGER;
	BEGIN
		typ := dcl^.typ; prev := typ;
		WHILE ((typ^.strobj = NIL) OR (typ^.comp = DynArr) OR Undefined(typ^.strobj)) & (typ^.comp # Record) & (typ^.form # NoTyp)
				& ~((typ^.form = Pointer) & (typ^.BaseTyp^.comp IN {Array, DynArr}) & ~typ.untagged) DO
			prev := typ; typ := typ^.BaseTyp
		END ;
		obj := typ^.strobj;
		IF typ^.form = NoTyp THEN	(* proper procedure *)
			OPG.WriteString(VoidType)
		ELSIF (obj # NIL) & ~Undefined(obj) THEN	(* named type, already declared *)
			Ident(obj)
		ELSIF typ^.comp = Record THEN
			OPG.WriteString(Struct); Andent(typ);
			IF (prev.form # Pointer) & ((obj # NIL) OR (dcl.name^ = "")) THEN
				(* named record type not yet declared OR anonymous record with empty name *)
				IF (typ^.BaseTyp # NIL) & (typ^.BaseTyp^.strobj.vis # internal) THEN
					OPG.WriteString(" { /* "); Ident(typ^.BaseTyp^.strobj); OPG.WriteString(" */"); OPG.WriteLn; Indent(1)
				ELSE OPG.Write(Blank); BegBlk
				END ;
				FieldList(typ, TRUE, off, n, dummy);
				EndBlk0
			END
		ELSIF (typ^.form = Pointer) & (typ^.BaseTyp^.comp IN {Array, DynArr}) & ~typ.untagged THEN
			typ := typ^.BaseTyp;
			OPG.WriteString(Struct);
			IF Universal(typ) THEN
				UniversalArrayName(typ)
			ELSE
				Andent(typ)
			END
(*
			IF  (typ.comp = DynArr)
					& (typ.BaseTyp.form IN {Bool, Char8, Char16, Int8, Int16, Int32, Int64, Set, Real32, Real64}) THEN
				typ := typ.BaseTyp;
				IF typ.BaseTyp # OPT.undftyp THEN typ := typ.BaseTyp END;	(* basic type alias *)
				ASSERT(typ.strobj.name^ # "");
				OPG.WriteOrigString(typ.strobj.name^); OPG.WriteString("_ARRAY")
			ELSIF (typ.comp = DynArr)
					& (typ.BaseTyp.form = Pointer) & (typ.BaseTyp.BaseTyp.comp = DynArr) & ~typ.BaseTyp.untagged
					& (typ.BaseTyp.BaseTyp.BaseTyp.form IN {Bool, Char8, Char16, Int8, Int16, Int32, Int64, Set, Real32, Real64}) THEN
				typ := typ.BaseTyp.BaseTyp.BaseTyp;
				IF typ.BaseTyp # OPT.undftyp THEN typ := typ.BaseTyp END;	(* basic type alias *)
				ASSERT(typ.strobj.name^ # "");
				OPG.WriteOrigString(typ.strobj.name^); OPG.WriteString("_ARRAY_P_ARRAY")
			ELSE
				OPG.WriteString(Struct); BegBlk;
				IF typ.comp = Array THEN
					BegStat; OPG.WriteString("INTEGER gc[3]"); EndStat;
					BegStat; NEW(obj);	(* aux. object for easy declaration *)
					obj.typ := typ; obj.mode := Fld;
					obj.name := OPT.NewName("data");
				ELSE
					BegStat; Str1("INTEGER gc[3], len[#]", typ.n + 1); EndStat;
					WHILE typ^.comp = DynArr DO typ := typ^.BaseTyp END ;
					BegStat; NEW(obj); NEW(obj.typ);	(* aux. object for easy declaration *)
					obj.typ.form := Comp; obj.typ.comp := Array; obj.typ.n := 1; obj.typ.BaseTyp := typ; obj.mode := Fld;
					obj.name := OPT.NewName("data");
				END;
				obj.linkadr := UndefinedType; DeclareBase(obj); OPG.Write(Blank);  DeclareObj(obj, FALSE);
				EndStat; EndBlk0
			END
*)
		END
	END DeclareBase;

	PROCEDURE InitTProcs(typ, obj: OPT.Object);
	BEGIN
		IF obj # NIL THEN
			InitTProcs(typ, obj^.left);
			IF obj^.mode = TProc THEN
				BegStat;
				OPG.WriteString("__INITBP(");
				Ident(typ); OPG.WriteString(Comma); Ident(obj);
				Str1(", #)", obj^.num);
				EndStat
			END ;
			InitTProcs(typ, obj^.right)
		END
	END InitTProcs;

	PROCEDURE LenList(par: OPT.Object; ansiDefine, showParamName: BOOLEAN);
		VAR typ: OPT.Struct; dim: SHORTINT;
	BEGIN
		IF showParamName THEN Ident(par); OPG.WriteString(LenExt) END ;
		dim := 1; typ := par^.typ^.BaseTyp;
		WHILE (typ^.comp = DynArr) & ~typ.untagged DO
			IF ansiDefine THEN OPG.WriteString(", INTEGER ") ELSE OPG.WriteString(Comma) END ;
			IF showParamName THEN Ident(par); OPG.WriteString(LenExt); OPG.WriteInt(dim) END ;
			typ := typ^.BaseTyp; INC(dim)
		END
	END LenList;

	PROCEDURE DeclareParams(par: OPT.Object; macro: BOOLEAN);
	BEGIN
		OPG.Write(OpenParen);
		WHILE par # NIL DO
			IF macro THEN OPG.WriteString(par.name)
			ELSE
				IF (par^.mode = Var) & (par^.typ^.form = Real32) THEN OPG.Write("_") END ;
				Ident(par)
			END ;
			IF (par^.typ^.comp = DynArr) & ~par.typ.untagged THEN
				OPG.WriteString(Comma); LenList(par, FALSE, TRUE);
			ELSIF (par^.mode = VarPar) & (par^.typ^.comp = Record) & ~par.typ.untagged THEN
				OPG.WriteString(Comma); OPG.WriteString(par.name); OPG.WriteString(TagExt)
			END ;
			par := par^.link;
			IF par # NIL THEN OPG.WriteString(Comma) END
		END ;
		OPG.Write(CloseParen)
	END DeclareParams;

	PROCEDURE ^DefineType(str: OPT.Struct);
	PROCEDURE ^ProcHeader(proc: OPT.Object; define: BOOLEAN);

	PROCEDURE DefineTProcTypes(obj: OPT.Object);	(* define all types that are used in a TProc definition *)
		VAR par: OPT.Object;
	BEGIN
		IF obj^.typ # OPT.notyp THEN DefineType(obj^.typ) END ;
		IF ansi THEN par := obj^.link;
			WHILE par # NIL DO DefineType(par^.typ); par := par^.link END
		END
	END DefineTProcTypes;

	PROCEDURE DeclareTProcs(obj: OPT.Object; VAR empty: BOOLEAN);
	BEGIN
		IF obj # NIL THEN
			DeclareTProcs(obj^.left, empty);
			IF obj^.mode = TProc THEN
				IF obj^.typ # OPT.notyp THEN DefineType(obj^.typ) END ;
				IF OPG.currFile = OPG.HeaderFile THEN
					IF obj^.vis # internal THEN
						DefineTProcTypes(obj);
						OPG.WriteString(Extern); empty := FALSE;
						ProcHeader(obj, FALSE)
					END
				ELSE empty := FALSE;
					DefineTProcTypes(obj);
					IF obj^.vis = internal THEN OPG.WriteString(Static)
					ELSE OPG.WriteString(Export)
					END ;
					ProcHeader(obj, FALSE)
				END
			END ;
			DeclareTProcs(obj^.right, empty)
		END
	END DeclareTProcs;

	PROCEDURE BaseTProc*(obj: OPT.Object): OPT.Object;
		VAR typ, base: OPT.Struct; mno: INTEGER; bobj: OPT.Object;
	BEGIN typ := obj^.link^.typ;	(* receiver type *)
		IF typ^.form = Pointer THEN typ := typ^.BaseTyp END ;
		base := typ^.BaseTyp; mno := obj^.num;
		WHILE (base # NIL) & (mno < base^.n) DO typ := base; base := typ^.BaseTyp END ;
		OPT.FindField(obj^.name, typ, bobj);
		IF bobj.typ = obj.typ THEN obj := bobj END;	(* don't use base method if return type is changed *)
		RETURN obj
	END BaseTProc;

	PROCEDURE DefineTProcMacros(obj: OPT.Object; VAR empty: BOOLEAN);
		VAR oc: BOOLEAN;
	BEGIN
		IF obj # NIL THEN
			DefineTProcMacros(obj^.left, empty);
			IF (obj^.mode = TProc) & (obj = BaseTProc(obj)) & ((OPG.currFile # OPG.HeaderFile) OR (obj^.vis # internal)) THEN
				OPG.WriteString("#define __");
				Ident(obj);
				DeclareParams(obj^.link, TRUE);
				OPG.WriteString(" __SEND(");
				IF obj^.link^.typ^.form = Pointer THEN
					OPG.WriteString("__TYPEOF("); Ident(obj^.link); OPG.Write(")")
				ELSE Ident(obj^.link); OPG.WriteString(TagExt)
				END ;
				Str1(", #, ", obj^.num);
				DeclareBase(obj);
(*
				IF obj^.typ = OPT.notyp THEN OPG.WriteString(VoidType) ELSE Ident(obj^.typ^.strobj) END ;
*)
				IF obj.typ.form  # NoTyp THEN Stars(obj.typ, oc) END;
				OPG.WriteString("(*)");
				IF ansi THEN
					AnsiParamList(obj^.link, FALSE);
				ELSE
					OPG.WriteString("()");
				END ;
				DeclareReturnType(obj.typ);
				OPG.WriteString(", ");
				DeclareParams(obj^.link, TRUE);
				OPG.Write(")"); OPG.WriteLn
			END ;
			DefineTProcMacros(obj^.right, empty)
		END
	END DefineTProcMacros;

	PROCEDURE DefineType(str: OPT.Struct); (* define a type object *)
		VAR obj, field, par: OPT.Object; empty: BOOLEAN;
	BEGIN
		IF (OPG.currFile = OPG.BodyFile) OR (str^.ref < OPM.MaxStruct (*for hidden exports*) ) THEN
			obj := str^.strobj;
			IF (obj = NIL) OR Undefined(obj) THEN
				IF obj # NIL THEN (* check for cycles *)
					IF obj^.linkadr = ProcessingType THEN
						IF str^.form # Pointer THEN OPM.Mark(244, str^.txtpos); obj^.linkadr := PredefinedType END
					ELSE obj^.linkadr := ProcessingType
					END
				END ;
				IF str^.comp = Record THEN
					(* the following exports the base type of an exported type even if the former is non-exported *)
					IF str^.BaseTyp # NIL THEN DefineType(str^.BaseTyp) END ;
					field := str^.link;
					WHILE (field # NIL) & (field^.mode = Fld) DO
						IF (field^.vis # internal) OR (OPG.currFile = OPG.BodyFile) THEN DefineType(field^.typ) END ;
						field := field^.link
					END
				ELSIF str^.form = Pointer THEN
					IF str^.BaseTyp^.comp # Record THEN DefineType(str^.BaseTyp) END
				ELSIF str^.comp IN {Array, DynArr} THEN
					DefineType(str^.BaseTyp)
				ELSIF str^.form = ProcTyp THEN
					IF str^.BaseTyp # OPT.notyp THEN DefineType(str^.BaseTyp) END ;
					field := str^.link;
					WHILE field # NIL DO DefineType(field^.typ); field := field^.link END
				END
			END ;
			IF (obj # NIL) & Undefined(obj) THEN
				OPG.WriteString("typedef"); OPG.WriteLn; OPG.Write(Tab); Indent(1);
				obj^.linkadr := ProcessingType;
				DeclareBase(obj); OPG.Write(Blank);
				obj^.typ^.strobj := NIL; (* SG: trick to make DeclareObj declare the type *)
				DeclareObj(obj, 0);
				obj^.typ^.strobj := obj; (* SG: revert trick *)
				obj^.linkadr := 3+OPG.currFile;
				EndStat; Indent(-1); OPG.WriteLn;
				IF obj^.typ^.comp = Record THEN empty := TRUE;
					DeclareTProcs(str^.link, empty); DefineTProcMacros(str^.link, empty);
					IF ~empty THEN OPG.WriteLn END
				END
			END
		END
	END DefineType;

	PROCEDURE Prefixed(x: OPT.ConstExt;  y: ARRAY OF SHORTCHAR): BOOLEAN;
		VAR i: SHORTINT;
	BEGIN i := 0;
		WHILE x[i+1] = y[i] DO INC(i) END ;
		RETURN y[i] = 0X
	END Prefixed;

	PROCEDURE CProcDefs(obj: OPT.Object; vis: SHORTINT);
		VAR i: SHORTINT; ext: OPT.ConstExt;
	BEGIN
		IF obj # NIL THEN
			CProcDefs(obj^.left, vis);
			(* bug: obj.history cannot be used to cover unexported and deleted CProcs; use special flag obj.adr = 1 *)
			IF (obj^.mode = CProc) & (obj^.vis >= vis) & (obj^.adr = 1) THEN
				ext := obj.conval.ext; i := 1;
				IF (LEN(ext) > 1) & ((ext[1] # "#") & ~(Prefixed(ext, "extern ") OR Prefixed(ext, Extern))) THEN
					OPG.WriteString("#define "); Ident(obj);
					DeclareParams(obj^.link, TRUE);
					OPG.Write(Tab);
				END ;
				FOR i := i TO ORD(obj.conval.ext[0]) DO OPG.Write(obj.conval.ext[i]) END;
				OPG.WriteLn
			END ;
			CProcDefs(obj^.right, vis)
		END
	END CProcDefs;

	PROCEDURE TypeDefs* (obj: OPT.Object; vis(*replaced by test on currFile in DefineType*): SHORTINT);
	BEGIN
		IF obj # NIL THEN
			TypeDefs(obj^.left, vis);
			(* test typ.txtpos to skip types that have been unexported; obj.history # removed is not enough!*)
			IF (obj^.mode = Typ) & (obj^.typ^.txtpos > 0) THEN DefineType(obj^.typ) END ;
			TypeDefs(obj^.right, vis)
		END
	END TypeDefs;

	PROCEDURE DefAnonRecs(n: OPT.Node);
		VAR o: OPT.Object; typ: OPT.Struct;
	BEGIN
		WHILE (n # NIL) & (n^.class = Ninittd) DO
			typ := n^.typ;
			IF (typ^.strobj = NIL) (* & ((OPG.currFile = OPG.BodyFile) OR (typ.ref < OPM.MaxStruct)) *) THEN
				DefineType(typ);	(* declare base and field types, if any *)
				NEW(o); o.typ := typ; o.name := OPT.null; DeclareBase(o); EndStat; OPG.WriteLn
				(* simply defines a named struct, but not a type;
					o.name = "" signals field list expansion for DeclareBase in this very special case *)
			END ;
			n := n^.link
		END
	END DefAnonRecs;


	PROCEDURE Universal (typ: OPT.Struct): BOOLEAN;
		VAR p: OPT.Object;
	BEGIN
		IF typ.comp IN {Array, Record} THEN
			RETURN (typ.strobj # NIL) & (typ.strobj.name^ # "")
		ELSIF typ.form IN {Comp, Pointer} THEN
			RETURN Universal(typ.BaseTyp)
		ELSIF typ.form = ProcTyp THEN
			p := typ.link;
			WHILE p # NIL DO
				IF ~Universal(p.typ) THEN RETURN FALSE END;
				p := p.link
			END;
			RETURN Universal(typ.BaseTyp)
		END;
		RETURN TRUE
	END Universal;

	PROCEDURE UniversalArrayName (typ: OPT.Struct);
		VAR p: OPT.Object;
	BEGIN
		IF typ.form IN {Bool, Char8, Char16, Int8, Int16, Int32, Int64, Set, Real32, Real64} THEN
			IF typ.BaseTyp # OPT.undftyp THEN typ := typ.BaseTyp END;	(* basic type alias *)
			OPG.WriteOrigString(typ.strobj.name^)
		ELSE
			IF typ.comp = Record THEN
				ASSERT((typ.strobj # NIL) & (typ.strobj.name^ # ""), 100);
				Ident(typ.strobj)
			ELSIF typ.comp = Array THEN
				ASSERT((typ.strobj # NIL) & (typ.strobj.name^ # ""), 100);
				Ident(typ.strobj); OPG.WriteString("_OBJ")
			ELSIF typ.comp = DynArr THEN
				UniversalArrayName(typ.BaseTyp); OPG.WriteString("_ARRAY")
			ELSIF typ.form = Pointer THEN
				UniversalArrayName(typ.BaseTyp); OPG.WriteString("_PTR")
			ELSIF typ.form = ProcTyp THEN
				OPG.WriteString("X__"); p := typ.link;
				WHILE p # NIL DO
					UniversalArrayName(p.typ); OPG.WriteString("__");
					p := p.link
				END;
				OPG.WriteString("X");
				IF typ.BaseTyp # OPT.notyp THEN
					OPG.WriteString("R");
					OPG.WriteString("__"); UniversalArrayName(typ.BaseTyp)
				END
			ELSE
				HALT(100)
			END
		END
	END UniversalArrayName;

	PROCEDURE DefAnonArrays;
		VAR i: INTEGER; atyp, typ, t: OPT.Struct; obj: OPT.Object;
	BEGIN
		i := 0;
		WHILE i < arrayIndex DO
			atyp := arrayTypes[i]; typ := atyp;
			IF Universal(atyp) THEN
				OPG.WriteString("#ifndef "); UniversalArrayName(atyp); OPG.WriteString("_DEF"); OPG.WriteLn;
				OPG.WriteString("#define "); UniversalArrayName(atyp); OPG.WriteString("_DEF"); OPG.WriteLn
			END;
			OPG.WriteString("typedef struct ");
			IF Universal(atyp) THEN UniversalArrayName(atyp) ELSE Andent(atyp) END;
			OPG.Write(" "); BegBlk;
			BegStat; OPG.WriteString("INTEGER gc[3]");
			IF atyp.comp = DynArr THEN
				Str1(", len[#]", atyp.n + 1);
				WHILE typ^.comp = DynArr DO typ := typ^.BaseTyp END;	(* remove open levels *)
				IF typ.form = Pointer THEN	(* replace by anonymous pointer *)
					NEW(t); t.BaseTyp := typ.BaseTyp;
					t.sysflag := typ.sysflag; t.untagged := typ.untagged; typ := t;
					typ.form := Pointer; typ.comp := Basic
				END;
				NEW(t); t.BaseTyp := typ; typ := t;	(* replace by one level with one element *)
				typ.form := Comp; typ.comp := Array; typ.n := 1
			END;
			EndStat;
			BegStat; NEW(obj);	(* aux. object for easy declaration *)
			obj.typ := typ; obj.mode := Fld;
			obj.name := OPT.NewName("data");
			obj.linkadr := UndefinedType; DeclareBase(obj); OPG.Write(Blank);  DeclareObj(obj, 0);
			EndStat; EndBlk0; OPG.Write(" ");
			IF Universal(atyp) THEN
				UniversalArrayName(atyp); OPG.Write(";"); OPG.WriteLn;
				OPG.WriteString("#endif "); OPG.WriteLn
			ELSE
				Andent(atyp); OPG.Write(";"); OPG.WriteLn
			END;
			OPG.WriteLn;
			INC(i)
		END
	END DefAnonArrays;

	PROCEDURE InsertArrayType* (typ: OPT.Struct);
		VAR i: INTEGER;
	BEGIN
		ASSERT(typ.comp IN {Array, DynArr});
		IF (typ.comp = DynArr) & (typ.BaseTyp.form IN {Bool, Char8, Char16, Int8, Int16, Int32, Int64, Real32, Real64, Set}) THEN
			(* declared in SYSTEM.h *)
		ELSE
			i := 0;
			WHILE i < arrayIndex DO
				IF OPT.EqualType(typ, arrayTypes[i]) THEN RETURN END;
				INC(i)
			END;
			arrayTypes[arrayIndex] := typ;
			INC(arrayIndex)
		END
	END InsertArrayType;

	PROCEDURE CleanupArrays*;
		VAR i: INTEGER;
	BEGIN
		i := 0;
		WHILE i < arrayIndex DO arrayTypes[i] := NIL; INC(i) END
	END CleanupArrays;


	PROCEDURE StructValue (typ: OPT.Struct);
	BEGIN
		IF typ.comp = Record THEN
			OPG.Write("("); Andent(typ); OPG.WriteString("__desc");
			OPG.WriteString(" + "); OPG.WriteInt(typ.n + 1); OPG.Write(")")
		ELSE
			Andent(typ); OPG.WriteString("__desc")
		END
	END StructValue;

	PROCEDURE StructDecl (typ: OPT.Struct);
	BEGIN
		IF typ = OPT.sysptrtyp THEN OPG.WriteString("13")
		ELSIF typ = OPT.anytyp THEN OPG.WriteString("11")
		ELSIF typ = OPT.anyptrtyp THEN OPG.WriteString("12")
		ELSIF typ = OPT.guidtyp THEN OPG.WriteString("33")
		ELSIF typ = OPT.restyp THEN OPG.WriteString("34")
		ELSE
			IF typ.mno # 0 THEN

			END;
			CASE typ.form OF
			| Undef, Byte, String8, NilTyp, NoTyp, String16: OPG.WriteString("0")
			| Bool, Char8: OPG.WriteInt(typ.form - 1)
			| Int8..Set: OPG.WriteInt(typ.form)
			| Char16: OPG.WriteString("3")
			| Int64: OPG.WriteString("10")
			| ProcTyp:
				IF typ.untagged OR (OPT.GlbMod[typ.mno].library # NIL) THEN OPG.WriteString("14")
				ELSE OPG.WriteString("(INTEGER)"); StructValue(typ)
				END
			| Pointer:
				IF typ.untagged OR (OPT.GlbMod[typ.mno].library # NIL) THEN OPG.WriteString("13")
				ELSE OPG.WriteString("(INTEGER)"); StructValue(typ)
				END
			| Comp:
				IF typ.untagged OR (OPT.GlbMod[typ.mno].library # NIL) THEN OPG.WriteString("0")
				ELSE OPG.WriteString("(INTEGER)"); StructValue(typ)
				END
			END
		END
	END StructDecl;

	PROCEDURE NameIdx (obj: OPT.Object): INTEGER;
		VAR n: INTEGER; o: OPT.Object; name: OPT.String;
	BEGIN
		n := 0; name := obj.name;
		IF name # OPT.null THEN
			IF obj.num = 0 THEN
				o := nameList;
				WHILE (o # NIL) & (o.name^ # name^) DO o := o.nlink END;
				IF o # NIL THEN
					obj.num := o.num
				ELSE
					obj.num := namex;
					WHILE name[n] # 0X DO INC(n) END;
					INC(namex, n + 1);
					obj.nlink := nameList; nameList := obj
				END
			END;
			n := obj.num;
		END;
		RETURN n
	END NameIdx;

	PROCEDURE ObjectDecl (obj, nameObj: OPT.Object);
		VAR vis, mode: INTEGER;
	BEGIN
		OPT.FPrintObj(obj); BegStat;
		IF obj.mode = Typ THEN OPG.WriteHexInt(obj.typ.pbfp)
		ELSIF obj.mode = Fld THEN OPG.Write("0")
		ELSE OPG.WriteHexInt(obj.fprint)
		END;
		OPG.WriteString(", ");
		IF obj.mode IN {LProc, XProc, TProc} THEN OPG.WriteString("(INTEGER)"); Ident(obj)
		ELSIF obj.mode = Var THEN OPG.WriteString("(INTEGER)&"); CompleteIdent(obj)
		ELSIF obj.mode = Typ THEN OPG.WriteHexInt(obj.typ.pvfp)
		ELSIF obj.mode = Fld THEN OPG.WriteInt(obj.adr)
		ELSE OPG.WriteInt(0)
		END;
		OPG.WriteString(", ");
		IF obj.vis = internal THEN vis := 1
		ELSIF obj.vis = externalR THEN vis := 2
		ELSIF obj.vis = external THEN vis := 4
		END;
		CASE obj.mode OF
		| Con, CProc: mode := 1
		| Typ: mode := 2
		| Var: mode := 3
		| LProc, XProc, TProc: mode := 4
		| Fld: mode := 5
		END;
		OPG.WriteInt(NameIdx(nameObj)); OPG.WriteString("<<8 | ");
		OPG.WriteString("0x"); OPG.WriteHex(vis * 16 + mode); OPG.WriteString(", ");
		IF obj.mode IN {Var, Typ, Fld} THEN StructDecl(obj.typ) ELSE OPG.Write("0") END;
		OPG.WriteString(","); OPG.WriteLn
	END ObjectDecl;

	PROCEDURE NumMeth (root: OPT.Object; num: INTEGER): OPT.Object;
		VAR obj: OPT.Object;
	BEGIN
		IF (root = NIL) OR (root.mode = TProc) & (root.num = num) THEN RETURN root END;
		obj := NumMeth(root.left, num);
		IF obj = NIL THEN obj := NumMeth(root.right, num) END;
		RETURN obj
	END NumMeth;

	PROCEDURE^ Size*(typ: OPT.Struct): INTEGER;	(* aligned size of typ *)

	PROCEDURE FindPtrs (typ: OPT.Struct; adr: INTEGER; var: OPT.Object; VAR num: INTEGER);
		VAR fld: OPT.Object; btyp: OPT.Struct; i, n: INTEGER;
	BEGIN
		IF (typ.form = Pointer) & ~typ.untagged THEN
			IF var # NIL THEN BegStat; OPG.WriteString("(INTEGER)&"); CompleteIdent(var); OPG.WriteString(" + ") END;
			OPG.WriteInt(adr); OPG.WriteString(", "); INC(num);
			IF var # NIL THEN OPG.WriteLn END
		ELSIF (typ.comp = Record) & (typ.sysflag # union) THEN
			btyp := typ.BaseTyp;
			IF btyp # NIL THEN FindPtrs(btyp, adr, var, num) END ;
			fld := typ.link;
			WHILE (fld # NIL) & (fld.mode = Fld) DO
				IF (fld.name^ = OPM.HdPtrName) THEN
					IF var # NIL THEN BegStat; OPG.WriteString("(INTEGER)&"); CompleteIdent(var); OPG.WriteString(" + ") END;
					OPG.WriteInt(fld.adr + adr); OPG.WriteString(", "); INC(num);
					IF var # NIL THEN OPG.WriteLn END
				ELSE FindPtrs(fld.typ, fld.adr + adr, var, num)
				END;
				fld := fld.link
			END
		ELSIF typ.comp = Array THEN
			btyp := typ.BaseTyp; n := typ.n;
			WHILE btyp.comp = Array DO n := btyp.n * n; btyp := btyp.BaseTyp END ;
			IF (btyp.form = Pointer) OR (btyp.comp = Record) THEN
				i := num; FindPtrs(btyp, adr, var, num);
				IF num # i THEN i := 1;
					WHILE i < n DO
						INC(adr, Size(btyp)); FindPtrs(btyp, adr, var, num); INC(i)
					END
				END
			END
		END
	END FindPtrs;

	PROCEDURE DescDecl (typ: OPT.Struct);
		VAR lev, size, form, nx, i: INTEGER; t, xb: OPT.Struct; m, fld: OPT.Object;
	BEGIN
		IF (typ.mno = 0) & (typ # OPT.anyptrtyp) & (typ.ext # done3) THEN
			typ.ext := done3;
			lev := 0; size := 0;
			IF typ.comp = Record THEN
				size := Size(typ); lev := typ.extlev;
				IF typ.attribute = extAttr THEN form := 5
				ELSIF typ.attribute = limAttr THEN form := 9
				ELSIF typ.attribute = absAttr THEN form := 13
				ELSE form := 1
				END;
				IF openArrayFields THEN
					BegStat; OPG.WriteString("static "); OPG.WriteString("SYSTEM_DIRECTORY ");
					Andent(typ); OPG.WriteString("__flds = "); BegBlk;
				ELSE
					BegStat; OPG.WriteString("static INTEGER "); Andent(typ); OPG.WriteString("__flds[] = "); BegBlk;
				END;
				fld := typ.link; i := 0;
				WHILE (fld # NIL) & (fld.mode = Fld) DO INC(i); fld := fld.link END;
				BegStat; OPG.WriteInt(i); OPG.WriteString(", "); OPG.WriteLn;
				fld := typ.link;
				WHILE (fld # NIL) & (fld.mode = Fld) DO ObjectDecl(fld, fld); fld := fld.link END;
				EndBlk0; EndStat
			ELSIF typ.comp = Array THEN
				IF typ.BaseTyp.form IN {Pointer, ProcTyp, Comp} THEN DescDecl(typ.BaseTyp) END;
				size := typ.n; form := 2
			ELSIF typ.comp = DynArr THEN
				IF typ.BaseTyp.form IN {Pointer, ProcTyp, Comp} THEN DescDecl(typ.BaseTyp) END;
				form := 2; lev := typ.n + 1
			ELSIF typ.form = Pointer THEN
				IF typ.BaseTyp.form IN {Pointer, ProcTyp, Comp} THEN DescDecl(typ.BaseTyp) END;
				form := 3
			ELSE ASSERT(typ.form = ProcTyp);
				OPM.FPrint(size, XProc); OPT.FPrintSign(size, typ.BaseTyp, typ.link); form := 0
			END;
			BegStat; OPG.WriteString(Export); OPG.WriteString("INTEGER ");
			Andent(typ); OPG.WriteString("__desc[] = "); BegBlk;
			IF typ.comp = Record THEN (* methods *)
				ASSERT(typ.allocated);
				xb := typ;
				REPEAT xb := xb.BaseTyp UNTIL (xb = NIL) OR (xb.mno # 0) OR xb.untagged;
				BegStat; OPG.WriteString("-1, "); OPG.WriteLn; i := typ.n;
				WHILE i > 0 DO
					DEC(i); t := typ;
					REPEAT
						m := NumMeth(t.link, i); t := t.BaseTyp
					UNTIL (m # NIL) OR (t = xb);
					BegStat;
					IF m # NIL THEN
						IF absAttr IN m.conval.setval THEN OPG.WriteString("0, ")
						ELSE OPG.WriteString("(INTEGER)"); Ident(m); OPG.WriteString(", ")
						END
					ELSIF (xb = NIL) OR xb.untagged THEN OPG.WriteString("0, ")	(* unimplemented ANYREC method *)
					ELSE OPG.WriteString("0, ")	(* FIXUP *)
					END;
					OPG.WriteLn
				END;
			END;
			BegStat; OPG.WriteInt(size); OPG.WriteString(","); OPG.WriteLn;
			BegStat; OPG.WriteString("(INTEGER)&");
			OPG.WriteString(OPG.modName); OPG.WriteString("__desc,"); OPG.WriteLn;
			IF typ.strobj # NIL THEN nx := NameIdx(typ.strobj) ELSE nx := 0 END;
			BegStat; OPG.WriteInt(nx); OPG.WriteString("<<8 | ");
			OPG.WriteString("0x"); OPG.WriteHex(lev * 16 + form); OPG.WriteString(","); OPG.WriteLn;
			IF typ.comp = Record THEN
				i := 0;
				WHILE i <= typ.extlev DO
					BegStat; t := typ;
					WHILE t.extlev > i DO t := t.BaseTyp END;
					StructDecl(t); OPG.Write(","); OPG.WriteLn;
					INC(i)
				END;
				IF i < 16 THEN
					BegStat;
					WHILE i < 16 DO OPG.WriteString("0, "); INC(i) END;
					OPG.WriteLn
				END;
				BegStat; OPG.WriteString("(INTEGER)");
				IF openArrayFields THEN OPG.WriteString("&") END;
				Andent(typ); OPG.WriteString("__flds, "); OPG.WriteLn;
				BegStat; i := 0; FindPtrs(typ, 0, NIL, i); OPG.WriteInt(-(4 * i + 4)); OPG.WriteLn
			ELSIF typ.form # ProcTyp THEN
				BegStat; StructDecl(typ.BaseTyp); OPG.WriteLn
			ELSE
				OPG.Write("0")
			END;
			EndBlk0; EndStat
		END
	END DescDecl;

	PROCEDURE TDescDecl* (typ: OPT.Struct);
		VAR fld: OPT.Object; t: OPT.Struct;
	BEGIN
		DescDecl(typ); fld := typ.link;
		WHILE (fld # NIL) & (fld.mode = Fld) DO
			IF fld.history # removed THEN
				t := fld.typ;
				WHILE (t # NIL) & (t.form IN {Pointer, ProcTyp, Comp}) & (t.mno = 0) & (t # OPT.anyptrtyp) DO
					DescDecl(t);
					t := t.BaseTyp
				END
			END;
			fld := fld.link
		END;
	END TDescDecl;

	PROCEDURE ImportDecl (obj: OPT.Object; VAR num: INTEGER);
		VAR mod: OPT.Object;
	BEGIN
		IF obj # NIL THEN
			ImportDecl(obj^.left, num);
			IF (obj^.mode = Mod) & (obj^.mnolev < 0) THEN	(* @self and SYSTEM have mnolev = 0 *)
				mod := OPT.GlbMod[-obj^.mnolev];
				IF mod.library = NIL THEN
					BegStat; OPG.Write("&"); OPG.WriteString(mod.name^); OPG.WriteString("__desc,"); OPG.WriteLn;
					INC(num)
				END
			END;
			ImportDecl(obj^.right, num);
		END
	END ImportDecl;

	PROCEDURE TProcPrep (obj: OPT.Object; VAR num: INTEGER);
	BEGIN
		IF obj # NIL THEN
			TProcPrep(obj.left, num);
			IF (obj.mode IN {LProc, XProc, TProc}) & (obj.conval.setval * {absAttr, empAttr} = {}) THEN
				INC(num);
				TProcPrep(obj.scope.right, num)
			END;
			TProcPrep(obj.right, num)
		END
	END TProcPrep;

	PROCEDURE TProcDecl (obj, owner: OPT.Object);
		VAR o: OPT.Object;
	BEGIN
		IF obj # NIL THEN
			TProcDecl(obj.left, owner);
			IF (obj.mode IN {LProc, XProc, TProc}) & (obj.conval.setval * {absAttr, empAttr} = {}) THEN
				o := OPT.NewObj();
				o.name := OPT.NewName(owner.name + "." + obj.name);
				ObjectDecl(obj, o);
				TProcDecl(obj.scope.right, o)
			END;
			TProcDecl(obj.right, owner)
		END
	END TProcDecl;

	PROCEDURE ExportPrep (obj: OPT.Object; VAR num: INTEGER);
	BEGIN
		IF obj # NIL THEN
			ExportPrep(obj^.left, num);
			IF (obj.history # removed)
				& ((obj.vis # internal)
					OR fullExportList & (obj.mode IN {Var, LProc, XProc}) & (obj.name^ # "")
					OR (obj.mode = Typ) & (obj.typ.strobj = obj) & (obj.typ.form = Comp)) THEN
				INC(num);
				IF (obj.mode IN {Var, Typ, Fld}) & (obj.typ.form IN {Pointer, ProcTyp, Comp}) & (obj.typ.mno = 0) THEN
					DescDecl(obj.typ)
				END;
				IF ~dynamicLink THEN
					IF fullExportList & (obj.mode = Typ) THEN
						TProcPrep(obj.typ.link, num)	(* methods *)
					ELSIF fullExportList & (obj.mode IN {LProc, XProc}) THEN
						TProcPrep(obj.scope.right, num)	(* local procedures *)
					END
				END
			END;
			ExportPrep(obj^.right, num);
		END
	END ExportPrep;

	PROCEDURE ExportDecl (obj: OPT.Object);
	BEGIN
		IF obj # NIL THEN
			ExportDecl(obj^.left);
			IF (obj.history # removed)
				& ((obj.vis # internal)
					OR fullExportList & (obj.mode IN {Var, LProc, XProc}) & (obj.name^ # "")
					OR (obj.mode = Typ) & (obj.typ.strobj = obj) & (obj.typ.form = Comp)) THEN
				ObjectDecl(obj, obj);
				IF ~dynamicLink THEN
					IF fullExportList & (obj.mode = Typ) THEN
						TProcDecl(obj.typ.link, obj)	(* methods *)
					ELSIF fullExportList & (obj.mode IN {LProc, XProc}) THEN
						TProcDecl(obj.scope.right, obj)	(* local procedures *)
					END
				END
			END;
			ExportDecl(obj^.right);
		END
	END ExportDecl;

	PROCEDURE ModDescDecl* (close: BOOLEAN);
		VAR imps, exps, ptrs, i, x: INTEGER; var, a, b, c: OPT.Object; name: OPT.String;
	BEGIN
		imps := 0; exps := 0; ptrs := 0;
		ExportPrep(OPT.topScope^.right, exps);
		OPG.WriteString("static SYSTEM_MODDESC *");
		OPG.WriteString(OPG.modName); OPG.WriteString("__imp[] = "); BegBlk;
		ImportDecl(OPT.topScope^.right, imps);
		IF imps = 0 THEN BegStat; OPG.Write("0"); OPG.WriteLn END;
		EndBlk0; EndStat;
		IF openArrayFields THEN
			OPG.WriteString("static SYSTEM_DIRECTORY ");
			OPG.WriteString(OPG.modName); OPG.WriteString("__exp = "); BegBlk;
		ELSE
			OPG.WriteString("static INTEGER ");
			OPG.WriteString(OPG.modName); OPG.WriteString("__exp[] = "); BegBlk;
		END;
		BegStat; OPG.WriteInt(exps); OPG.WriteString(", "); OPG.WriteLn;
		ExportDecl(OPT.topScope^.right); EndBlk0; EndStat;
		IF namex > MaxNameTab THEN OPM.err(242) END;

		a := nameList; nameList := NIL; b := NIL;
		WHILE a # NIL DO c := a; a := c.nlink; c.nlink := b; b := c END;
		OPG.WriteString("static char ");
		OPG.WriteString(OPG.modName); OPG.WriteString("__names[] = "); BegBlk;
		BegStat; OPG.WriteString("0,"); OPG.WriteLn; x := 1;
		WHILE b # NIL DO
			BegStat; i := 0; ASSERT(b.num = x); name := b.name;
			WHILE name[i] # 0X DO OPG.Write("'"); OPG.Write(name[i]); OPG.WriteOrigString("',"); INC(i); INC(x) END;
			OPG.WriteString("0,"); OPG.WriteLn; INC(x); b := b.nlink
		END;
		EndBlk0; EndStat;
(*
		OPG.WriteString("static char ");
		OPG.WriteString(OPG.modName); OPG.WriteString("__names[] = "); OPG.WriteLn; Indent(1);
		BegStat; OPG.WriteString('"\0"'); 	(* names[0] = 0X *)
		WHILE b # NIL DO
			OPG.WriteLn;
			BegStat; OPG.Write('"'); OPG.WriteOrigString(b.name^); OPG.WriteString('\0"'); b := b.nlink
		END;
		OPG.Write(";"); OPG.WriteLn; Indent(-1);
*)
		OPG.WriteString("static INTEGER ");
		OPG.WriteString(OPG.modName); OPG.WriteString("__ptrs[] = "); BegBlk;
		var := OPT.topScope.scope;
		WHILE var # NIL DO
			FindPtrs(var.typ, 0, var, ptrs); var := var.link
		END;
		IF ptrs = 0 THEN BegStat; OPG.Write("0"); OPG.WriteLn END;
		EndBlk0; EndStat;
		OPG.WriteString("struct SYSTEM_MODDESC ");
		OPG.WriteString(OPG.modName); OPG.WriteString("__desc = "); BegBlk;
		BegStat; OPG.WriteString("0, "); OPG.WriteInt(ORD(options)); OPG.WriteString(", 0, /* next, opts, refcnt */ "); OPG.WriteLn;
		BegStat; OPG.Write("{"); OPG.WriteTime(); OPG.WriteString("}, /* compTime */ "); OPG.WriteLn;
		BegStat; OPG.WriteString("{0, 0, 0, 0, 0, 0}, /* loadTime */ "); OPG.WriteLn;
		BegStat; OPG.WriteString(OPG.modName); OPG.WriteString("__body,"); OPG.WriteLn;
		BegStat;
		IF close THEN OPG.WriteString(OPG.modName); OPG.WriteString("__close,")
		ELSE OPG.WriteString("0,")
		END;
		OPG.WriteLn;
		BegStat; OPG.WriteInt(imps); OPG.WriteString(", /* nofimps */ "); OPG.WriteLn;
		BegStat; OPG.WriteInt(ptrs); OPG.WriteString(", /* nofptrs */ "); OPG.WriteLn;
		BegStat; OPG.WriteString("0, 0, 0, 0, 0, 0, 0, 0, /* csize..varBase */ "); OPG.WriteLn;
		BegStat; OPG.WriteString(OPG.modName); OPG.WriteString("__names,"); OPG.WriteLn;
		BegStat; OPG.WriteString(OPG.modName); OPG.WriteString("__ptrs,"); OPG.WriteLn;
		BegStat; OPG.WriteString(OPG.modName); OPG.WriteString("__imp,"); OPG.WriteLn;
		BegStat;
		IF openArrayFields THEN OPG.Write("&") ELSE OPG.WriteString("(SYSTEM_DIRECTORY*)") END;
		OPG.WriteString(OPG.modName); OPG.WriteString("__exp,"); OPG.WriteLn;
		BegStat; OPG.Write('"'); OPG.WriteOrigString(OPG.modName); OPG.Write('"'); OPG.WriteLn;
		EndBlk0; EndStat
	END ModDescDecl;

	PROCEDURE Align*(VAR adr: INTEGER; base: INTEGER);
	BEGIN
		CASE base OF
		|  2: INC(adr, adr MOD 2)
		|  4: INC(adr, (-adr) MOD 4)
		|  8: INC(adr, (-adr) MOD 8)
		|16: INC(adr, (-adr) MOD 16)
		ELSE (*1*)
		END
	END Align;

	PROCEDURE Base*(typ: OPT.Struct): INTEGER;	(* alignment of typ *)
	BEGIN
		IF typ^.comp = Record THEN RETURN typ^.align MOD 10000H
		ELSIF typ.form = Comp THEN RETURN Base(typ^.BaseTyp)
		ELSE RETURN typ.size
		END
	END Base;

	PROCEDURE Size*(typ: OPT.Struct): INTEGER;	(* aligned size of typ *)
		VAR size: INTEGER;
	BEGIN
		size := typ.size;
		IF typ^.comp = Record THEN
			IF size = 0 THEN size := 1 END;
			Align(size, typ.align MOD 10000H);
		END;
		RETURN size
	END Size;

	PROCEDURE FillGap(gap, off, align: INTEGER; VAR n, curAlign: INTEGER);
		VAR adr: INTEGER;
	BEGIN
		adr := off; Align(adr, align);
		IF (curAlign < align) & (gap - (adr - off) >= align) THEN (* preserve alignment of the enclosing struct! *)
			DEC(gap, (adr - off) + align);
			BegStat;
			IF align = OPT.int16typ.size THEN OPG.WriteString("SHORTINT")
			ELSIF align = OPT.int32typ.size THEN OPG.WriteString("INTEGER")
			ELSIF align = OPT.int64typ.size THEN OPG.WriteString("LONGINT")
			END ;
			Str1(" _prvt#", n); INC(n); EndStat;
			curAlign := align
		END ;
		IF gap > 0 THEN BegStat; Str1("char _prvt#", n); INC(n); Str1("[#]", gap); EndStat END
	END FillGap;

	PROCEDURE FieldList (typ: OPT.Struct; last: BOOLEAN; VAR off, n, curAlign: INTEGER);
		VAR fld: OPT.Object; base: OPT.Struct; gap, adr, align, fldAlign: INTEGER;
	BEGIN
		fld := typ.link; align := typ^.align MOD 10000H;
		IF typ.BaseTyp # NIL THEN FieldList(typ.BaseTyp, FALSE, off, n, curAlign)
		ELSE off := 0; n := 0; curAlign := 1
		END ;
		WHILE (fld # NIL) & (fld.mode = Fld) DO
			IF (OPG.currFile = OPG.HeaderFile) & (fld.vis = internal) OR
				(OPG.currFile = OPG.BodyFile) & (fld.vis = internal) & (typ^.mno # 0) THEN
				fld := fld.link;
				WHILE (fld # NIL) & (fld.mode = Fld) & (fld.vis = internal) DO fld := fld.link END ;
			ELSE
				(* mimic OPV.TypSize to detect gaps caused by private fields *)
				adr := off; fldAlign := Base(fld^.typ); Align(adr, fldAlign);
				gap := fld.adr - adr;
				IF fldAlign > curAlign THEN curAlign := fldAlign END ;
				IF gap > 0 THEN FillGap(gap, off, align, n, curAlign) END ;
				BegStat; DeclareBase(fld); OPG.Write(Blank); DeclareObj(fld, 0);
				off := fld.adr + Size(fld.typ); base := fld.typ; fld := fld.link;
				WHILE (fld # NIL) & (fld.mode = Fld) & (fld.typ = base) & (fld.adr = off)
(* ?? *)		& ((OPG.currFile = OPG.BodyFile) OR (fld.vis # internal) OR (fld.typ.strobj = NIL)) DO
					OPG.WriteString(", "); DeclareObj(fld, 0); off := fld.adr + Size(fld.typ); fld := fld.link
				END ;
				EndStat
			END
		END ;
		IF last THEN
			adr := typ.size; (* unaligned size *)
			IF adr = 0 THEN gap := 1 (* avoid empty struct *) ELSE gap := adr - off END ;
			IF gap > 0 THEN FillGap(gap, off, align, n, curAlign) END
		END
	END FieldList;

	PROCEDURE HasPtrs (typ: OPT.Struct): BOOLEAN;
		VAR fld: OPT.Object;
	BEGIN
		IF typ.form IN {Pointer, ProcTyp} THEN
			RETURN TRUE
		ELSIF typ.comp = Record THEN
			fld := typ.link;
			WHILE (fld # NIL) & (fld.mode = Fld) DO
				IF (fld.name^ = OPM.HdPtrName) OR (fld.name^ = OPM.HdProcName) OR HasPtrs(fld.typ) THEN RETURN TRUE END ;
				fld := fld.link
			END;
			IF typ.BaseTyp # NIL THEN RETURN HasPtrs(typ.BaseTyp) END
		ELSIF typ.comp = Array THEN
			RETURN HasPtrs(typ.BaseTyp)
		END;
		RETURN FALSE
	END HasPtrs;

	PROCEDURE IdentList (obj: OPT.Object; vis: SHORTINT);
	(* generate var and param lists; vis: 0 all global vars, local var, 1 exported(R) var, 2 par list, 3 scope var, 4 array copy*)
		VAR base: OPT.Struct; first: BOOLEAN; lastvis: SHORTINT;
	BEGIN
		base := NIL; first := TRUE;
		WHILE (obj # NIL) & (obj^.mode # TProc) DO
			IF (vis IN {0, 2})
			OR ((vis = 1) & (obj^.vis # 0))
			OR ((vis = 3) & ~obj^.leaf)
			OR (vis = 4) & (obj^.typ^.comp = Array) & (obj^.mode = Var) THEN
				IF (obj^.typ # base) OR (obj^.vis # lastvis) THEN	(* new variable base type definition required *)
					IF ~first THEN EndStat END ;
					first := FALSE;
					base := obj^.typ; lastvis := obj^.vis;
					BegStat;
					IF (vis = 1) & (obj^.vis # internal) THEN OPG.WriteString(Extern)
					ELSIF (obj^.mnolev = 0) & (vis = 0) THEN
						IF obj^.vis = internal THEN OPG.WriteString(Static)
						ELSE OPG.WriteString(Export)
						END
					END ;
					IF (vis = 2) & (obj^.mode = Var) & (base^.form = Real32) THEN OPG.WriteString("double")
					ELSE DeclareBase(obj)
					END
				ELSE OPG.Write(",");
				END ;
				OPG.Write(Blank);
				IF (vis = 2) & (obj^.mode = Var) & (base^.form = Real32) THEN OPG.Write("_") END ;
				DeclareObj(obj, vis);
				IF (obj^.typ^.comp = DynArr) & ~obj.typ.untagged THEN (* declare len parameter(s) *)
					EndStat; BegStat;
					base := OPT.int32typ;
					OPG.WriteString("INTEGER "); LenList(obj, FALSE, TRUE)
				ELSIF (obj^.mode = VarPar) & (obj^.typ^.comp = Record) & ~obj.typ.untagged THEN
					EndStat; BegStat;
					OPG.WriteString("SYSTEM_TYPEDESC *"); Ident(obj); OPG.WriteString(TagExt);
					base := NIL
				ELSIF ptrinit & (vis = 0) & (obj^.mnolev > 0) THEN
					IF obj^.typ^.form IN {Pointer, ProcTyp} THEN OPG.WriteString(" = NIL")
					ELSIF (obj.typ.form = Comp) & HasPtrs(obj.typ) THEN OPG.WriteString(" = {0}")
					END
				END
			END ;
			obj := obj^.link
		END ;
		IF ~first THEN EndStat END
	END IdentList;

	PROCEDURE AnsiParamList (obj: OPT.Object; showParamNames: BOOLEAN);
		VAR name: OPT.String;
	BEGIN
		OPG.Write("(");
		IF (obj = NIL) OR (obj^.mode = TProc) THEN OPG.WriteString("void")
		ELSE
			LOOP
				DeclareBase(obj);
				IF showParamNames THEN
					OPG.Write(Blank); DeclareObj(obj, 0)
				ELSE
					name := obj^.name;  obj^.name := OPT.null; DeclareObj(obj, 0); obj^.name := name
				END ;
				IF (obj^.typ^.comp = DynArr) & ~obj.typ.untagged THEN
					OPG.WriteString(", INTEGER ");
					LenList(obj, TRUE, showParamNames)
				ELSIF (obj^.mode = VarPar) & (obj^.typ^.comp = Record) & ~obj.typ.untagged THEN
					OPG.WriteString(", SYSTEM_TYPEDESC *");
					IF showParamNames THEN Ident(obj); OPG.WriteString(TagExt) END
				END ;
				IF (obj^.link = NIL) OR (obj^.link.mode = TProc) THEN EXIT END ;
				OPG.WriteString(", ");
				obj := obj^.link
			END
		END ;
		OPG.Write(")")
	END AnsiParamList;

	PROCEDURE DeclareReturnType (retTyp: OPT.Struct);
	BEGIN
		IF (retTyp.form = ProcTyp) & ((retTyp^.strobj = NIL) OR (retTyp^.strobj^.name^ = "")) THEN
			IF ansi THEN OPG.Write(")"); AnsiParamList(retTyp^.link, FALSE)
			ELSE OPG.WriteString(")()")
			END;
			DeclareReturnType(retTyp.BaseTyp)
		END
	END DeclareReturnType;

	PROCEDURE ProcHeader(proc: OPT.Object; define: BOOLEAN);
		VAR oc: BOOLEAN;
	BEGIN
		DeclareBase(proc);
(*
		IF proc^.typ = OPT.notyp THEN OPG.WriteString(VoidType) ELSE Ident(proc^.typ^.strobj) END ;
*)
		OPG.Write(Blank);
		IF proc.typ.form  # NoTyp THEN Stars(proc.typ, oc) END;
		IF proc.sysflag = callback THEN OPG.WriteString("__CALLBACK ") END;
		Ident(proc); OPG.Write(Blank);
		IF ansi THEN
			AnsiParamList(proc^.link, TRUE);
			DeclareReturnType(proc.typ);
			IF ~define THEN OPG.Write(";") END ;
			OPG.WriteLn;
		ELSIF define THEN
			DeclareParams(proc^.link, FALSE);
			DeclareReturnType(proc.typ);
			OPG.WriteLn;
			Indent(1); IdentList(proc^.link, 2(* map REAL to double *)); Indent(-1)
		ELSE OPG.WriteString("();"); OPG.WriteLn
		END
	END ProcHeader;

	PROCEDURE ProcPredefs (obj: OPT.Object; vis: BYTE); (* forward declaration of procedures *)
	BEGIN
		IF obj # NIL THEN
			ProcPredefs(obj^.left, vis);
			IF (obj^.mode IN {LProc, XProc}) & (obj^.vis >= vis) & ((obj^.history # removed) OR (obj^.mode = LProc)) THEN
				(* previous XProc may be deleted or become LProc after interface change*)
				IF vis = external THEN OPG.WriteString(Extern)
				ELSIF obj^.vis = internal THEN OPG.WriteString(Static)
				ELSE OPG.WriteString(Export)
				END ;
				ProcHeader(obj, FALSE);
			END ;
			ProcPredefs(obj^.right, vis);
		END;
	END ProcPredefs;

	PROCEDURE Include(name: ARRAY OF SHORTCHAR; ext: BOOLEAN);
	BEGIN
		OPG.WriteString("#include ");
		IF ext OR (name[0] # "<") THEN OPG.Write(Quotes) END;
		OPG.WriteString(name);
		IF ext THEN OPG.WriteString(".h") END;
		IF ext OR (name[0] # "<") THEN OPG.Write(Quotes) END;
		OPG.WriteLn
	END Include;

	PROCEDURE IncludeImports(obj: OPT.Object; vis: SHORTINT);
		VAR mod: OPT.Object; i: INTEGER;
	BEGIN
		i := 1;
		WHILE i < OPT.nofGmod DO
			mod := OPT.GlbMod[i];
			IF mod.vis >= vis THEN
				IF mod.library # NIL THEN
					Include(mod.library^, FALSE);
					IF mod.library^ = "windows.h" THEN windows := TRUE END
				ELSE
					Include(mod.name^, TRUE)	(* use unaliased module name *)
				END
			END;
			INC(i)
		END
(*
		IF obj # NIL THEN
			IncludeImports(obj^.left, vis);
			IF (obj^.mode = Mod) & (obj^.mnolev < 0) THEN	(* @self and SYSTEM have mnolev = 0 *)
				mod := OPT.GlbMod[-obj^.mnolev];
				IF mod.vis >= vis THEN
					IF mod.library # NIL THEN
						Include(mod.library^, FALSE);
						IF mod.library^ = "windows.h" THEN windows := TRUE END
					ELSE
						Include(mod.name^, TRUE)	(* use unaliased module name *)
					END
				END
			END;
			IncludeImports(obj^.right, vis);
		END;
*)
	END IncludeImports;

	PROCEDURE GenDynTypeDesc (typ: OPT.Struct; vis: INTEGER);
	BEGIN
		ASSERT(typ.mno = 0);
		IF vis = external THEN
			IF typ.ext # done1 THEN
				typ.ext := done1;
				(* IF typ.strobj # NIL THEN *)
					BegStat; OPG.WriteString(Extern);
					OPG.WriteString("INTEGER "); Andent(typ); OPG.WriteString("__desc[]"); EndStat;
					BegStat; OPG.WriteString(Extern);
					OPG.WriteString("SYSTEM_TYPEDESC *"); Andent(typ); OPG.WriteString(DynTypExt);
					EndStat
				(* END *)
			END
		ELSE
			IF typ.ext # done2 THEN
				typ.ext := done2;
				BegStat; OPG.WriteString(Export);
				OPG.WriteString("INTEGER "); Andent(typ); OPG.WriteString("__desc[]"); EndStat;
				BegStat; OPG.WriteString(Export);
				OPG.WriteString("SYSTEM_TYPEDESC *"); Andent(typ); OPG.WriteString(DynTypExt);
				OPG.WriteString(" = (SYSTEM_TYPEDESC*)"); StructValue(typ); EndStat
			END
		END
	END GenDynTypeDesc;

	PROCEDURE GenExpTypes (obj: OPT.Object; vis: INTEGER);
		VAR num: INTEGER;
	BEGIN
		IF obj # NIL THEN
			GenExpTypes(obj^.left, vis);
			IF (obj.mode IN {Var, Typ, Fld}) & (obj.typ.form IN {Pointer, ProcTyp, Comp}) & (obj.history # removed) & (obj.typ.mno = 0)
					& ((obj.vis # internal) OR (obj.mode = Typ) & (obj.typ.strobj = obj) & (obj.typ.form = Comp))
					& (obj.typ # OPT.anyptrtyp) & (obj.typ # OPT.anytyp) THEN
				GenDynTypeDesc(obj.typ, vis)
			END;
			GenExpTypes(obj^.right, vis);
		END
	END GenExpTypes;

	PROCEDURE GenDynTypes (n: OPT.Node; vis: INTEGER);
		VAR fld: OPT.Object; t: OPT.Struct;
	BEGIN
		WHILE (n # NIL) & (n^.class = Ninittd) DO
			GenDynTypeDesc(n.typ, vis);
			fld := n.typ.link;
			WHILE (fld # NIL) & (fld.mode = Fld) DO
				IF fld.history # removed THEN
					t := fld.typ;
					WHILE (t # NIL) & (t.form IN {Pointer, ProcTyp, Comp}) & (t.mno = 0) & (t # OPT.anyptrtyp) DO
						GenDynTypeDesc(t, vis);
						t := t.BaseTyp
					END
				END;
				fld := fld.link
			END;
			n := n^.link
		END;
		GenExpTypes(OPT.topScope^.right, vis)
	END GenDynTypes;

	PROCEDURE GenHdr*(n: OPT.Node);
	BEGIN
		(* includes are delayed until it is known which ones are needed in the header *)
		OPG.currFile := OPG.HeaderFile;
		DefAnonRecs(n);
		TypeDefs(OPT.topScope^.right, 1); OPG.WriteLn;
		DefAnonArrays;
		IdentList(OPT.topScope^.scope, 1); OPG.WriteLn;
		GenDynTypes(n, external); OPG.WriteLn;
		ProcPredefs(OPT.topScope^.right, 1); OPG.WriteLn;
		OPG.WriteString(Extern); OPG.WriteString("void ");
		OPG.WriteString(OPG.modName); OPG.WriteString("__reg()"); EndStat;
		OPG.WriteString(Extern); OPG.WriteString("void ");
		OPG.WriteString(OPG.modName); OPG.WriteString("__body()"); EndStat;
		OPG.WriteString(Extern); OPG.WriteString("struct SYSTEM_MODDESC ");
		OPG.WriteString(OPG.modName); OPG.WriteString("__desc"); EndStat;
		CProcDefs(OPT.topScope^.right, 1); OPG.WriteLn;
		OPG.WriteString("#endif"); OPG.WriteLn
	END GenHdr;

	PROCEDURE GenHeaderMsg;
		VAR i: INTEGER;
	BEGIN
		OPG.WriteString("/* "); OPG.WriteString(HeaderMsg);
		FOR i := 0 TO 31 DO
			IF i IN OPG.opt THEN
				CASE i OF	(* c.f. ScanOptions in OPM *)
				| OPG.extsf: OPG.Write("e")
				| OPG.newsf: OPG.Write("s")
				| OPG.mainprog: OPG.Write("m")
				| OPG.inxchk: OPG.Write("x")
				| OPG.vcpp: OPG.Write("v")
				| OPG.ranchk: OPG.Write("r")
				| OPG.typchk: OPG.Write("t")
				| OPG.assert: OPG.Write("a")
				| OPG.ansi: OPG.Write("k")
				| OPG.ptrinit: OPG.Write("p")
				| OPG.include0: OPG.Write("i")
				END
			END
		END;
		OPG.WriteString(" */"); OPG.WriteLn
	END GenHeaderMsg;

	PROCEDURE GenHdrIncludes*;
	BEGIN
		OPG.currFile := OPG.HeaderInclude;
		GenHeaderMsg;
		OPG.WriteLn;
		OPG.WriteString("#ifndef "); OPG.WriteString(OPG.modName); OPG.WriteString(FlagExt); OPG.WriteLn;
		OPG.WriteString("#define "); OPG.WriteString(OPG.modName); OPG.WriteString(FlagExt); OPG.WriteLn;
		OPG.WriteLn;
		Include(BasicIncludeFile, TRUE);
		IncludeImports(OPT.topScope^.right, 1); OPG.WriteLn
	END GenHdrIncludes;

	PROCEDURE GenBdy*(n: OPT.Node; close: BOOLEAN);
	BEGIN
		OPG.currFile := OPG.BodyFile;
		GenHeaderMsg;
		Include(BasicIncludeFile, TRUE);
		IncludeImports(OPT.topScope^.right, 0); OPG.WriteLn;
		DefAnonRecs(n);
		TypeDefs(OPT.topScope^.right, 0); OPG.WriteLn;
		DefAnonArrays;
		IdentList(OPT.topScope^.scope, 0); OPG.WriteLn;
		GenDynTypes(n, internal); OPG.WriteLn;
		ProcPredefs(OPT.topScope^.right, 0); OPG.WriteLn;
		CProcDefs(OPT.topScope^.right, 0);
		OPG.WriteString(Export); OPG.WriteString("void ");
		OPG.WriteString(OPG.modName); OPG.WriteString("__reg()"); EndStat;
		OPG.WriteString(Export); OPG.WriteString("void ");
		OPG.WriteString(OPG.modName); OPG.WriteString("__body()"); EndStat;
		IF close THEN
			OPG.WriteString("static "); OPG.WriteString("void ");
			OPG.WriteString(OPG.modName); OPG.WriteString("__close()"); EndStat
		END;
		OPG.WriteString(Export); OPG.WriteString("struct SYSTEM_MODDESC ");
		OPG.WriteString(OPG.modName); OPG.WriteString("__desc");
		EndStat; OPG.WriteLn; OPG.WriteLn;
	END GenBdy;

	PROCEDURE InitImports(obj: OPT.Object);
		VAR mod: OPT.Object; i: INTEGER;
	BEGIN
		i := 1;
		WHILE i < OPT.nofGmod DO
			mod := OPT.GlbMod[i];
			IF (mod.link # NIL) & (mod.library = NIL) THEN
				BegStat; OPG.WriteString(mod.name); OPG.WriteString("__body()"); EndStat
			END;
			INC(i)
		END
(*
		IF obj # NIL THEN
			InitImports(obj^.left);
			IF (obj^.mode = Mod) & (obj^.mnolev # 0) THEN
				mod := OPT.GlbMod[-obj^.mnolev];
				IF mod.library = NIL THEN
					BegStat; OPG.WriteString(mod.name); OPG.WriteString("__body()"); EndStat
				END
			END ;
			InitImports(obj^.right)
		END
*)
	END InitImports;

	PROCEDURE RegImports(obj: OPT.Object);
		VAR mod: OPT.Object; i: INTEGER;
	BEGIN
		i := 1;
		WHILE i < OPT.nofGmod DO
			mod := OPT.GlbMod[i];
			IF (mod.link # NIL) & (mod.library = NIL) THEN
				BegStat; OPG.WriteString(mod.name); OPG.WriteString("__reg()"); EndStat
			END;
			INC(i)
		END
(*
		IF obj # NIL THEN
			RegImports(obj^.left);
			IF (obj^.mode = Mod) & (obj^.mnolev # 0) THEN
				mod := OPT.GlbMod[-obj^.mnolev];
				IF mod.library = NIL THEN
					BegStat; OPG.WriteString(mod.name); OPG.WriteString("__reg()"); EndStat
				END
			END ;
			RegImports(obj^.right)
		END
*)
	END RegImports;

	PROCEDURE RegEntry* (n: OPT.Node);
		VAR typ, t, xb: OPT.Struct; i: INTEGER; m: OPT.Object;
	BEGIN
		OPG.WriteLn;
		OPG.WriteString(Export); OPG.WriteString("void "); OPG.WriteString(OPG.modName);
		OPG.WriteString("__reg()"); OPG.WriteLn;
		BegBlk;
		BegStat; OPG.WriteString("__BEGREG(");
		OPG.WriteString(OPG.modName); OPG.WriteString("__desc"); OPG.Write(")"); EndStat;
		RegImports(OPT.topScope^.right);
		BegStat; OPG.WriteString("__REGMOD(");
		OPG.WriteString(OPG.modName); OPG.WriteString("__desc"); OPG.Write(")"); EndStat;
		WHILE (n # NIL) & (n^.class = Ninittd) DO	(* copy base methods to descriptor *)
			typ := n.typ; xb := typ;
			REPEAT xb := xb.BaseTyp UNTIL (xb = NIL) OR (xb.mno # 0) OR xb.untagged;
			i := typ.n;
			WHILE i > 0 DO
				DEC(i); t := typ;
				REPEAT
					m := NumMeth(t.link, i); t := t.BaseTyp
				UNTIL (m # NIL) OR (t = xb);
				IF (m = NIL) & (xb # NIL) & ~xb.untagged THEN
					BegStat;
					Andent(typ); OPG.WriteString("__desc["); OPG.WriteInt(typ.n - i); OPG.WriteString("] = ");
					Andent(xb); OPG.WriteString("__desc["); OPG.WriteInt(xb.n - i); OPG.WriteString("]");
					EndStat
				END
			END;
			n := n.link
		END;
		BegStat; OPG.WriteString("__ENDREG"); EndStat;
		EndBlk
	END RegEntry;

	PROCEDURE EnterBody*;
	BEGIN
		OPG.WriteLn;
		OPG.WriteString(Export); OPG.WriteString("void "); OPG.WriteString(OPG.modName);
		OPG.WriteString("__body()"); OPG.WriteLn;
		BegBlk;
		BegStat; OPG.WriteString("__BEGBODY(");
		OPG.WriteString(OPG.modName); OPG.WriteString("__desc"); OPG.Write(")"); EndStat;
		InitImports(OPT.topScope^.right)
	END EnterBody;

	PROCEDURE ExitBody*;
	BEGIN
		BegStat; OPG.WriteString("__ENDBODY"); EndStat;
		EndBlk
	END ExitBody;

	PROCEDURE EnterClose*;
	BEGIN
		OPG.WriteLn;
		OPG.WriteString("static void "); OPG.WriteString(OPG.modName);
		OPG.WriteString("__close()"); OPG.WriteLn;
		BegBlk;
		BegStat; OPG.WriteString("__BEGCLOSE(");
		OPG.WriteString(OPG.modName); OPG.WriteString("__desc"); OPG.Write(")"); EndStat
	END EnterClose;

	PROCEDURE ExitClose*;
	BEGIN
		BegStat; OPG.WriteString("__ENDCLOSE"); EndStat;
		EndBlk
	END ExitClose;

	PROCEDURE MainBody*;
	BEGIN
		OPG.WriteLn; OPG.WriteString(Export);
		IF ansi THEN
			OPG.WriteString("main(int argc, char **argv)");
		ELSE
			OPG.WriteString("main(argc, argv)"); OPG.WriteLn;
			OPG.Write(Tab); OPG.WriteString("int argc; char **argv;");
		END;
		OPG.WriteLn; BegBlk;
		BegStat; OPG.WriteString(OPG.modName); OPG.WriteString("__reg()"); EndStat;
		BegStat; OPG.WriteString(OPG.modName); OPG.WriteString("__body()"); EndStat;
		BegStat; OPG.WriteString("return 0"); EndStat;
		EndBlk;
		IF windows THEN
			OPG.WriteLn;
			OPG.WriteString("int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) ");
			OPG.WriteLn; BegBlk;
			BegStat; OPG.WriteString(OPG.modName); OPG.WriteString("__reg()"); EndStat;
			BegStat; OPG.WriteString(OPG.modName); OPG.WriteString("__body()"); EndStat;
			BegStat; OPG.WriteString("return 0"); EndStat;
			EndBlk
		END
	END MainBody;

	PROCEDURE DefineInter* (proc: OPT.Object); (* define intermediate scope record and variable *)
		VAR scope: OPT.Object;
	BEGIN
		scope := proc^.scope;
		OPG.WriteString(Static); OPG.WriteString(Struct); OPG.WriteString(scope^.name); OPG.Write(Blank);
		BegBlk;
		IdentList(proc^.link, 3); (* parameters *)
		IdentList(scope^.scope, 3); (* local variables *)
		BegStat; (* scope link field declaration *)
		OPG.WriteString(Struct); OPG.WriteString (scope^.name);
		OPG.Write(Blank); OPG.Write(Star); OPG.WriteString(LinkName); EndStat;
		EndBlk0; OPG.Write(Blank);
		OPG.Write(Star); OPG.WriteString (scope^.name); OPG.WriteString(GlobalScope); EndStat; OPG.WriteLn;
		ProcPredefs (scope^.right, 0);
		OPG.WriteLn;
	END DefineInter;

	PROCEDURE EnterProc* (proc: OPT.Object);
		VAR var, scope: OPT.Object; typ, t: OPT.Struct; dim: SHORTINT;
	BEGIN
		IF proc^.vis = internal THEN OPG.WriteString(Static) END ;
		ProcHeader(proc, TRUE);
		BegBlk;
		scope := proc^.scope;
		IdentList(scope^.scope, 0);
		IF ~scope^.leaf THEN (* declare intermediate procedure scope record variable*)
			BegStat; OPG.WriteString(Struct); OPG.WriteString (scope^.name);
			OPG.Write(Blank); OPG.WriteString(LocalScope); EndStat
		END ;
		IdentList(proc^.link, 4);	(* declare copy of fixed size value array parameters *)
		IF ~ansi THEN
			var := proc^.link;
			WHILE var # NIL DO (* "unpromote" value real parameters *)
				IF (var^.typ^.form = Real32) & (var^.mode = Var) THEN
					BegStat;
					Ident(var^.typ^.strobj); OPG.Write(Blank); Ident(var); OPG.WriteString(" = (SHORTREAL)_"); Ident(var);
					EndStat
				END ;
				var := var^.link
			END
		END ;
		IF dynamicLink THEN
			BegStat; OPG.WriteString('__ENTER("');
			OPG.WriteOrigString(OPT.SelfName);
			IF proc.mode = TProc THEN OPG.Write("."); OPG.WriteOrigString(proc.link.typ.strobj.name^) END;
			OPG.Write(".");
			IF proc.entry # NIL THEN OPG.WriteOrigString(proc.entry^)
			ELSE OPG.WriteOrigString(proc.name^)
			END;
			OPG.WriteString('")'); EndStat
		END;
		var := proc^.link;
		WHILE var # NIL DO (* copy value array parameters *)
			IF (var^.typ^.comp IN {Array, DynArr}) & (var^.mode = Var) & ~var^.typ^.untagged THEN
				BegStat;
				IF var^.typ^.comp = Array THEN
					OPG.WriteString(DupArrFunc);
					Ident(var); typ := var^.typ;
				ELSE
					OPG.WriteString(DupFunc);
					Ident(var); OPG.WriteString(Comma); Ident(var); OPG.WriteString(LenExt);
					typ := var^.typ^.BaseTyp; dim := 1;
					WHILE typ^.comp = DynArr DO
						OPG.WriteString(" * "); Ident(var); OPG.WriteString(LenExt); OPG.WriteInt(dim);
						typ := typ^.BaseTyp; INC(dim)
					END
				END;
				OPG.Write(CloseParen); EndStat
(*
			ELSIF initOutPointer & (var.mode = VarPar) & (var.vis = outPar) & HasPtrs(var.typ) THEN
*)
			ELSIF initOutPointer & (var.mode = VarPar) & (var.vis = outPar) & (var.typ.form IN {Pointer, ProcTyp}) THEN
				BegStat;
				OPG.WriteString("*"); Ident(var);
				IF var.typ.form IN {Pointer, ProcTyp} THEN OPG.WriteString(" = NIL")
				ELSE OPG.WriteString(" = {0}")
				END;
				EndStat
			END ;
			var := var^.link
		END ;
		IF ~scope^.leaf THEN
			var := proc^.link; (* copy addresses of parameters into local scope record *)
			WHILE var # NIL DO
				IF ~var^.leaf THEN (* only if used by a nested procedure *)
					BegStat;
					OPG.WriteString(LocalScope); OPG.Write(Dot); Ident(var);
					OPG.WriteString(Becomes);
					IF var^.typ^.comp IN {Array, DynArr} THEN OPG.WriteString("(void*)")
						(* K&R and ANSI differ in the type: array or element type*)
					ELSIF var^.mode # VarPar THEN OPG.Write("&")
					END ;
					Ident(var);
					IF (var^.typ^.comp = DynArr) & ~var.typ.untagged THEN
						typ := var^.typ; dim := 0;
						REPEAT (* copy len(s) *)
							OPG.WriteString("; ");
							OPG.WriteString(LocalScope); OPG.Write(Dot); Ident(var); OPG.WriteString(LenExt);
							IF dim # 0 THEN OPG.WriteInt(dim) END ;
							OPG.WriteString(Becomes); Ident(var); OPG.WriteString(LenExt);
							IF dim # 0 THEN OPG.WriteInt(dim) END ;
							typ := typ^.BaseTyp
						UNTIL typ^.comp # DynArr;
					ELSIF (var^.mode = VarPar) & (var^.typ^.comp = Record) & ~var.typ.untagged THEN
						OPG.WriteString("; ");
						OPG.WriteString(LocalScope); OPG.Write(Dot); Ident(var); OPG.WriteString(TagExt);
						OPG.WriteString(Becomes); Ident(var); OPG.WriteString(TagExt)
					END ;
					EndStat
				END;
				var := var^.link;
			END;
			var := scope^.scope; (* copy addresses of local variables into scope record *)
			WHILE var # NIL DO
				IF ~var^.leaf THEN (* only if used by a nested procedure *)
					BegStat;
					OPG.WriteString(LocalScope); OPG.Write(Dot); Ident(var); OPG.WriteString(Becomes);
					IF var^.typ^.comp # Array THEN OPG.Write("&")
					ELSE OPG.WriteString("(void*)")	(* K&R and ANSI differ in the type: array or element type*)
					END ;
					Ident(var); EndStat
				END ;
				var := var^.link
			END;
			(* now link new scope *)
			BegStat; OPG.WriteString(LocalScope); OPG.Write(Dot); OPG.WriteString(LinkName);
			OPG.WriteString(Becomes); OPG.WriteString(scope^.name); OPG.WriteString(GlobalScope); EndStat;
			BegStat; OPG.WriteString(scope^.name); OPG.WriteString(GlobalScope); OPG.WriteString(Becomes);
			OPG.Write("&"); OPG.WriteString(LocalScope); EndStat
		END
	END EnterProc;

	PROCEDURE ExitProc*(proc: OPT.Object; eoBlock, implicitRet: BOOLEAN);
		VAR var: OPT.Object; indent: BOOLEAN;
	BEGIN
		indent := eoBlock;
		IF implicitRet & (proc^.typ # OPT.notyp) THEN
			OPG.Write(Tab); OPG.WriteString("__RETCHK;"); OPG.WriteLn
		ELSIF ~eoBlock OR implicitRet THEN
			IF ~proc^.scope^.leaf THEN
				(* link scope pointer of nested proc back to previous scope *)
				IF indent THEN BegStat ELSE indent := TRUE END ;
				OPG.WriteString(proc^.scope^.name); OPG.WriteString(GlobalScope);
				OPG.WriteString(Becomes); OPG.WriteString(LocalScope); OPG.Write(Dot); OPG.WriteString(LinkName);
				EndStat
			END;
			(* delete array value parameters *)
			var := proc^.link;
			WHILE var # NIL DO
				IF (var^.typ^.comp = DynArr) & (var^.mode = Var) & ~var^.typ^.untagged THEN
					IF indent THEN BegStat ELSE indent := TRUE END ;
					OPG.WriteString(DelFunc); Ident(var); OPG.Write(CloseParen); EndStat
				END ;
				var := var^.link
			END;
			IF dynamicLink THEN
				IF indent THEN BegStat ELSE indent := TRUE END ;
				OPG.WriteString("__EXIT"); EndStat
			END
		END ;
		IF eoBlock THEN EndBlk; OPG.WriteLn
		ELSIF indent THEN BegStat
		END
	END ExitProc;

	PROCEDURE TypeOf*(ap: OPT.Object);
		VAR i: SHORTINT;
	BEGIN
		ASSERT(ap.typ.comp = Record);
		IF ap.mode = VarPar THEN
			IF ap.mnolev # OPG.level THEN	(*intermediate level var-par record; possible WITH-guarded*)
				OPG.WriteString(ap^.scope^.name); OPG.WriteString("_s->"); Ident(ap)
			ELSE (*local var-par record*)
				Ident(ap)
			END ;
			OPG.WriteString(TagExt)
		ELSIF ap^.typ^.strobj # NIL THEN
			Ident(ap^.typ^.strobj); OPG.WriteString(DynTypExt)
		ELSE Andent(ap.typ); OPG.WriteString(DynTypExt)	(*anonymous ap type, p^ *)
		END
	END TypeOf;

	PROCEDURE Cmp*(rel: SHORTINT);
	BEGIN
		CASE rel OF
			eql :
					OPG.WriteString(" == ");
		|	neq :
					OPG.WriteString(" != ");
		|	lss :
					OPG.WriteString(" < ");
		|	leq :
					OPG.WriteString(" <= ");
		|	gtr :
					OPG.WriteString(" > ");
		|	geq :
					OPG.WriteString(" >= ");
		END;
	END Cmp;

	PROCEDURE SetInclude* (exclude: BOOLEAN);
	BEGIN
		IF exclude THEN OPG.WriteString(" &= ~"); ELSE OPG.WriteString(" |= "); END;
	END SetInclude;

	PROCEDURE Increment* (decrement: BOOLEAN);
	BEGIN
		IF decrement THEN OPG.WriteString(" -= "); ELSE OPG.WriteString(" += "); END;
	END Increment;

	PROCEDURE Halt* (n: INTEGER);
	BEGIN
		Str1("__HALT(#)", n)
	END Halt;

	PROCEDURE Len* (obj: OPT.Object; array: OPT.Struct; dim: INTEGER);
		VAR d: INTEGER;
	BEGIN
		d := dim;
		WHILE d > 0 DO array := array^.BaseTyp; DEC(d) END ;
		IF array^.comp = DynArr THEN
			ASSERT(~array.untagged);
			CompleteIdent(obj); OPG.WriteString(LenExt);
			IF dim # 0 THEN OPG.WriteInt(dim) END
		ELSE (* array *)
			OPG.WriteInt(array^.n)
		END
(*
		IF array^.comp = DynArr THEN
			CompleteIdent(obj); OPG.WriteString(LenExt);
			IF dim # 0 THEN OPG.WriteInt(dim) END
		ELSE (* array *)
			WHILE dim > 0 DO array := array^.BaseTyp; DEC(dim) END ;
			OPG.WriteInt(array^.n)
		END
*)
	END Len;

	PROCEDURE Character (ch: CHAR; wide: BOOLEAN);
	BEGIN
		IF (ch = "\") OR (ch = "?") OR (ch = SingleQuote) OR (ch = Quotes) THEN OPG.Write("\"); OPG.Write(SHORT(ch))
		ELSIF (ch >= " ") & (ch <= "~") THEN OPG.Write(SHORT(ch))
		ELSIF wide THEN OPG.WriteString("\x"); OPG.WriteHex(ORD(ch) DIV 256); OPG.WriteHex(ORD(ch) MOD 256)
		ELSE ASSERT(ch <= 0FFX); OPG.WriteString("\x"); OPG.WriteHex(ORD(ch))
		END
	END Character;

	PROCEDURE Case*(caseVal: INTEGER; form: SHORTINT);
	VAR
		ch: SHORTCHAR;
	BEGIN
		OPG.WriteString(CaseStat);
		CASE form OF
		|	Char8 :
					IF (caseVal >= ORD(" ")) & (caseVal <= ORD("~")) THEN
						OPG.WriteString("(SHORTCHAR)");
						OPG.Write(SingleQuote);
						Character(CHR(caseVal), FALSE);
						OPG.Write(SingleQuote)
					ELSE
						OPG.WriteString("0x"); OPG.WriteHex (caseVal);
					END
		|	Char16:
					IF (caseVal >= ORD(" ")) & (caseVal <= ORD("~")) THEN
						OPG.WriteString("(_CHAR)");
						OPG.Write(SingleQuote);
						Character(CHR(caseVal), FALSE);
						OPG.Write(SingleQuote)
					ELSE
						OPG.WriteString("0x"); OPG.WriteHex (caseVal DIV 256 MOD 256); OPG.WriteHex (caseVal MOD 256);
					END
		|	Int8, Int16, Int32 :
					OPG.WriteInt (caseVal);
		END;
		OPG.WriteString(Colon);
	END Case;

	PROCEDURE GetLongWords (con: OPT.Const; OUT hi, low: INTEGER);
		CONST N = 4294967296.0; (* 2^32 *)
		VAR rh, rl: REAL;
	BEGIN
		rl := con.intval; rh := con.realval / N;
		IF rh >= MAX(INTEGER) + 1.0 THEN rh := rh - 1; rl := rl + N
		ELSIF rh < MIN(INTEGER) THEN rh := rh + 1; rl := rl - N
		END;
		hi := SHORT(ENTIER(rh));
		rl := rl + (rh - hi) * N;
		IF rl < 0 THEN hi := hi - 1; rl := rl + N
		ELSIF rl >= N THEN hi := hi + 1; rl := rl - N
		END;
		IF rl >= MAX(INTEGER) + 1.0 THEN rl := rl - N END;
		low := SHORT(ENTIER(rl))
	END GetLongWords;

	PROCEDURE Constant* (con: OPT.Const; form: SHORTINT);
		VAR i, len, x: INTEGER; ch, lch: CHAR; s: SET;
			hex, hi, low: INTEGER; skipLeading: BOOLEAN;
	BEGIN
		CASE form OF
			Byte:
					OPG.WriteInt(con^.intval)
		|	Bool:
					OPG.WriteInt(con^.intval)
		|	Char8:
					IF (con^.intval >= ORD(" ")) & (con^.intval <= ORD("~")) THEN
						OPG.WriteString("(SHORTCHAR)");
						OPG.Write(SingleQuote);
						Character(CHR(con^.intval), FALSE);
						OPG.Write(SingleQuote)
					ELSE
						OPG.WriteInt(con^.intval)
					END
		|	Char16:
					IF (con^.intval >= ORD(" ")) & (con^.intval <= ORD("~")) THEN
						OPG.WriteString("(_CHAR)");
						OPG.Write("L"); OPG.Write(SingleQuote);
						Character(CHR(con^.intval), TRUE);
						OPG.Write(SingleQuote)
					ELSE
						OPG.WriteInt(con^.intval)
					END
		|	Int8, Int16, Int32:
					OPG.WriteInt(con^.intval)
		|	Int64:
					GetLongWords(con, hi, low);
					IF (hi = 0) & (low >= 0) OR (hi = -1) & (low < 0) THEN
						OPG.WriteInt(low)
					ELSE
						OPG.WriteString("0x");
						OPG.WriteHex(hi DIV 1000000H MOD 256);
						OPG.WriteHex(hi DIV 10000H MOD 256);
						OPG.WriteHex(hi DIV 100H MOD 256);
						OPG.WriteHex(hi MOD 256);
						OPG.WriteHex(low DIV 1000000H MOD 256);
						OPG.WriteHex(low DIV 10000H MOD 256);
						OPG.WriteHex(low DIV 100H MOD 256);
						OPG.WriteHex(low MOD 256);
					END
		|	Real32:
					IF con.realval = INF THEN OPG.WriteString("__INFS")
					ELSIF con.realval = -INF THEN OPG.WriteString("-__INFS")
					ELSE OPG.WriteReal(con^.realval, "f")
					END
		|	Real64:
					IF con.realval = INF THEN OPG.WriteString("__INF")
					ELSIF con.realval = -INF THEN OPG.WriteString("-__INF")
					ELSE OPG.WriteReal(con^.realval, 0X)
					END
		|	Set:
					OPG.WriteString("0x");
					skipLeading := TRUE;
					s := con^.setval; i := MAX(SET) + 1;
					REPEAT
						hex := 0;
						REPEAT
							DEC(i); hex := 2 * hex;
							IF i IN s THEN INC(hex) END
						UNTIL i MOD 8 = 0;
						IF (hex # 0) OR ~skipLeading THEN
							OPG.WriteHex(hex);
							skipLeading := FALSE
						END
					UNTIL i = 0;
					IF skipLeading THEN OPG.Write("0") END
		|	String8:
					OPG.Write(Quotes);
					len := con^.intval2 - 1; i := 0; ch := " ";
					WHILE i < len DO
						lch := ch; ch := con^.ext^[i];
						IF ((lch < " ") OR (lch > "~")) & ((CAP(ch) >= "A") & (CAP(ch) <= "F") OR (ch >= "0") & (ch <= "9")) THEN
							OPG.Write(Quotes); OPG.Write(Quotes)
						END;
						Character(ch, FALSE); INC(i);
					END ;
					OPG.Write(Quotes)
		|	String16:
					IF longStringMacro THEN
						OPG.WriteString('__LSTR("');
						i := 0; OPM.GetUtf8(con^.ext, x, i); lch := " "; ch := CHR(x);
						WHILE x # 0 DO
							IF ((lch < " ") OR (lch > "~")) & ((CAP(ch) >= "A") & (CAP(ch) <= "F") OR (ch >= "0") & (ch <= "9")) THEN
								OPG.Write(Quotes); OPG.Write(Quotes)
							END;
							Character(ch, FALSE);
							OPM.GetUtf8(con^.ext, x, i); lch := ch; ch := CHR(x)
						END ;
						OPG.WriteString('")')
					ELSE
						OPG.Write("L"); OPG.Write(Quotes);
						i := 0; OPM.GetUtf8(con^.ext, x, i); lch := " "; ch := CHR(x);
						WHILE x # 0 DO
							IF ((lch < " ") OR (lch > "~")) & ((CAP(ch) >= "A") & (CAP(ch) <= "F") OR (ch >= "0") & (ch <= "9")) THEN
								OPG.Write(Quotes); OPG.Write("L"); OPG.Write(Quotes)
							END;
							Character(ch, TRUE);
							OPM.GetUtf8(con^.ext, x, i); lch := ch; ch := CHR(x)
						END ;
						OPG.Write(Quotes)
					END
		|	NilTyp:
					OPG.WriteString(NilConst);
		END;
	END Constant;


	PROCEDURE InitKeywords;
		VAR n, i: BYTE;

		PROCEDURE Enter(s: ARRAY OF SHORTCHAR);
			VAR h: SHORTINT;
		BEGIN h := PerfectHash(s); hashtab[h] := n; keytab[n] := s$; INC(n)
		END Enter;

	BEGIN n := 0;
		FOR i := 0 TO 104 DO hashtab[i] := -1 END ;
		Enter("asm");
		Enter("auto");
		Enter("break");
		Enter("case");
		Enter("char");
		Enter("const");
		Enter("continue");
		Enter("default");
		Enter("do");
		Enter("double");
		Enter("else");
		Enter("enum");
		Enter("extern");
		Enter("export");	(* pseudo keyword used by ofront *)
		Enter("float");
		Enter("for");
		Enter("fortran");
		Enter("goto");
		Enter("if");
		Enter("import");	(* pseudo keyword used by ofront*)
		Enter("int");
		Enter("long");
		Enter("register");
		Enter("return");
		Enter("short");
		Enter("signed");
		Enter("sizeof");
		Enter("static");
		Enter("struct");
		Enter("switch");
		Enter("typedef");
		Enter("union");
		Enter("unsigned");
		Enter("void");
		Enter("volatile");
		Enter("while");

(* what about common predefined names from cpp as e.g. 
               Operating System:   ibm, gcos, os, tss and unix
               Hardware:           interdata, pdp11,  u370,  u3b,
                                   u3b2,   u3b5,  u3b15,  u3b20d,
                                   vax, ns32000,  iAPX286,  i386,
                                   sparc , and sun
               UNIX system variant:
                                   RES, and RT
               The lint(1V) command:
                                   lint
 *)
	END InitKeywords;

BEGIN
	done1 := OPT.NewName("done1");
	done2 := OPT.NewName("done2");
	done3 := OPT.NewName("done3");
	InitKeywords
END OfrontCPC.
