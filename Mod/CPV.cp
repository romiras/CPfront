MODULE OfrontCPV;
(*
	C source code generator
	based on OfrontOPV by J. Templ
	8.12.99 bh

	todo:
*)

	IMPORT OPM := DevCPM, OPS := DevCPS, OPT := DevCPT, OPG := OfrontCPG, OPC := OfrontCPC, StdLog;

	CONST
		processor* = 1;

		(* object modes *)
		Var = 1; VarPar = 2; Con = 3; Fld = 4; Typ = 5; LProc = 6; XProc = 7;
		CProc = 9; IProc = 10; Mod = 11; TProc = 13;

		(* symbol values or ops *)
		times = 1; slash = 2; div = 3; mod = 4;
		and = 5; plus = 6; minus = 7; or = 8; eql = 9;
		neq = 10; lss = 11; leq = 12; gtr = 13; geq = 14;
		in = 15; is = 16; ash = 17; msk = 18; len = 19;
		conv = 20; abs = 21; cap = 22; odd = 23; not = 33;
		(*SYSTEM*)
		adr = 24; cc = 25; bit = 26; lsh = 27; rot = 28; val = 29;
		min = 34; max = 35; typfn = 36; size = 37;
		thisrecfn = 45; thisarrfn = 46;

		(* structure forms *)
		Undef = 0; Byte = 1; Bool = 2; Char8 = 3; Int8 = 4; Int16 = 5; Int32 = 6;
		Real32 = 7; Real64 = 8; Set = 9; String8 = 10; NilTyp = 11;
		Pointer = 13; ProcTyp = 14; Comp = 15;
		Char16 = 16; String16 = 17; Int64 = 18;

		(* composite structure forms *)
		Array = 2; DynArr = 3; Record = 4;

		(* nodes classes *)
		Nvar = 0; Nvarpar = 1; Nfield = 2; Nderef = 3; Nindex = 4; Nguard = 5; Neguard = 6;
		Nconst = 7; Ntype = 8; Nproc = 9; Nupto = 10; Nmop = 11; Ndop = 12; Ncall = 13;
		Ninittd = 14; Nenter = 18; Nassign = 19;
		Nifelse =20; Ncase = 21; Nwhile = 22; Nrepeat = 23; Nloop = 24; Nexit = 25;
		Nreturn = 26; Nwith = 27; Ntrap = 28; Ncomp = 30;

		(*function number*)
		assign = 0; newfn = 1; incfn = 13; decfn = 14;
		inclfn = 15; exclfn = 16; copyfn = 18; assertfn = 32;

		(*SYSTEM function number*)
		getfn = 24; putfn = 25; getrfn = 26; putrfn = 27; sysnewfn = 30; movefn = 31;

		(* COM function number *)
		validfn = 40; queryfn = 42;

		(*procedure flags*)
		hasBody = 1; isRedef = 2; slNeeded = 3; imVar = 4; isHidden = 29; isGuarded = 30; isCallback = 31;

		(* attribute flags (attr.adr, struct.attribute, proc.conval.setval) *)
		newAttr = 16; absAttr = 17; limAttr = 18; empAttr = 19; extAttr = 20;

		(* sysflag *)
		untagged = 1; noAlign = 3; align2 = 4; align8 = 6; union = 7;
		interface = 10; guarded = 8; noframe = 16;
		nilBit = 1; enumBits = 8; new = 1; iid = 2;
		stackArray = 120;

		super = 1;

		UndefinedType = 0; (* named type not yet defined *)
		ProcessingType = 1; (* pointer type is being processed *)
		PredefinedType = 2; (* for all predefined types *)
		DefinedInHdr = 3+OPG.HeaderFile; (* named type has been defined in header file *)
		DefinedInBdy = 3+OPG.BodyFile; (* named type has been defined in body file *)

		OpenParen = "(";
		CloseParen = ")";
		OpenBracket = "[";
		CloseBracket = "]";
		Blank = " ";
		Comma = ", ";
		Deref = "*";
		EntierFunc = "__ENTIER(";
		IsFunc = "__IS(";
		IsPFunc = "__ISP(";
		GuardPtrFunc = "__GUARDP(";
		GuardRecFunc = "__GUARDR(";
		TypeFunc = "__TYPEOF(";
		SetOfFunc = "__SETOF(";
		SetRangeFunc = "__SETRNG(";
		CopyFunc = "__COPY(";
		MoveFunc = "__MOVE(";
		GetFunc = "__GET(";
		PutFunc = "__PUT(";
		DynTypExt = "__typ";
		WithChk = "__WITHCHK";
		Break = "break";
		ElseStat = "else ";

		MinPrec = -1;
		MaxPrec = 12;
		ProcTypeVar = 11; (* precedence number when a call is made with a proc type variable *)

		internal = 0;

	TYPE
		ExitInfo = RECORD level, label: SHORTINT END ;


	VAR
		assert, inxchk, mainprog, ansi: BOOLEAN;
		stamp: SHORTINT;	(* unique number for nested objects *)
		recno: SHORTINT;	(* number of anonymous record types *)

		exit: ExitInfo;	(* to check if EXIT is simply a break *)
		nofExitLabels: SHORTINT;
		naturalAlignment: BOOLEAN;


	PROCEDURE Init*(opt: SET);
		CONST chk = 0; achk = 1; ass = 2;
	BEGIN
		OPC.Init(opt);
		stamp := 0; recno := 0; nofExitLabels := 0;
		assert := ass IN opt;
		inxchk := chk IN opt;
		mainprog := OPG.mainprog IN OPG.opt;
		ansi := OPG.ansi IN OPG.opt
	END Init;

	PROCEDURE NaturalAlignment(size, max: INTEGER): INTEGER;
		VAR i: INTEGER;
	BEGIN
		IF size >= max THEN RETURN max
		ELSE i := 1;
			WHILE i < size DO INC(i, i) END ;
			RETURN i
		END
	END NaturalAlignment;

	PROCEDURE TypSize*(typ: OPT.Struct);
		VAR f, c: SHORTINT; offset, size, base, fbase: INTEGER;
			fld: OPT.Object; btyp: OPT.Struct;
	BEGIN
		IF typ = OPT.undftyp THEN OPM.err(58)
		ELSIF typ^.size = -1 THEN
			f := typ^.form; c := typ^.comp;
			IF c = Record THEN btyp := typ^.BaseTyp;
				IF btyp = NIL THEN offset := 0; base := OPG.RecAlign;
				ELSE TypSize(btyp); offset := btyp^.size; base := btyp^.align
				END;
				fld := typ^.link;
				WHILE (fld # NIL) & (fld^.mode = Fld) DO
					btyp := fld^.typ; TypSize(btyp);
					size := OPC.Size(btyp); fbase := OPC.Base(btyp);
					IF typ.sysflag = union THEN
						fld^.adr := 0;
						IF size > offset THEN offset := size END
					ELSE
						OPC.Align(offset, fbase);
						fld^.adr := offset; INC(offset, size)
					END;
					IF fbase > base THEN base := fbase END ;
					fld := fld^.link
				END ;
				IF OPG.RecSize = 0 THEN base := NaturalAlignment(offset, OPG.RecAlign) END ;
				typ^.size := offset; typ^.align := base;
				typ.n := -1  (* methods not counted yet *)
			ELSIF c = Array THEN
				TypSize(typ^.BaseTyp);
				typ^.size := typ^.n * OPC.Size(typ^.BaseTyp);
			ELSIF f = Pointer THEN
				typ^.size := OPM.PointerSize;
				IF typ^.BaseTyp = OPT.undftyp THEN OPM.Mark(128, typ^.n)
				ELSE TypSize(typ^.BaseTyp)
				END;
				IF (typ.BaseTyp = OPT.anytyp) & (typ.strobj = NIL) THEN typ.strobj := OPT.anyptrtyp.strobj END
			ELSIF f = ProcTyp THEN
				typ^.size := OPM.ProcSize;
			ELSIF c = DynArr THEN
				btyp := typ^.BaseTyp; TypSize(btyp);
				IF (typ.sysflag = untagged) OR typ.untagged THEN typ.size := 4
				ELSIF btyp^.comp = DynArr THEN typ^.size := btyp^.size + 4	(* describes dim not size *)
				ELSE typ^.size := 8
				END ;
			END;
			IF typ^.strobj = NIL THEN
				INC(recno); INC(typ.align, recno * 10000H)
			END
		END
	END TypSize;

	PROCEDURE CountTProcs(rec: OPT.Struct);
		VAR btyp: OPT.Struct;

		PROCEDURE TProcs(obj: OPT.Object);	(* obj.mnolev = 0, TProcs of base type already counted *)
			VAR redef: OPT.Object;
		BEGIN
			IF obj # NIL THEN
				TProcs(obj.left);
				IF obj.mode = TProc THEN
					OPT.FindBaseField(obj.name^, rec, redef);
					IF redef # NIL THEN
						obj.num := redef.num (*mthno*);
						IF ~(isRedef IN obj.conval.setval) OR (redef.conval.setval * {extAttr, absAttr, empAttr} = {}) THEN
							OPM.Mark(119, rec.txtpos)
						END
					ELSE obj.num := rec.n; INC(rec.n)
					END ;
					IF obj.conval.setval * {hasBody, absAttr, empAttr} = {} THEN OPM.Mark(129, obj.adr) END
				END ;
				TProcs(obj.right)
			END
		END TProcs;

	BEGIN
		IF rec.n = -1 THEN
			IF rec.untagged THEN rec.n := 0 ELSE rec.n := OPT.anytyp.n END;
			btyp := rec.BaseTyp;
			IF btyp # NIL THEN CountTProcs(btyp); rec.n := btyp.n END;
			TProcs(rec.link)
		END
	END CountTProcs;

	PROCEDURE ^Traverse (obj, outerScope: OPT.Object; exported: BOOLEAN);

	PROCEDURE TraverseType (typ: OPT.Struct);
		VAR p: OPT.Object;
	BEGIN
		IF ~typ^.allocated THEN
			typ^.allocated := TRUE;
			TypSize(typ);
			IF typ^.comp = Record THEN
				CountTProcs(typ);
				IF typ.extlev > 14 THEN OPM.Mark(233, typ.txtpos) END;
				IF typ^.BaseTyp # NIL THEN TraverseType(typ^.BaseTyp) END;
				Traverse(typ^.link, typ^.strobj, FALSE)
			ELSIF typ^.form = Pointer THEN
				IF typ^.BaseTyp = OPT.undftyp THEN OPM.Mark(128, typ.txtpos) END;
				TraverseType(typ^.BaseTyp);
				IF ~typ.untagged & (typ.BaseTyp.comp IN {Array, DynArr}) THEN
					OPC.InsertArrayType(typ.BaseTyp)
				END
			ELSIF typ^.form = ProcTyp THEN
				TraverseType(typ^.BaseTyp);
				p := typ^.link;
				WHILE p # NIL DO TraverseType(p.typ); p := p.link END
			ELSE (* typ^.comp IN {Array, DynArr} *)
				 TraverseType(typ^.BaseTyp)
			END
		END
	END TraverseType;

	PROCEDURE Stamp(VAR s: OPT.String);
		VAR i, j, k: SHORTINT; n: ARRAY 10 OF SHORTCHAR; t: OPT.String;
	BEGIN INC(stamp);
		i := 0; j := stamp;
		WHILE s[i] # 0X DO INC(i) END ;
		IF i > 25 THEN i := 25 END ;
		IF i + 12 > LEN(s^) THEN NEW(t, i + 12); t^ := s$; s := t END;
		s[i] := "_"; s[i+1] := "_"; INC(i, 2); k := 0;
		REPEAT n[k] := SHORT(CHR((j MOD 10) + ORD("0"))); j := SHORT(j DIV 10); INC(k) UNTIL j = 0;
		REPEAT DEC(k); s[i] := n[k]; INC(i) UNTIL k = 0;
		s[i] := 0X;
	END Stamp;

	PROCEDURE Traverse (obj, outerScope: OPT.Object; exported: BOOLEAN);
		VAR mode: SHORTINT; scope, p: OPT.Object;
	BEGIN
		IF obj # NIL THEN
			Traverse(obj^.left, outerScope, exported);
			IF obj^.name[0] = "@" THEN obj^.name[0] := "_"; Stamp(obj^.name) END ;	(* translate and make unique @for, ... *)
			obj^.linkadr := UndefinedType;
			mode := obj^.mode;
			IF (mode = Typ) & ((obj^.vis # internal) = exported) THEN TraverseType(obj^.typ);
			ELSIF mode IN {Var, VarPar, Fld} THEN TraverseType(obj^.typ)
			END;
			IF ~exported THEN (* do this only once *)
				IF (mode IN {LProc, Typ}) & (obj^.mnolev > 0) THEN
					obj.entry := obj.name; NEW(obj.name, LEN(obj.name$) + 12); Stamp(obj^.name)
				END ;
				IF mode IN {Var, VarPar, Typ} THEN
					obj^.scope := outerScope
				ELSIF mode IN {LProc, XProc, TProc, CProc, IProc} THEN
					TraverseType(obj^.typ);
					p := obj^.link;
					WHILE p # NIL DO TraverseType(p.typ); p := p.link END;
					scope := obj^.scope;
					scope^.leaf := TRUE;
					NEW(scope.name, LEN(obj.name$) + 12); scope.name^ := obj.name$; Stamp(scope^.name);
					IF mode = CProc THEN obj^.adr := 1 (* c.f. OPC.CProcDefs *) END ;
					IF scope^.mnolev > 1 THEN outerScope^.leaf := FALSE END ;
					Traverse (obj^.scope^.right, obj^.scope, FALSE);
					IF obj.conval.setval * {hasBody, absAttr, empAttr} = {} THEN OPM.Mark(129, obj.adr) END
				END
			END;
			Traverse(obj^.right, outerScope, exported);
		END
	END Traverse;

	PROCEDURE AdrAndSize* (topScope: OPT.Object);
	BEGIN
		OPM.errpos := topScope^.adr;	(* text position of scope used if error *)
		topScope^.leaf := TRUE;
		Traverse(topScope^.right, topScope, TRUE);	(* first pass only on exported types and procedures	*)
		Traverse(topScope^.right, topScope, FALSE);	(* second pass *)
		(* mark basic types as predefined, OPC.Ident can avoid qualification*)
		OPT.char8typ^.strobj^.linkadr := PredefinedType;
		OPT.char16typ^.strobj^.linkadr := PredefinedType;
		OPT.settyp^.strobj^.linkadr := PredefinedType;
		OPT.real32typ^.strobj^.linkadr := PredefinedType;
		OPT.real64typ^.strobj^.linkadr := PredefinedType;
		OPT.int8typ^.strobj^.linkadr := PredefinedType;
		OPT.int16typ^.strobj^.linkadr := PredefinedType;
		OPT.int32typ^.strobj^.linkadr := PredefinedType;
		OPT.int64typ^.strobj^.linkadr := PredefinedType;
		OPT.booltyp^.strobj^.linkadr := PredefinedType;
		OPT.sysptrtyp^.strobj^.linkadr := PredefinedType;
		OPT.guidtyp^.strobj^.linkadr := PredefinedType;
		OPT.iunktyp^.strobj^.linkadr := PredefinedType;
		OPT.anyptrtyp^.strobj^.linkadr := PredefinedType;
		OPT.anytyp^.strobj^.linkadr := PredefinedType;
	END AdrAndSize;

(* ____________________________________________________________________________________________________________________________________________________________________ *)

	PROCEDURE Precedence (class, subclass, form, comp: SHORTINT): SHORTINT;
	BEGIN
		CASE class OF
			Nconst, Nvar, Nfield, Nindex, Nproc, Ncall:
					RETURN 10
		|	Nguard: IF OPG.typchk IN OPG.opt THEN RETURN 10 ELSE RETURN 9 (*cast*) END
		|	Nvarpar:
					IF comp IN {Array, DynArr} THEN RETURN 10 ELSE RETURN 9 END (* arrays don't need deref *)
		|	Nderef:
					RETURN 9
		|	Nmop:
					CASE subclass OF
						not, minus, adr, typfn, val, conv, size:
								RETURN 9
					|	is, abs, cap, odd, cc, bit:
								RETURN 10
					END
		|	Ndop:
					CASE subclass OF
						times:
								IF form = Set THEN RETURN 4 ELSE RETURN 8 END
					|	slash:
								IF form = Set THEN RETURN 3 ELSE RETURN 8 END
					|	div, mod:
								RETURN 10 (* div/mod are replaced by functions *)
					|	plus:
								IF form = Set THEN RETURN 2 ELSE RETURN 7 END
					|	minus:
								IF form = Set THEN RETURN 4 ELSE RETURN 7 END
					|	lss, leq, gtr, geq:
								RETURN 6
					|	eql, neq:
								RETURN 5
					|	and:
								RETURN 1
					|	or:
								RETURN 0
					|	len, in, ash, msk, bit, lsh, rot, min, max:
								RETURN 10
					END;
		|	Nupto:
					RETURN 10
		|	Ntype, Neguard: (* ignored anyway *)
					RETURN MaxPrec
		|	Ncomp:
					RETURN 10
		END;
	END Precedence;

	PROCEDURE^ expr (n: OPT.Node; prec: SHORTINT);
	PROCEDURE^ design(n: OPT.Node; prec: SHORTINT);

	PROCEDURE Len(n: OPT.Node; dim: INTEGER; incl0x: BOOLEAN);
		VAR d: INTEGER; array: OPT.Struct;
	BEGIN
		WHILE n^.class = Nindex DO INC(dim); n := n^.left END ;
		WHILE (n.class = Nmop) & (n.subcl = val) DO n := n.left END;
		IF n.typ.form = String8 THEN
			OPG.WriteString("__STRLENS("); expr(n, MinPrec); OPG.Write(CloseParen);
			IF incl0x THEN OPG.WriteString(" + 1") END
		ELSIF n.typ.form = String16 THEN
			OPG.WriteString("__STRLEN("); expr(n, MinPrec); OPG.Write(CloseParen);
			IF incl0x THEN OPG.WriteString(" + 1") END
		ELSIF (n^.class = Nderef) & (n^.typ^.comp = DynArr) THEN
			d := dim; array := n.typ;
			WHILE d > 0 DO array := array^.BaseTyp; DEC(d) END ;
			IF array.comp = DynArr THEN
				expr(n^.left, 10); OPG.WriteString("->len["); OPG.WriteInt(dim); OPG.Write("]")
			ELSE
				OPG.WriteInt(array^.n)
			END
		ELSE
			OPC.Len(n^.obj, n^.typ, dim)
		END
	END Len;

	PROCEDURE SameExp (n1, n2: OPT.Node): BOOLEAN;
	BEGIN
		WHILE (n1.class = n2.class) & (n1.typ = n2.typ) DO
			CASE n1.class OF
			| Nvar, Nvarpar, Nproc: RETURN n1.obj = n2.obj
			| Nconst: RETURN (n1.typ.form IN {Int8..Int32}) & (n1.conval.intval = n2.conval.intval)
			| Nfield: IF n1.obj # n2.obj THEN RETURN FALSE END
			| Nderef, Nguard:
			| Nindex: IF ~SameExp(n1.right, n2.right) THEN RETURN FALSE END
			| Nmop: IF (n1.subcl # n2.subcl) OR (n1.subcl = is) THEN RETURN FALSE END
			| Ndop: IF (n1.subcl # n2.subcl) OR ~SameExp(n1.right, n2.right) THEN RETURN FALSE END
			ELSE RETURN FALSE
			END ;
			n1 := n1.left; n2 := n2.left
		END;
		RETURN FALSE
	END SameExp;

	PROCEDURE SideEffects(n: OPT.Node): BOOLEAN;
	BEGIN
		IF n # NIL THEN RETURN (n^.class = Ncall) OR SideEffects(n^.left) OR SideEffects(n^.right)
		ELSE RETURN FALSE
		END
	END SideEffects;

	PROCEDURE Entier(n: OPT.Node; prec: SHORTINT);
	BEGIN
		IF n^.typ^.form IN {Real32, Real64} THEN
			OPG.WriteString(EntierFunc); expr(n, MinPrec); OPG.Write(CloseParen)
		ELSE expr(n, prec)
		END
	END Entier;

	PROCEDURE LEntier(n: OPT.Node; prec: SHORTINT);
	BEGIN
		IF n^.typ^.form IN {Real32, Real64} THEN
			OPG.WriteString("__ENTIERL("); expr(n, MinPrec); OPG.Write(CloseParen)
		ELSE expr(n, prec)
		END
	END LEntier;

	PROCEDURE Convert(n: OPT.Node; form, prec: SHORTINT);
	BEGIN
		CASE form OF
		| Int8: OPG.WriteString("(_BYTE)"); Entier(n, 9)
		| Int16: OPG.WriteString("(SHORTINT)"); Entier(n, 9)
		| Int32: OPG.WriteString("(INTEGER)"); Entier(n, 9)
		| Int64: OPG.WriteString("(LONGINT)"); LEntier(n, 9)
		| Char8: OPG.WriteString("(SHORTCHAR)"); Entier(n, 9)
		| Char16: OPG.WriteString("(_CHAR)"); Entier(n, 9)
		| Set: OPG.WriteString("(SET)"); Entier(n, 9)
		| Real32: OPG.WriteString("(SHORTREAL)"); expr(n, prec)
		| Real64: OPG.WriteString("(REAL)"); expr(n, prec)
		ELSE expr(n, prec)
		END
	END Convert;

	PROCEDURE TypeOf(n: OPT.Node);
	BEGIN
		IF n.class = Ntype THEN
			OPC.Andent(n^.typ); OPG.WriteString(DynTypExt)
		ELSIF n^.typ^.form = Pointer THEN
			OPG.WriteString(TypeFunc); expr(n, MinPrec); OPG.Write(")")
		ELSIF n^.class IN {Nvar, Nindex, Nfield} THEN	(* dyn rec type = stat rec type *)
			OPC.Andent(n^.typ); OPG.WriteString(DynTypExt)
		ELSIF n^.class = Nderef THEN	(* p^ *)
			OPG.WriteString(TypeFunc); expr(n^.left, MinPrec); OPG.Write(")")
		ELSIF n^.class = Nguard THEN	(* r(T) *)
			TypeOf(n^.left)	(* skip guard *)
		ELSIF (n^.class = Nmop) & (n^.subcl = val) THEN
			(*SYSTEM.VAL(typ, var par rec)*)
			OPC.TypeOf(n^.left^.obj)
		ELSE	(* var par rec *)
			OPC.TypeOf(n^.obj)
		END
	END TypeOf;

	PROCEDURE Index(n, d: OPT.Node; prec, dim: SHORTINT);
	BEGIN
		IF ~inxchk
		OR (n^.right^.class = Nconst) & ((n^.right^.conval^.intval = 0) OR (n^.left^.typ^.comp # DynArr))
		OR (n^.left^.typ^.comp = DynArr) & n.left.typ.untagged THEN
			expr(n^.right, prec)
		ELSE
			IF SideEffects(n^.right) THEN OPG.WriteString("__XF(") ELSE OPG.WriteString("__X(") END ;
			expr(n^.right, MinPrec); OPG.WriteString(Comma); Len(d, dim, FALSE); OPG.Write(CloseParen)
		END
	END Index;

	PROCEDURE Adr(n: OPT.Node; prec: SHORTINT);
	BEGIN
		IF n^.typ^.form IN {String8, String16} THEN expr(n, prec)
		ELSIF (n.class = Nderef) & (n.subcl = 0) & ~(n.typ.comp IN {Array, DynArr}) THEN expr(n.left, prec)
		ELSIF (n.class = Nvarpar) & ~(n.typ.comp IN {Array, DynArr}) THEN OPC.CompleteIdent(n.obj)
		ELSE OPG.Write("&"); expr(n, prec)
		END
	END Adr;

	PROCEDURE design(n: OPT.Node; prec: SHORTINT);
		VAR obj: OPT.Object; typ: OPT.Struct;
			class, designPrec, comp: SHORTINT;
			d, x: OPT.Node; dims, i: SHORTINT;
	BEGIN
		comp := n^.typ^.comp; obj := n^.obj; class := n^.class;
		designPrec := Precedence(class, n^.subcl, n^.typ^.form, comp);
		IF (class = Nvar) & (obj^.mnolev > 0) & (obj^.mnolev # OPG.level) & (prec = 10) THEN designPrec := 9 END ;
		IF prec > designPrec THEN OPG.Write(OpenParen) END;
		IF prec = ProcTypeVar THEN OPG.Write(Deref) END; (* proc var calls must be dereferenced in K&R C *)
		CASE class OF
			Nproc:
					OPC.Ident(n^.obj)
		|	Nvar:
					OPC.CompleteIdent(n^.obj)
		|	Nvarpar:
					IF ~(comp IN {Array, DynArr}) THEN OPG.Write(Deref) END; (* deref var parameter *)
					OPC.CompleteIdent(n^.obj)
		|	Nfield:
					IF n^.left^.class = Nderef THEN expr(n^.left^.left, designPrec); OPG.WriteString("->")
					ELSE design(n^.left, designPrec); OPG.Write(".")
					END ;
					OPC.Ident(n^.obj)
		|	Nderef:
					IF n.subcl # 0 THEN
						expr(n.left, designPrec)
					ELSE
						IF n^.typ^.comp IN {Array, DynArr} THEN
							IF n.typ.untagged THEN expr(n^.left, designPrec)
							ELSE expr(n^.left, 10); OPG.WriteString("->data")
							END
						ELSE OPG.Write(Deref); expr(n^.left, designPrec)
						END
					END
		|	Nindex:
					d := n^.left;
					IF d^.typ^.comp = DynArr THEN dims := 0;
						WHILE d^.class = Nindex DO d := d^.left; INC(dims) END ;
						IF n^.typ^.comp = DynArr THEN Adr(d, designPrec) ELSE design(d, designPrec) END;
						OPG.Write(OpenBracket);
						IF n^.typ^.comp = DynArr THEN OPG.Write("(") END ;
						i := dims; x := n;
						WHILE x # d DO	(* apply Horner schema *)
							IF x^.left # d THEN Index(x, d, 7, i); OPG.WriteString(" + "); Len(d, i, FALSE);  OPG.WriteString(" * ("); DEC(i)
							ELSE Index(x, d, MinPrec, i)
							END ;
							x := x^.left
						END ;
						FOR i := 1 TO dims DO OPG.Write(")") END ;
						IF n^.typ^.comp = DynArr THEN
							(*  element type is DynArr; finish Horner schema with virtual indices = 0*)
							OPG.Write(")");
							WHILE i <= d.typ.n DO
								OPG.WriteString(" * "); Len(d, i, FALSE);
								INC(i)
							END
						END ;
						OPG.Write(CloseBracket)
					ELSE
						design(n^.left, designPrec);
						OPG.Write(OpenBracket);
						Index(n, n^.left, MinPrec, 0);
						OPG.Write(CloseBracket)
					END
		|	Nguard:
					typ := n^.typ; obj := n^.left^.obj;
					IF OPG.typchk IN OPG.opt THEN
						IF typ^.comp = Record THEN OPG.WriteString(GuardRecFunc);
							IF obj^.mnolev # OPG.level THEN (*intermediate level var-par record*)
								OPG.WriteString(obj^.scope^.name); OPG.WriteString("__curr->"); OPC.Ident(obj)
							ELSE (*local var-par record*)
								OPC.Ident(obj)
							END ;
						ELSE (*Pointer*)
							IF typ^.BaseTyp^.strobj = NIL THEN OPG.WriteString("__GUARDA(")
							ELSE OPG.WriteString(GuardPtrFunc)
							END ;
							expr(n^.left, MinPrec); typ := typ^.BaseTyp
						END ;
						OPG.WriteString(Comma);
						OPC.Andent(typ); OPG.WriteString(Comma);
						OPG.WriteInt(typ^.extlev); OPG.Write(")")
					ELSE
						IF typ^.comp = Record THEN (* do not cast record directly, cast pointer to record *)
							OPG.WriteString("*("); OPC.Ident(typ^.strobj); OPG.WriteString("*)"); OPC.CompleteIdent(obj)
						ELSE (*simply cast pointer*)
							OPG.Write("("); OPC.Ident(typ^.strobj); OPG.Write(")"); expr(n^.left, designPrec)
						END
					END
		|	Neguard:
					HALT(100);
					IF OPG.typchk IN OPG.opt THEN
						IF n^.left^.class = Nvarpar THEN OPG.WriteString("__GUARDEQR(");
							OPC.CompleteIdent(n^.left^.obj); OPG.WriteString(Comma); TypeOf(n^.left);
						ELSE OPG.WriteString("__GUARDEQP("); expr(n^.left^.left, MinPrec)
						END ; (* __GUARDEQx includes deref *)
						OPG.WriteString(Comma); OPC.Ident(n^.left^.typ^.strobj); OPG.Write(")")
					ELSE
						expr(n^.left, MinPrec)	(* always lhs of assignment *)
					END
		|	Nmop:
					IF n^.subcl = val THEN design(n^.left, prec) END
		END ;
		IF prec > designPrec THEN OPG.Write(CloseParen) END
	END design;

	PROCEDURE ActualPar(n: OPT.Node; fp: OPT.Object);
		VAR typ, aptyp: OPT.Struct; comp, form, mode, prec, dim: SHORTINT; useAdr: BOOLEAN; n1: OPT.Node;
	BEGIN
		OPG.Write(OpenParen);
		WHILE n # NIL DO typ := fp^.typ;
			comp := typ^.comp; form := typ^.form; mode := fp^.mode; prec := MinPrec;
			IF (mode = VarPar) & (n.typ = OPT.niltyp) THEN
				OPG.WriteString("0")
			ELSIF (mode = VarPar) & ((n.subcl = thisarrfn) OR (n.subcl = thisrecfn)) THEN
				OPG.WriteString("(void*)"); expr(n.left, MinPrec); OPG.WriteString(Comma);
				IF n.subcl = thisrecfn THEN OPG.WriteString("(SYSTEM_TYPEDESC*)") END;
				expr(n.right, MinPrec)
			ELSE
				useAdr := FALSE;
				IF (mode = VarPar) & (n^.class = Nmop) & (n^.subcl = val) THEN	(* avoid cast in lvalue *)
					OPG.Write(OpenParen); OPC.Ident(n^.typ^.strobj); OPG.WriteString("*)"); prec := 10
				END ;
				IF ~(n^.typ^.comp IN {Array, DynArr}) THEN
					IF mode = VarPar THEN
						IF ansi & (typ # n^.typ) THEN OPG.WriteString("(void*)") END ;
						useAdr := TRUE; prec := 9
					ELSIF ansi THEN
						IF (comp IN {Array, DynArr}) & (n^.class = Nconst) THEN	(* force to unsigned char *)
							IF n.typ.form = String8 THEN OPG.WriteString("(SHORTCHAR*)")
							ELSE OPG.WriteString("(_CHAR*)")
							END
						ELSIF (form = Pointer) & (typ # n^.typ) & (n^.typ # OPT.niltyp) THEN
							OPG.WriteString("(void*)")	(* type extension *)
						END
					ELSE
						IF (form IN {Real32, Real64}) & (n^.typ^.form IN {Int8, Int16, Int32}) THEN (* real promotion *)
							OPG.WriteString("(REAL)"); prec := 9
						ELSIF (form = Int32) & (n^.typ^.form < Int32) THEN (* integral promotion *)
							OPG.WriteString("(INTEGER)"); prec := 9
						END
					END
				ELSIF ansi THEN
					(* casting of params should be simplified eventually *)
					IF (mode = VarPar) & (typ # n^.typ) & (prec = MinPrec) THEN OPG.WriteString("(void*)") END
				END ;
				n1 := n;
				IF (mode = VarPar) & (n^.class = Nmop) & (n^.subcl = val) THEN n1 := n.left END;	(* avoid cast in lvalue *)
				IF useAdr THEN Adr(n1, prec) ELSE expr(n1, prec) END ;
				IF (form = Int64) & (n^.class = Nconst)
				& (n^.conval^.intval <= MAX(INTEGER)) & (n^.conval^.intval >= MIN(INTEGER)) THEN
					OPG.Write("L")
				ELSIF (comp = Record) & (mode = VarPar) THEN
					IF ~typ.untagged THEN OPG.WriteString(", "); TypeOf(n) END
				ELSIF (comp = DynArr) & ~typ.untagged THEN
					IF n^.class = Nconst THEN (* ap is string constant *)
						OPG.WriteString(Comma); OPG.WriteInt(n^.conval^.intval2); (* OPG.Write("L") *)
					ELSE
						aptyp := n^.typ; dim := 0;
						WHILE (typ^.comp = DynArr) & (typ^.BaseTyp^.form # Byte) DO
							OPG.WriteString(Comma); Len(n, dim, TRUE);
							typ := typ^.BaseTyp; aptyp := aptyp^.BaseTyp; INC(dim)
						END ;
(*
						IF (typ^.comp = DynArr) & (typ^.BaseTyp^.form = Byte) THEN
							OPG.WriteString(Comma);
							WHILE aptyp^.comp = DynArr DO
								Len(n, dim, FALSE); OPG.WriteString(" * "); INC(dim); aptyp := aptyp^.BaseTyp
							END ;
							OPG.WriteInt(OPC.SIZE(aptyp)); (* OPG.Write("L") *)
						END
*)
					END
				END
			END;
			n := n^.link; fp := fp^.link;
			IF n # NIL THEN OPG.WriteString(Comma) END
		END ;
		OPG.Write(CloseParen)
	END ActualPar;

	PROCEDURE SuperProc(n: OPT.Node): OPT.Object;
		VAR obj: OPT.Object; typ: OPT.Struct;
	BEGIN typ := n^.right^.typ;	(* receiver type *)
		IF typ^.form = Pointer THEN typ := typ^.BaseTyp END ;
		OPT.FindField(n^.left^.obj^.name, typ^.BaseTyp, obj);
		RETURN obj
	END SuperProc;

	PROCEDURE StringModifier (n: OPT.Node);
		VAR trunc: BOOLEAN;
	BEGIN
		trunc := FALSE;
		WHILE (n.class = Nmop) & (n.subcl = conv) DO
			IF n.typ.form = String8 THEN trunc := TRUE END;
			n := n.left
		END;
		IF (n.typ.form = String8) OR (n.typ.form = Comp) & (n.typ.BaseTyp.form = Char8) THEN OPG.Write("S")
		ELSIF trunc THEN OPG.Write("T")
		ELSE OPG.Write("L")
		END
	END StringModifier;

	PROCEDURE^ compStat (n: OPT.Node; exp: BOOLEAN);

	PROCEDURE expr (n: OPT.Node; prec: SHORTINT);
		VAR
			class: SHORTINT;
			subclass: SHORTINT;
			form: SHORTINT;
			exprPrec: SHORTINT;
			typ: OPT.Struct;
			l, r: OPT.Node;
			proc: OPT.Object;
	BEGIN
		class := n^.class; subclass := n^.subcl; form := n^.typ^.form;
		l := n^.left; r := n^.right;
		exprPrec := Precedence (class, subclass, form, n^.typ^.comp);
		IF (exprPrec <= prec) & (class IN {Nconst, Nupto, Nmop, Ndop, Ncall, Nguard, Neguard}) THEN
			OPG.Write(OpenParen);
		END;
		CASE class OF
			Nconst:
					OPC.Constant(n^.conval, form)
		|	Nupto:	(* n^.typ = OPT.settyp *)
					OPG.WriteString(SetRangeFunc); expr(l, MinPrec); OPG.WriteString(Comma); expr (r, MinPrec);
					OPG.Write(CloseParen)
		|	Nmop:
					CASE subclass OF
						not:
								OPG.Write("!"); expr(l, exprPrec)
					|	minus:
								IF form = Set THEN OPG.Write("~") ELSE OPG.Write("-"); END ;
								expr(l, exprPrec)
					|	is:
								typ := n^.obj^.typ;
								IF l^.typ^.comp = Record THEN OPG.WriteString(IsFunc); OPC.TypeOf(l^.obj)
								ELSE OPG.WriteString(IsPFunc); expr(l, MinPrec); typ := typ^.BaseTyp
								END ;
								OPG.WriteString(Comma);
								OPC.Andent(typ); OPG.WriteString("__typ, ");
								OPG.WriteInt(typ^.extlev); OPG.Write(")")
					|	conv:
								IF form IN {String8, String16} THEN
									expr(l, exprPrec)
								ELSE
									Convert(l, form, exprPrec)
								END
					|	bit:
								OPG.WriteString(SetOfFunc); Entier(l, MinPrec); OPG.Write(CloseParen)
					|	abs:
								IF SideEffects(l) THEN
									IF l^.typ^.form = Real64 THEN OPG.WriteString("__ABSFD(")
									ELSIF l^.typ^.form = Real32 THEN OPG.WriteString("__ABSFF(")
									ELSIF l^.typ^.form = Int64 THEN OPG.WriteString("__ABSL(")
									ELSE OPG.WriteString("__ABSF(")
									END
								ELSE OPG.WriteString("__ABS(")
								END ;
								expr(l, MinPrec); OPG.Write(CloseParen)
					|	cap:
								OPG.WriteString("__CAP("); expr(l, MinPrec); OPG.Write(CloseParen)
					|	odd:
								OPG.WriteString("__ODD("); expr(l, MinPrec); OPG.Write(CloseParen)
					|	size:
								ASSERT(l^.class = Ntype);
								OPG.WriteString("sizeof ("); OPC.Andent(l^.typ); OPG.Write(CloseParen)
					|	typfn: (*SYSTEM*)
								OPG.WriteString("(INTEGER)"); TypeOf(l)
					|	adr: (*SYSTEM*)
								OPG.WriteString("(INTEGER)");
								IF l^.class = Ntype THEN TypeOf(l)
								ELSIF l^.class = Nvarpar THEN OPC.CompleteIdent(l^.obj)
								ELSE
									IF ~(l^.typ^.form IN {String8, String16}) & ~(l^.typ^.comp IN {Array, DynArr}) THEN Adr(l, exprPrec)
									ELSE expr(l, exprPrec)
									END
								END
					|	val: (*SYSTEM*)
								IF ~(n^.typ^.form IN {Real32, Real64, Comp})
								& ~(l^.typ^.form IN {Real32, Real64, Comp}) THEN	(* use cast *)
									OPG.Write(OpenParen); OPC.Ident(n^.typ^.strobj); OPG.Write(CloseParen);
									expr(l, exprPrec)
								ELSIF l^.class IN {Nvar, Nvarpar, Nfield, Nindex} THEN	(* lvalue *)
									OPG.WriteString("__VAL("); OPC.Ident(n^.typ^.strobj); OPG.WriteString(Comma);
									expr(l, MinPrec); OPG.Write(CloseParen)
								ELSE	(* use real <-> int conversions *)
									IF n.typ.form = Real32 THEN OPG.WriteString("__VALSR((INTEGER)")
									ELSIF n.typ.form = Real64 THEN OPG.WriteString("__VALR((LONGINT)")
									ELSE OPG.Write("("); OPC.Ident(n^.typ^.strobj); OPG.Write(")");
									END;
									IF l.typ.form = Real32 THEN OPG.WriteString("__VALI("); expr(l, MinPrec); OPG.Write(")")
									ELSIF l.typ.form = Real64 THEN  OPG.WriteString("__VALL("); expr(l, MinPrec); OPG.Write(")")
									ELSE expr(l, exprPrec)
									END;
									IF n.typ.form IN {Real32, Real64} THEN OPG.Write(")") END
								END
					ELSE OPM.err(217)
					END
		|	Ndop:
					CASE subclass OF
						len:
							Len(l, r^.conval^.intval, FALSE)
					| in, ash, msk, bit, lsh, rot, div, mod, min, max:
							CASE subclass OF
							|	in:
										OPG.WriteString("__IN(")
							|	ash:
										IF r^.class = Nconst THEN
											IF r^.conval^.intval >= 0 THEN OPG.WriteString("__ASHL(")
											ELSE OPG.WriteString("__ASHR(")
											END
										ELSIF ~SideEffects(r) THEN OPG.WriteString("__ASH(")
										ELSIF form = Int64 THEN OPG.WriteString("__ASHFL(")
										ELSE OPG.WriteString("__ASHF(")
										END
							|	msk:
										OPG.WriteString("__MASK(");
							|	bit:
										OPG.WriteString("__BIT(")
							|	lsh:
										IF r^.class = Nconst THEN
											IF r^.conval^.intval >= 0 THEN OPG.WriteString("__LSHL(")
											ELSE OPG.WriteString("__LSHR(")
											END
										ELSE OPG.WriteString("__LSH(")
										END
							|	rot:
										IF r^.class = Nconst THEN
											IF r^.conval^.intval >= 0 THEN OPG.WriteString("__ROTL(")
											ELSE OPG.WriteString("__ROTR(")
											END
										ELSE OPG.WriteString("__ROT(")
										END
							|	div:
										IF ~SideEffects(n) & (r^.class = Nconst) & (r^.conval^.intval > 0) THEN OPG.WriteString("__DIV(")
										ELSIF form = Int64 THEN OPG.WriteString("__DIVFL(")
										ELSE OPG.WriteString("__DIVF(")
										END
							|	mod:
										IF ~SideEffects(n) & (r^.class = Nconst) & (r^.conval^.intval > 0) THEN OPG.WriteString("__MOD(")
										ELSIF form = Int64 THEN OPG.WriteString("__MODFL(")
										ELSE OPG.WriteString("__MODF(")
										END
							|	min:
										IF ~SideEffects(n) THEN OPG.WriteString("__MIN(")
										ELSIF form = Real32 THEN OPG.WriteString("__MINFF(")
										ELSIF form = Real64 THEN OPG.WriteString("__MINFD(")
										ELSIF form = Int64 THEN OPG.WriteString("__MINFL(")
										ELSE OPG.WriteString("__MINF(")
										END
							|	max:
										IF ~SideEffects(n) THEN OPG.WriteString("__MAX(")
										ELSIF form = Real32 THEN OPG.WriteString("__MAXFF(")
										ELSIF form = Real64 THEN OPG.WriteString("__MAXFD(")
										ELSIF form = Int64 THEN OPG.WriteString("__MAXFL(")
										ELSE OPG.WriteString("__MAXF(")
										END
							END ;
							expr(l, MinPrec);
							OPG.WriteString(Comma);
							IF (subclass IN {ash, lsh, rot}) & (r^.class = Nconst) & (r^.conval^.intval < 0) THEN
								OPG.WriteInt(-r^.conval^.intval)
							ELSE expr(r, MinPrec)
							END ;
							IF subclass IN {lsh, rot} THEN OPG.WriteString(Comma); OPC.Ident(l^.typ^.strobj) END ;
							OPG.Write(CloseParen)
					| eql .. geq:
							IF l^.typ^.form IN {String8, String16, Comp} THEN
								OPG.WriteString("__STRCMP");
								IF (r.class = Nmop) & (r.subcl = conv) THEN	(* converted must be first *)
									StringModifier(r); StringModifier(l); OPG.Write("(");
									expr(r, MinPrec); OPG.WriteString(Comma); expr(l, MinPrec);
									IF subclass >= lss THEN subclass := SHORT((subclass - gtr) MOD 4 + lss) END
								ELSE
									StringModifier(l); StringModifier(r); OPG.Write("(");
									expr(l, MinPrec); OPG.WriteString(Comma); expr(r, MinPrec)
								END;
								OPG.Write(")"); OPC.Cmp(subclass); OPG.Write("0")
							ELSE
								expr(l, exprPrec); OPC.Cmp(subclass);
								typ := l^.typ;
								IF (typ^.form = Pointer) & (r^.typ.form # NilTyp) & (r^.typ # typ) & (r^.typ # OPT.sysptrtyp) THEN
									OPG.WriteString("(void *) ")
								END ;
								expr(r, exprPrec)
							END
					ELSE
						expr(l, exprPrec);
						CASE subclass OF
							times:
									IF form = Set THEN OPG.WriteString(" & ")
										ELSE OPG.WriteString(" * ")
									END
						|	slash:
									IF form = Set THEN OPG.WriteOrigString(" ^ ")
									ELSE OPG.WriteString(" / ");
										IF (r^.obj = NIL) OR (r^.obj^.typ^.form IN {Int8, Int16, Int32}) THEN
											OPG.Write(OpenParen); OPC.Ident(n^.typ^.strobj); OPG.Write(CloseParen)
										END
									END
						|	and:
									OPG.WriteString(" && ")
						|	plus:
									IF form IN {String8, String16} THEN OPM.err(265)
									ELSIF form = Set THEN OPG.WriteString(" | ")
									ELSE OPG.WriteString(" + ")
									END
						|	minus:
									IF form = Set THEN OPG.WriteString(" & ~")
									ELSE OPG.WriteString(" - ")
									END;
						|	or:
									OPG.WriteString(" || ")
						END;
						expr(r, exprPrec)
					END
		|	Ncall:
					IF (l^.obj # NIL) & (l^.obj^.mode = TProc) THEN
						IF l^.subcl = super THEN proc := SuperProc(n)
						ELSE OPG.WriteString("__"); proc := OPC.BaseTProc(l^.obj)
						END ;
						OPC.Ident(proc);
						n^.obj := proc^.link
					ELSIF l^.class = Nproc THEN design(l, 10)
					ELSE design(l, ProcTypeVar)
					END ;
					ActualPar(r, n^.obj)
		|	Ncomp:
					OPG.Write("("); compStat(l, TRUE); expr(r, MinPrec); OPG.Write(")")
		ELSE
					design(n, prec); (* not exprPrec! *)
		END ;
		IF (exprPrec <= prec) & (class IN {Nconst, Nupto, Nmop, Ndop, Ncall, Nguard}) THEN
			OPG.Write(CloseParen)
		END
	END expr;

	PROCEDURE^ stat(n: OPT.Node; outerProc: OPT.Object);

	PROCEDURE IfStat(n: OPT.Node; withtrap: BOOLEAN; outerProc: OPT.Object);
		VAR if: OPT.Node; obj: OPT.Object; typ: OPT.Struct; adr: INTEGER;
	BEGIN	(* n^.class IN {Nifelse, Nwith} *)
		if := n^.left; (* name := ""; *)
		WHILE if # NIL DO
			OPG.WriteString("if "); expr(if^.left, MaxPrec);	(* if *)
			OPG.Write(Blank); OPC.BegBlk;
			IF (n^.class = Nwith) & (if^.left^.left # NIL) THEN (* watch out for const expr *)
				obj := if^.left^.left^.obj; typ := obj^.typ; adr := obj^.adr;
				IF typ^.comp = Record THEN
					(* introduce alias pointer for var records; T1 *name__ = rec; *)
					OPC.BegStat; OPC.Ident(if^.left^.obj); OPG.WriteString(" *");
					OPG.WriteString(obj.name); OPG.WriteString("__ = (void*)");
					obj^.adr := 0; (* for nested WITH with same variable; always take the original name *)
					OPC.CompleteIdent(obj);
					OPC.EndStat
				END ;
				obj^.adr := 1;	(* signal special handling of variable name to OPC.CompleteIdent *)
				obj^.typ := if^.left^.obj^.typ;
				stat(if^.right, outerProc);
				obj^.typ := typ; obj^.adr := adr
			ELSE
				stat(if^.right, outerProc)
			END ;
			if := if^.link;
			IF (if # NIL) OR (n^.right # NIL) OR withtrap THEN OPC.EndBlk0(); OPG.WriteString(" else ");
			ELSE OPC.EndBlk()
			END
		END ;
		IF withtrap THEN OPG.WriteString(WithChk); OPC.EndStat()
		ELSIF n^.right # NIL THEN OPC.BegBlk; stat(n^.right, outerProc); OPC.EndBlk
		END
	END IfStat;

	PROCEDURE CaseStat(n: OPT.Node; outerProc: OPT.Object);
		VAR switchCase, label: OPT.Node;
			low, high: INTEGER; form, i: SHORTINT;
	BEGIN
		OPG.WriteString("switch "); expr(n^.left, MaxPrec);
		OPG.Write(Blank); OPC.BegBlk;
		form := n^.left^.typ^.form;
		switchCase := n^.right^.left;
		WHILE switchCase # NIL DO	(* switchCase^.class = Ncasedo *)
			label := switchCase^.left;
			i := 0;
			WHILE label # NIL DO (* label^.class = NConst *)
				low := label^.conval^.intval;
				high := label^.conval^.intval2;
				WHILE low <= high DO
					IF i = 0 THEN OPC.BegStat END ;
					OPC.Case(low, form);
					INC(low); INC(i);
					IF i = 5 THEN OPG.WriteLn; i := 0 END
				END ;
				label := label^.link
			END ;
			IF i > 0 THEN OPG.WriteLn END ;
			OPC.Indent(1);
			stat(switchCase^.right, outerProc);
			OPC.BegStat; OPG.WriteString(Break); OPC.EndStat;
			OPC.Indent(-1);
			switchCase := switchCase^.link
		END ;
		OPC.BegStat; OPG.WriteString("default: ");
		IF n^.right^.conval^.setval # {} THEN	(* else branch *)
			OPC.Indent(1); OPG.WriteLn; stat(n^.right^.right, outerProc);
			OPC.BegStat; OPG.WriteString(Break); OPC.Indent(-1)
		ELSE
			OPG.WriteString("__CASECHK")
		END ;
		OPC.EndStat; OPC.EndBlk
	END CaseStat;

	PROCEDURE ImplicitReturn(n: OPT.Node): BOOLEAN;
	BEGIN
		WHILE (n # NIL) & (n.class # Nreturn) DO n := n^.link END ;
		RETURN n = NIL
	END ImplicitReturn;

	PROCEDURE NewArr(d, x: OPT.Node);
		VAR typ, base: OPT.Struct; elem, n: INTEGER;
	BEGIN
		typ := d^.typ^.BaseTyp; base := typ.BaseTyp; elem := 1;
		design(d, MinPrec); OPG.WriteString(" = __NEWARR");
		IF typ.comp = Array THEN
			OPG.Write("0"); elem := typ.n
		ELSIF base.comp IN {Array, DynArr} THEN
			OPG.WriteInt(typ.n + 1);
			WHILE base^.comp = DynArr DO base := base^.BaseTyp END
		END;
		WHILE base^.comp = Array DO elem := elem * base.n; base := base^.BaseTyp END;
		OPG.Write("(");
		IF base.comp = Record THEN
			IF base.untagged THEN OPM.err(138) END;
			OPG.WriteString("(INTEGER)"); OPC.Andent(base); OPG.WriteString(DynTypExt)
		ELSE	(* basic type *)
			CASE base.form OF
			| Undef, Byte, Char8: n := 1
			| Int16: n := 2
			| Int8: n := 3
			| Int32: n := 4
			| Bool: n := 5
			| Set: n := 6
			| Real32: n := 7
			| Real64: n := 8
			| Char16: n := 9
			| Int64: n := 10
			| ProcTyp: n := 11
			| Pointer: IF base.untagged THEN n := 12 ELSE n := 0 END
			END;
			OPG.WriteInt(n)
		END;
		OPG.WriteString(", ");
		IF typ.comp = DynArr THEN
			IF base = typ.BaseTyp THEN	(* 1 dim dynamic *)
				expr(x, MinPrec)
			ELSE
				WHILE x # NIL DO expr(x, MinPrec); OPG.WriteString(", "); x := x.link END;
				OPG.WriteInt(elem)
			END
		ELSE
			OPG.WriteInt(elem)
		END;
		OPG.Write(")")
	END NewArr;

	PROCEDURE DefineTDescs(n: OPT.Node; close: BOOLEAN);
	BEGIN
		WHILE (n # NIL) & (n^.class = Ninittd) DO OPC.TDescDecl(n^.typ); n := n^.link END;
		OPC.ModDescDecl(close)
	END DefineTDescs;

	PROCEDURE AddCopy (left, right: OPT.Node; first: BOOLEAN);
	BEGIN
		IF first THEN OPG.WriteString("__STRCOPY") ELSE OPG.WriteString("__STRAPND") END;
		StringModifier(right); StringModifier(left); OPG.Write("(");
		expr(right, MinPrec); OPG.WriteString(Comma); expr(left, MinPrec); OPG.WriteString(Comma);
		IF left.typ.untagged THEN OPG.WriteString("-1") ELSE Len(left, 0, FALSE) END;
		OPG.Write(")")
	END AddCopy;

	PROCEDURE StringCopy (left, right: OPT.Node; exp: BOOLEAN);
	BEGIN
		ASSERT(right.class # Nconst);
		IF right.class = Ndop THEN
			ASSERT(right.subcl = plus);
			IF ~SameExp(left, right.left) THEN
				AddCopy(left, right.left, TRUE);
				IF exp THEN OPG.WriteString(", ") ELSE OPC.EndStat; OPC.BegStat END
			END;
			right := right.right;
			WHILE right.class = Ndop DO
				ASSERT(right.subcl = plus);
				AddCopy(left, right.left, FALSE);
				IF exp THEN OPG.WriteString(", ") ELSE OPC.EndStat; OPC.BegStat END;
				right := right.right
			END;
			AddCopy(left, right, FALSE)
		ELSE
			AddCopy(left, right, TRUE)
		END
	END StringCopy;

	PROCEDURE compStat (n: OPT.Node; exp: BOOLEAN);
		VAR l, r: OPT.Node;
	BEGIN
		WHILE (n # NIL) & OPM.noerr DO
			IF ~exp THEN OPC.BegStat END;
			ASSERT(n.class = Nassign);
			l := n^.left; r := n^.right;
			IF n^.subcl = assign THEN
				IF r.typ.form IN {String8, String16} THEN
					IF r.class # Nconst THEN
						StringCopy(l, r, exp)
					ELSE
						OPG.WriteString(MoveFunc);
						expr(r, MinPrec); OPG.WriteString(Comma); expr(l, MinPrec); OPG.WriteString(Comma);
						OPG.WriteInt(r^.conval^.intval2 * r.typ.BaseTyp.size); OPG.Write(CloseParen)
					END
				ELSE ASSERT(l^.typ^.form IN {Pointer, Char8, Char16});
					design(l, MinPrec); OPG.WriteString(" = "); expr(r, MinPrec)
				END
			ELSE ASSERT(n.subcl = newfn);
				ASSERT(l.typ.BaseTyp.comp = DynArr);
				NewArr(l, r)
			END;
			IF exp THEN OPG.WriteString(", "); OPG.WriteLn; OPC.BegStat; OPG.Write(9X) ELSE OPC.EndStat END;
			n := n^.link
		END
	END compStat;

	PROCEDURE stat(n: OPT.Node; outerProc: OPT.Object);
		VAR proc: OPT.Object; saved: ExitInfo; l, r: OPT.Node;
	BEGIN
		WHILE (n # NIL) & OPM.noerr DO
			OPM.errpos := n^.conval^.intval;
			IF ~(n^.class IN {Ninittd, Ncomp}) THEN OPC.BegStat; END;
			CASE n^.class OF
				Nenter:
						IF n^.obj = NIL THEN (* enter module *)
							HALT(100);
						ELSE (* enter proc *)
							proc := n^.obj;
							OPC.TypeDefs(proc^.scope^.right, 0);
							IF ~proc^.scope^.leaf THEN OPC.DefineInter (proc) END ; (* define intermediate procedure scope *)
							INC(OPG.level); stat(n^.left, proc); DEC(OPG.level);
							OPC.EnterProc(proc); stat(n^.right, proc);
							OPC.ExitProc(proc, TRUE, ImplicitReturn(n^.right));
						END
			|	Ninittd: (* done in enter module *)
			|	Nassign:
					CASE n^.subcl OF
						assign:
								l := n^.left; r := n^.right;
								IF l^.typ^.comp IN {Array, DynArr} THEN
									IF (r.typ.form IN {String8, String16}) & (r.class # Nconst) THEN
										StringCopy(l, r, FALSE)
									ELSE
										OPG.WriteString(MoveFunc);
										expr(r, MinPrec); OPG.WriteString(Comma); expr(l, MinPrec); OPG.WriteString(Comma);
										IF r^.typ = OPT.string8typ THEN OPG.WriteInt(r^.conval^.intval2)
										ELSIF r^.typ = OPT.string16typ THEN OPG.WriteInt(r^.conval^.intval2 * OPT.char16typ.size)
										ELSE OPG.WriteInt(OPC.Size(r^.typ))
										END ;
										OPG.Write(CloseParen)
									END
								ELSE
									IF (l^.typ^.form = Pointer) & (l^.obj # NIL) & (l^.obj^.adr = 1) & (l^.obj^.mode = Var) THEN
										l^.obj^.adr := 0; design(l, MinPrec); l^.obj^.adr := 1;			(* avoid cast of WITH-variable *)
										IF r^.typ^.form # NilTyp THEN OPG.WriteString(" = (void*)")
										ELSE OPG.WriteString(" = ")
										END
									ELSE
										design(l, MinPrec); OPG.WriteString(" = ")
									END ;
									IF l^.typ = r^.typ THEN expr(r, MinPrec)
									ELSIF (l^.typ^.form IN {Pointer, ProcTyp}) & (r^.typ^.form # NilTyp)
											& (l^.typ^.strobj # NIL) & (l.typ.strobj.name^ # "") THEN
										OPG.Write("("); OPC.Ident(l^.typ^.strobj); OPG.Write(")"); expr(r, MinPrec)
									ELSIF l^.typ^.comp = Record THEN
										OPG.WriteString("*("); OPC.Andent(l^.typ); OPG.WriteString("*)"); Adr(r, 9)
(*
									ELSIF (r.typ.form = Int64) & ((l.typ.form <= Int32) OR (l.typ.form = Char16))
											OR (r.typ.form = Real64) & (l.typ.form = Real32) THEN
										OPG.Write("("); OPC.Ident(l.typ.strobj); OPG.Write(")"); expr(r, MinPrec)
*)
									ELSE expr(r, MinPrec)
									END
								END
					|	newfn:
								IF n^.left^.typ^.BaseTyp^.comp = Record THEN
									design(n^.left, MinPrec); OPG.WriteString(" = "); OPG.WriteString("__NEW(");
									OPC.Andent(n^.left^.typ^.BaseTyp); OPG.WriteString("__typ)")
								ELSIF n^.left^.typ^.BaseTyp^.comp IN {Array, DynArr} THEN
									NewArr(n^.left, n^.right)
								END
					|	incfn, decfn:
								expr(n^.left, MinPrec); OPC.Increment(n^.subcl = decfn); expr(n^.right, MinPrec)
					|	inclfn, exclfn:
								expr(n^.left, MinPrec); OPC.SetInclude(n^.subcl = exclfn);
								OPG.WriteString(SetOfFunc); expr(n^.right, MinPrec);
								OPG.Write(CloseParen)
					|	copyfn:
								HALT(100);
								OPG.WriteString(CopyFunc);
								expr(n^.right, MinPrec); OPG.WriteString(Comma); expr(n^.left, MinPrec); OPG.WriteString(Comma);
								Len(n^.left, 0, FALSE); OPG.Write(CloseParen)
					|	(*SYSTEM*)movefn:
								OPG.WriteString(MoveFunc);
								expr(n^.right, MinPrec); OPG.WriteString(Comma); expr(n^.left, MinPrec); OPG.WriteString(Comma);
								expr(n^.right^.link, MinPrec);
								OPG.Write(CloseParen)
					|	(*SYSTEM*)getfn:
								OPG.WriteString(GetFunc); expr(n^.right, MinPrec); OPG.WriteString(Comma); expr(n^.left, MinPrec);
								OPG.WriteString(Comma);
								IF n.left.typ.strobj # NIL THEN OPC.Ident(n^.left^.typ^.strobj)
								ELSIF n.left.typ.form IN {Pointer, ProcTyp} THEN OPG.WriteString("void*")
								ELSE OPM.err(217)
								END;
								OPG.Write(CloseParen)
					|	(*SYSTEM*)putfn:
								OPG.WriteString(PutFunc); expr(n^.left, MinPrec); OPG.WriteString(Comma); expr(n^.right, MinPrec);
								OPG.WriteString(Comma);
								IF n.right.typ.strobj # NIL THEN OPC.Ident(n^.right^.typ^.strobj)
								ELSIF n.right.typ.form IN {Pointer, ProcTyp} THEN OPG.WriteString("void*")
								ELSE OPM.err(217)
								END;
								OPG.Write(CloseParen)
					|	(*SYSTEM*)getrfn, putrfn: OPM.err(217)
					END
			|	Ncall:
						IF (n^.left^.obj # NIL) & (n^.left^.obj^.mode = TProc) THEN
							IF n^.left^.subcl = super THEN proc := SuperProc(n)
							ELSE OPG.WriteString("__"); proc := OPC.BaseTProc(n^.left^.obj)
							END ;
							OPC.Ident(proc);
							n^.obj := proc^.link
						ELSIF n^.left^.class = Nproc THEN design(n^.left, 10)
						ELSE design(n^.left, ProcTypeVar)
						END ;
						ActualPar(n^.right, n^.obj)
			|	Nifelse:
						IF n^.subcl # assertfn THEN IfStat(n, FALSE, outerProc)
						ELSIF assert THEN OPG.WriteString("__ASSERT("); expr(n^.left^.left^.left, MinPrec); OPG.WriteString(Comma);
							OPG.WriteInt(n^.left^.right^.right^.conval^.intval); OPG.Write(CloseParen); OPC.EndStat
						END
			|	Ncase:
						INC(exit.level); CaseStat(n, outerProc); DEC(exit.level)
			|	Nwhile:
						INC(exit.level); OPG.WriteString("while "); expr(n^.left, MaxPrec);
						OPG.Write(Blank); OPC.BegBlk; stat(n^.right, outerProc); OPC.EndBlk;
						DEC(exit.level)
			|	Nrepeat:
						INC(exit.level); OPG.WriteString("do "); OPC.BegBlk; stat(n^.left, outerProc); OPC.EndBlk0;
						OPG.WriteString(" while (!");  expr(n^.right, 9); OPG.Write(CloseParen);
						DEC(exit.level)
			|	Nloop:
						saved := exit; exit.level := 0; exit.label := -1;
						OPG.WriteString("for (;;) "); OPC.BegBlk; stat(n^.left, outerProc); OPC.EndBlk;
						IF exit.label # -1 THEN
							OPC.BegStat; OPG.WriteString("exit__"); OPG.WriteInt(exit.label); OPG.Write(":"); OPC.EndStat
						END ;
						exit := saved
			|	Nexit:
						IF exit.level = 0 THEN OPG.WriteString(Break)
						ELSE
							IF exit.label = -1 THEN exit.label := nofExitLabels; INC(nofExitLabels) END ;
							OPG.WriteString("goto exit__"); OPG.WriteInt(exit.label)
						END
			|	Nreturn:
						IF OPG.level = 0 THEN
							IF mainprog THEN OPG.WriteString("__FINI") ELSE OPG.WriteString("__ENDMOD") END
						ELSE
							OPC.ExitProc(outerProc, FALSE, FALSE);
							OPG.WriteString("return");
							IF n^.left # NIL THEN OPG.Write(Blank);
								IF (n^.left^.typ^.form = Pointer) & (n^.obj^.typ # n^.left^.typ) THEN
									OPG.WriteString("(void*)"); expr(n^.left, 10)
								ELSE
									expr(n^.left, MinPrec)
								END
							END
						END
			|	Nwith:
						IfStat(n, n^.subcl = 0, outerProc)
			|	Ntrap:
						OPC.Halt(n^.right^.conval^.intval)
			|	Ncomp:
						compStat(n.left, FALSE); stat(n.right, outerProc)
			END ;
			IF ~(n^.class IN {Nenter, Ninittd, Nifelse, Nwith, Ncase, Nwhile, Nloop, Ncomp}) THEN OPC.EndStat END ;
			n := n^.link
		END
	END stat;

	PROCEDURE Module*(prog: OPT.Node);
	BEGIN
		OPG.level := 0;
		IF ~mainprog THEN OPC.GenHdr(prog^.right); OPC.GenHdrIncludes END ;
		OPC.GenBdy(prog^.right, prog.link # NIL);
		ASSERT((prog.class = Nenter) & (prog.obj = NIL));
		INC(OPG.level); stat(prog^.left, NIL); DEC(OPG.level);
		DefineTDescs(prog^.right, prog.link # NIL);
		OPC.RegEntry(prog^.right);
		OPC.EnterBody; stat(prog^.right, NIL); OPC.ExitBody;
		IF prog.link # NIL THEN	(* close section *)
			OPC.EnterClose; stat(prog^.link, NIL); OPC.ExitClose
		END;
		IF mainprog THEN OPC.MainBody END;
		OPC.CleanupArrays
	END Module;

END OfrontCPV.
