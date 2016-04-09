MODULE OfrontOPP;	(* NW, RC 6.3.89 / 10.2.94 *)	(* object model 4.12.93 *)

	IMPORT
		OPB := OfrontOPB, OPT := OfrontOPT, OPS := OfrontOPS, MC := MultiCardinals, OPM := OfrontOPM;

	CONST
		(* numtyp values *)
		char = 1; integer = 2; real = 3; longreal = 4;

		(* symbol values *)
		null = 0; times = 1; slash = 2; div = 3; mod = 4;
		and = 5; plus = 6; minus = 7; or = 8; eql = 9;
		neq = 10; lss = 11; leq = 12; gtr = 13; geq = 14;
		in = 15; is = 16; arrow = 17; period = 18; comma = 19;
		colon = 20; upto = 21; rparen = 22; rbrak = 23; rbrace = 24;
		of = 25; then = 26; do = 27; to = 28; by = 29;
		lparen = 30; lbrak = 31; lbrace = 32; not = 33; becomes = 34;
		number = 35; nil = 36; string = 37; ident = 38; semicolon = 39;
		bar = 40; end = 41; else = 42; elsif = 43; until = 44;
		if = 45; case = 46; while = 47; repeat = 48; for = 49;
		loop = 50; with = 51; exit = 52; return = 53; array = 54;
		record = 55; pointer = 56; begin = 57; const = 58; type = 59;
		var = 60; procedure = 61; import = 62; module = 63; eof = 64;
		out = 71;
		unsgn = 40; (* псевдооперация unsigned для div *)

		(* object modes *)
		Var = 1; VarPar = 2; Con = 3; Fld = 4; Typ = 5; LProc = 6; XProc = 7;
		SProc = 8; CProc = 9; IProc = 10; Mod = 11; Head = 12; TProc = 13;

		(* Structure forms *)
		Undef = 0; Byte = 1; Bool = 2; Char = 3; SInt = 4; Int = 5; LInt = 6;
		Real = 7; LReal = 8; Set = 9; String = 10; NilTyp = 11; NoTyp = 12;
		Pointer = 13; ProcTyp = 14; Comp = 15;
		intSet = {Byte, SInt..LInt};

		(* composite structure forms *)
		Basic = 1; Array = 2; DynArr = 3; Record = 4;

		(*function number*)
		haltfn = 0; newfn = 1; incfn = 13; decfn = 14; sysnewfn = 30;

		(* nodes classes *)
		Nvar = 0; Nvarpar = 1; Nfield = 2; Nderef = 3; Nindex = 4; Nguard = 5; Neguard = 6;
		Nconst = 7; Ntype = 8; Nproc = 9; Nupto = 10; Nmop = 11; Ndop = 12; Ncall = 13;
		Ninittd = 14; Nif = 15; Ncaselse = 16; Ncasedo = 17; Nenter = 18; Nassign = 19;
		Nifelse = 20; Ncase = 21; Nwhile = 22; Nrepeat = 23; Nloop = 24; Nexit = 25;
		Nreturn = 26; Nwith = 27; Ntrap = 28;

		(* node subclasses *)
		super = 1;

		(* module visibility of objects *)
		internal = 0; external = 1; externalR = 2; inPar = 3; outPar = 4;

		(* procedure flags (conval^.setval) *)
		hasBody = 1; isRedef = 2; slNeeded = 3;

		(* sysflags *)
		nilBit = 1;

	TYPE
		CaseTable = ARRAY OPM.MaxCases OF
			RECORD
				low, high: INTEGER
			END ;

	VAR
		sym, level: BYTE;
		LoopLevel: SHORTINT;
		TDinit, lastTDinit: OPT.Node;
		nofFwdPtr: SHORTINT;
		FwdPtr: ARRAY 64 OF OPT.Struct;

	PROCEDURE^ Type(VAR typ, banned: OPT.Struct);
	PROCEDURE^ Expression(VAR x: OPT.Node);
	PROCEDURE^ Block(VAR procdec, statseq: OPT.Node);

	PROCEDURE Sign(n: INTEGER): INTEGER;
	BEGIN
		IF n = 0 THEN RETURN 0 END;
		IF n > 0 THEN RETURN 1 END;
		RETURN -1
	END Sign;

	PROCEDURE err(n: SHORTINT);
	BEGIN OPM.err(n)
	END err;

	PROCEDURE CheckSym(s: SHORTINT);
	BEGIN
		IF sym = s THEN OPS.Get(sym) ELSE OPM.err(s) END
	END CheckSym;

	PROCEDURE qualident(VAR id: OPT.Object);
		VAR obj: OPT.Object; lev: BYTE;
	BEGIN (*sym = ident*)
		OPT.Find(obj); OPS.Get(sym);
		IF (sym = period) & (obj # NIL) & (obj^.mode = Mod) THEN
			OPS.Get(sym);
			IF sym = ident THEN
				OPT.FindImport(obj, obj); OPS.Get(sym)
			ELSE err(ident); obj := NIL
			END
		END ;
		IF obj = NIL THEN err(0);
			obj := OPT.NewObj(); obj^.mode := Var; obj^.typ := OPT.undftyp; obj^.adr := 0
		ELSE lev := obj^.mnolev;
			IF (obj^.mode IN {Var, VarPar}) & (lev # level) THEN
				obj^.leaf := FALSE;
				IF lev > 0 THEN OPB.StaticLink(SHORT(SHORT(level-lev))) END
			END
		END ;
		id := obj
	END qualident;

	PROCEDURE ConstExpression(VAR x: OPT.Node);
	BEGIN Expression(x);
		IF x^.class # Nconst THEN
			err(50); x := OPB.NewIntConst(1)
		END
	END ConstExpression;

	PROCEDURE CheckMark(VAR vis: BYTE);
	BEGIN OPS.Get(sym);
		IF (sym = times) OR (sym = minus) THEN
			IF level > 0 THEN err(47) END ;
			IF sym = times THEN vis := external ELSE vis := externalR END ;
			OPS.Get(sym)
		ELSE vis := internal
		END
	END CheckMark;

	PROCEDURE CheckSysFlag(VAR sysflag: SHORTINT; default: SHORTINT);
		VAR x: OPT.Node; sf: INTEGER;
	BEGIN
		IF sym = lbrak THEN OPS.Get(sym); IF ~OPT.SYSimported THEN err(135) END;
			ConstExpression(x);
			IF x^.typ^.form IN intSet THEN sf := x^.conval^.intval;
				IF (sf < 0) OR (sf > OPM.MaxSysFlag) THEN err(220); sf := 0 END
			ELSE err(51); sf := 0
			END ;
			sysflag := SHORT(sf); CheckSym(rbrak)
		ELSE sysflag := default
		END
	END CheckSysFlag;

	PROCEDURE CheckSysFlagVarPar(VAR sysflag: SHORTINT);
	BEGIN
		IF sym = lbrak THEN OPS.Get(sym);
			IF ~OPT.SYSimported THEN err(135) END;
			IF (sym = ident) & (OPS.name = "nil") THEN OPS.Get(sym); sysflag := nilBit
			ELSE OPS.Get(sym); err(220); sysflag := 0
			END ;
			CheckSym(rbrak)
		ELSE sysflag := 0
		END
	END CheckSysFlagVarPar;

	PROCEDURE RecordType(VAR typ, banned: OPT.Struct);
		VAR fld, first, last, base: OPT.Object;
			ftyp: OPT.Struct; sysflag: SHORTINT;
	BEGIN typ := OPT.NewStr(Comp, Record); typ^.BaseTyp := NIL;
		CheckSysFlag(sysflag, -1);
		IF sym = lparen THEN
			OPS.Get(sym); (*record extension*)
			IF sym = ident THEN
				qualident(base);
				IF (base^.mode = Typ) & (base^.typ^.comp = Record) THEN
					IF base^.typ = banned THEN err(58)
					ELSE base^.typ^.pvused := TRUE;
						typ^.BaseTyp := base^.typ; typ^.extlev := SHORT(SHORT(base^.typ^.extlev + 1)); typ^.sysflag := base^.typ^.sysflag
					END
				ELSE err(52)
				END
			ELSE err(ident)
			END ;
			CheckSym(rparen)
		END ;
		IF sysflag >= 0 THEN typ^.sysflag := sysflag END ;
		OPT.OpenScope(0, NIL); first := NIL; last := NIL;
		LOOP
			IF sym = ident THEN
				LOOP
					IF sym = ident THEN
						IF typ^.BaseTyp # NIL THEN
							OPT.FindField(OPS.name, typ^.BaseTyp, fld);
							IF fld # NIL THEN err(1) END
						END ;
						OPT.Insert(OPS.name, fld); CheckMark(fld^.vis);
						fld^.mode := Fld; fld^.link := NIL; fld^.typ := OPT.undftyp;
						IF first = NIL THEN first := fld END ;
						IF last = NIL THEN typ^.link := fld ELSE last^.link := fld END ;
						last := fld
					ELSE err(ident)
					END ;
					IF sym = comma THEN OPS.Get(sym)
					ELSIF sym = ident THEN err(comma)
					ELSE EXIT
					END
				END ;
				CheckSym(colon); Type(ftyp, banned);
				ftyp^.pvused := TRUE;
				IF ftyp^.comp = DynArr THEN ftyp := OPT.undftyp; err(88) END ;
				WHILE first # NIL DO
					first^.typ := ftyp; first := first^.link
				END
			END ;
			IF sym = semicolon THEN OPS.Get(sym)
			ELSIF sym = ident THEN err(semicolon)
			ELSE EXIT
			END
		END ;
		OPT.CloseScope
	END RecordType;

	PROCEDURE ArrayType(VAR typ, banned: OPT.Struct);
		VAR x: OPT.Node; n: INTEGER; sysflag: SHORTINT;
	BEGIN CheckSysFlag(sysflag, 0);
		IF sym = of THEN	(*dynamic array*)
			typ := OPT.NewStr(Comp, DynArr); typ^.mno := 0; typ^.sysflag := sysflag;
			OPS.Get(sym); Type(typ^.BaseTyp, banned);
			typ^.BaseTyp^.pvused := TRUE;
			IF typ^.BaseTyp^.comp = DynArr THEN typ^.n := typ^.BaseTyp^.n + 1
			ELSE typ^.n := 0
			END
		ELSE
			typ := OPT.NewStr(Comp, Array); typ^.sysflag := sysflag; ConstExpression(x);
			IF x^.typ^.form IN intSet THEN n := x^.conval^.intval;
				IF (n <= 0) OR (n > OPM.MaxIndex) THEN err(63); n := 1 END
			ELSE err(51); n := 1
			END ;
			typ^.n := n;
			IF sym = of THEN
				OPS.Get(sym); Type(typ^.BaseTyp, banned);
				typ^.BaseTyp^.pvused := TRUE
			ELSIF sym = comma THEN
				OPS.Get(sym); IF sym # of THEN ArrayType(typ^.BaseTyp, banned) END
			ELSE err(35)
			END ;
			IF typ^.BaseTyp^.comp = DynArr THEN typ^.BaseTyp := OPT.undftyp; err(88) END
		END
	END ArrayType;

	PROCEDURE PointerType(VAR typ: OPT.Struct);
		VAR id: OPT.Object;
	BEGIN typ := OPT.NewStr(Pointer, Basic); CheckSysFlag(typ^.sysflag, 0);
		CheckSym(to);
		IF sym = ident THEN OPT.Find(id);
			IF id = NIL THEN
				IF nofFwdPtr < LEN(FwdPtr) THEN FwdPtr[nofFwdPtr] := typ; INC(nofFwdPtr)
				ELSE err(224)
				END ;
				typ^.link := OPT.NewObj(); typ^.link^.name := OPS.name$;
				typ^.BaseTyp := OPT.undftyp; OPS.Get(sym) (*forward ref*)
			ELSE qualident(id);
				IF id^.mode = Typ THEN
					IF id^.typ^.comp IN {Array, DynArr, Record} THEN
						typ^.BaseTyp := id^.typ
					ELSE typ^.BaseTyp := OPT.undftyp; err(57)
					END
				ELSE typ^.BaseTyp := OPT.undftyp; err(52)
				END
			END
		ELSE Type(typ^.BaseTyp, OPT.notyp);
			IF ~(typ^.BaseTyp^.comp IN {Array, DynArr, Record}) THEN
				typ^.BaseTyp := OPT.undftyp; err(57)
			END
		END
	END PointerType;

	PROCEDURE FormalParameters(VAR firstPar: OPT.Object; VAR resTyp: OPT.Struct);
		VAR mode, vis: BYTE; sys: SHORTINT;
				par, first, last, res: OPT.Object; typ: OPT.Struct;
	BEGIN first := NIL; last := firstPar;
		IF (sym = ident) OR (sym = var) OR (sym = in) THEN
			LOOP
				sys := 0; vis := 0;
				IF sym = var THEN OPS.Get(sym); mode := VarPar
				ELSIF sym = in THEN OPS.Get(sym); mode := VarPar; vis := inPar
				ELSIF sym = out THEN OPS.Get(sym); mode := VarPar; vis := outPar
				ELSE mode := Var
				END ;
				IF mode = VarPar THEN CheckSysFlagVarPar(sys) END;
				LOOP
					IF sym = ident THEN
						OPT.Insert(OPS.name, par); OPS.Get(sym);
						par^.mode := mode; par^.link := NIL; par^.vis := vis; par^.sysflag := SHORT(sys);
						IF first = NIL THEN first := par END ;
						IF firstPar = NIL THEN firstPar := par ELSE last^.link := par END ;
						last := par
					ELSE err(ident)
					END ;
					IF sym = comma THEN OPS.Get(sym)
					ELSIF sym = ident THEN err(comma)
					ELSIF sym = var THEN err(comma); OPS.Get(sym)
					ELSE EXIT
					END
				END ;
				CheckSym(colon); Type(typ, OPT.notyp);
				IF mode = Var THEN typ^.pvused := TRUE END ;
				IF (mode = VarPar) & (vis = inPar) & (typ^.form # Undef) & (typ^.form # Comp) & (typ^.sysflag = 0) THEN
					err(177)
				END;
				(* typ^.pbused is set when parameter type name is parsed *)
				WHILE first # NIL DO first^.typ := typ; first := first^.link END ;
				IF sym = semicolon THEN OPS.Get(sym)
				ELSIF sym = ident THEN err(semicolon)
				ELSE EXIT
				END
			END
		END ;
		CheckSym(rparen);
		IF sym = colon THEN
			OPS.Get(sym); resTyp := OPT.undftyp;
			IF sym = ident THEN qualident(res);
				IF res^.mode = Typ THEN
					IF res^.typ^.form < Comp THEN resTyp := res^.typ
					ELSE err(54)
					END
				ELSE err(52)
				END
			ELSE err(ident)
			END
		ELSE resTyp := OPT.notyp
		END
	END FormalParameters;

	PROCEDURE TypeDecl(VAR typ, banned: OPT.Struct);
		VAR id: OPT.Object;
	BEGIN typ := OPT.undftyp;
		IF sym < lparen THEN err(12);
			REPEAT OPS.Get(sym) UNTIL sym >= lparen
		END ;
		IF sym = ident THEN qualident(id);
			IF id^.mode = Typ THEN
				IF id^.typ # banned THEN typ := id^.typ ELSE err(58) END
			ELSE err(52)
			END
		ELSIF sym = array THEN
			OPS.Get(sym); ArrayType(typ, banned)
		ELSIF sym = record THEN
			OPS.Get(sym); RecordType(typ, banned);
			OPB.Inittd(TDinit, lastTDinit, typ); CheckSym(end)
		ELSIF sym = pointer THEN
			OPS.Get(sym); PointerType(typ)
		ELSIF sym = procedure THEN
			OPS.Get(sym); typ := OPT.NewStr(ProcTyp, Basic); CheckSysFlag(typ^.sysflag, 0);
			IF sym = lparen THEN
				OPS.Get(sym); OPT.OpenScope(level, NIL);
				FormalParameters(typ^.link, typ^.BaseTyp); OPT.CloseScope
			ELSE typ^.BaseTyp := OPT.notyp; typ^.link := NIL
			END
		ELSE err(12)
		END ;
		LOOP
			IF (sym >= semicolon) & (sym <= else) OR (sym = rparen) OR (sym = eof) THEN EXIT END;
			err(15); IF sym = ident THEN EXIT END;
			OPS.Get(sym)
		END
	END TypeDecl;

	PROCEDURE Type(VAR typ, banned: OPT.Struct);
	BEGIN TypeDecl(typ, banned);
		IF (typ^.form = Pointer) & (typ^.BaseTyp = OPT.undftyp) & (typ^.strobj = NIL) THEN err(0) END
	END Type;

	PROCEDURE selector(VAR x: OPT.Node);
		VAR obj, proc: OPT.Object; y: OPT.Node; typ: OPT.Struct; name: OPS.Name;
	BEGIN
		LOOP
			IF sym = lbrak THEN OPS.Get(sym);
				LOOP
					IF (x^.typ # NIL) & (x^.typ^.form = Pointer) THEN OPB.DeRef(x) END ;
					Expression(y); OPB.Index(x, y);
					IF sym = comma THEN OPS.Get(sym) ELSE EXIT END
				END ;
				CheckSym(rbrak)
			ELSIF sym = period THEN OPS.Get(sym);
				IF sym = ident THEN name := OPS.name; OPS.Get(sym);
					IF x^.typ # NIL THEN
						IF x^.typ^.form = Pointer THEN OPB.DeRef(x) END ;
						IF x^.typ^.comp = Record THEN
							OPT.FindField(name, x^.typ, obj); OPB.Field(x, obj);
							IF (obj # NIL) & (obj^.mode = TProc) THEN
								IF sym = arrow THEN  (* super call *) OPS.Get(sym);
									y := x^.left;
									IF y^.class = Nderef THEN y := y^.left END ;	(* y = record variable *)
									IF y^.obj # NIL THEN
										proc := OPT.topScope;	(* find innermost scope which owner is a TProc *)
										WHILE (proc^.link # NIL) & (proc^.link^.mode # TProc) DO proc := proc^.left END ;
										IF (proc^.link = NIL) OR (proc^.link^.link # y^.obj) THEN err(75) END ;
										typ := y^.obj^.typ;
										IF typ^.form = Pointer THEN typ := typ^.BaseTyp END ;
										OPT.FindField(x^.obj^.name, typ^.BaseTyp, proc);
										IF proc # NIL THEN x^.subcl := super ELSE err(74) END
									ELSE err(75)
									END
								ELSIF (x^.left^.readonly) & (obj^.link^.mode = VarPar) & (obj^.link^.vis = 0) THEN err(76)
								END ;
								IF (obj^.typ # OPT.notyp) & (sym # lparen) THEN err(lparen) END
							END
						ELSE err(53)
						END
					ELSE err(52)
					END
				ELSE err(ident)
				END
			ELSIF sym = arrow THEN OPS.Get(sym); OPB.DeRef(x)
			ELSIF (sym = lparen) & (x^.class < Nconst) & (x^.typ^.form # ProcTyp) &
					((x^.obj = NIL) OR (x^.obj^.mode # TProc)) THEN
				OPS.Get(sym);
				IF sym = ident THEN
					qualident(obj);
					IF obj^.mode = Typ THEN OPB.TypTest(x, obj, TRUE)
					ELSE err(52)
					END
				ELSE err(ident)
				END ;
				CheckSym(rparen)
			ELSE EXIT
			END
		END
	END selector;

	PROCEDURE ActualParameters(VAR aparlist: OPT.Node; fpar: OPT.Object);
		VAR apar, last: OPT.Node;
	BEGIN aparlist := NIL; last := NIL;
		IF sym # rparen THEN
			LOOP Expression(apar);
				IF fpar # NIL THEN
					OPB.Param(apar, fpar); OPB.Link(aparlist, last, apar);
					fpar := fpar^.link;
				ELSE err(64)
				END ;
				IF sym = comma THEN OPS.Get(sym)
				ELSIF (lparen <= sym) & (sym <= ident) THEN err(comma)
				ELSE EXIT
				END
			END
		END ;
		IF fpar # NIL THEN err(65) END
	END ActualParameters;

	PROCEDURE StandProcCall(VAR x: OPT.Node);
		VAR y: OPT.Node; m: BYTE; n: SHORTINT;
	BEGIN m := SHORT(SHORT(x^.obj^.adr)); n := 0;
		IF sym = lparen THEN OPS.Get(sym);
			IF sym # rparen THEN
				LOOP
					IF n = 0 THEN Expression(x); OPB.StPar0(x, m); n := 1
					ELSIF n = 1 THEN Expression(y); OPB.StPar1(x, y, m); n := 2
					ELSE Expression(y); OPB.StParN(x, y, m, n); INC(n)
					END ;
					IF sym = comma THEN OPS.Get(sym)
					ELSIF (lparen <= sym) & (sym <= ident) THEN err(comma)
					ELSE EXIT
					END
				END ;
				CheckSym(rparen)
			ELSE OPS.Get(sym)
			END ;
			OPB.StFct(x, m, n)
		ELSE err(lparen)
		END ;
		IF (level > 0) & ((m = newfn) OR (m = sysnewfn)) THEN OPT.topScope^.link^.leaf := FALSE END
	END StandProcCall;

	PROCEDURE Element(VAR x: OPT.Node);
		VAR y: OPT.Node;
	BEGIN Expression(x);
		IF sym = upto THEN
			OPS.Get(sym); Expression(y); OPB.SetRange(x, y)
		ELSE OPB.SetElem(x)
		END
	END Element;

	PROCEDURE Sets(VAR x: OPT.Node);
		VAR y: OPT.Node;
	BEGIN
		IF sym # rbrace THEN
			Element(x);
			LOOP
				IF sym = comma THEN OPS.Get(sym)
				ELSIF (lparen <= sym) & (sym <= ident) THEN err(comma)
				ELSE EXIT
				END ;
				Element(y); OPB.Op(plus, x, y)
			END
		ELSE x := OPB.EmptySet()
		END ;
		CheckSym(rbrace)
	END Sets;


	PROCEDURE String2Chars ( a :OPT.Node; VAR x: OPT.Node);
	(* переводит строку в массив символов *)
	VAR i:INTEGER;
	BEGIN
		FOR i := 0 TO a^.conval^.intval2-1 DO
			x^.conval^.arr(OPT.ConstArrOfByte).val[x^.conval^.intval+i] := SHORT(ORD(a^.conval^.ext^[i]));
		END;
	END String2Chars;

	PROCEDURE ConstArray (VAR x: OPT.Node; typ: OPT.Struct);
	(* конструкция "константный массив". typ - текущий уровень массива *)
	VAR apar:  OPT.Node; n, i: INTEGER; fp: OPT.Object;  y: OPT.ConstArr;
	BEGIN
		IF (sym # lparen) & (typ^.BaseTyp^.form IN {Char,Byte} )  THEN (* это должна быть строка*)
			ConstExpression(apar); (* попытаемся взять эту строку *)
			IF  (apar^.typ^.comp=1)&(apar^.typ^.form= String) &
				(apar^.conval^.intval2 <= typ^.n) THEN (* взяли строку допустимой длины *)
				String2Chars(apar, x);
				INC(x^.conval^.intval,  typ^.n);
			ELSIF  (apar^.typ^.comp=1)&(apar^.typ^.form  IN {Char,Byte}) THEN
				(* прочитался символ *)
					x^.conval^.arr(OPT.ConstArrOfByte).val[x^.conval^.intval] :=  SHORT(SHORT(apar^.conval^.intval));
					INC(x^.conval^.intval,  typ^.n);
			ELSE
				err(63);
			END;
		ELSE
		CheckSym(lparen);
		n:=typ^.n; (* количество элементов массива *)
		i := 0;
		IF  typ^.BaseTyp^.form IN intSet +{Bool,Char} THEN (* массив из целых (в т.ч. BYTE)  *)
			y := x^.conval^.arr;
			fp := OPT.NewObj(); fp^.typ :=  typ^.BaseTyp;
			fp^.mode := Var;  (*  fp  - переменная, элемент массива *)
			IF sym # rparen THEN
				LOOP  ConstExpression(apar); (* берем очередной элемент *)
					IF i < n THEN
						IF (i=0) & (typ^.BaseTyp^.form IN {Char,Byte} )   (* 'считывали первый символ *)
							& (apar^.typ^.comp=1) & (apar^.typ^.form= String)  (* а прочитали строку *)
							& (apar^.conval^.intval2 <= typ^.n)  (* допустимой длины *) THEN
							String2Chars(apar, x);
						ELSE
							OPB.Param(apar, fp); (* проверим на совместимость*)
							(* тут нужно положить элемент в кусок памяти *)
							WITH
							| y : OPT.ConstArrOfByte DO (* BOOLEAN, CHAR, BYTE (для Ofront'а) *)
									IF x^.conval^.intval+i < LEN(y.val^) THEN
										y.val[x^.conval^.intval+i] := SHORT(SHORT(apar^.conval^.intval));
									ELSE  err(64) END;
							| y : OPT.ConstArrOfSInt DO (* типы с размером в 2 байта *)
									IF x^.conval^.intval+i < LEN(y.val^) THEN
										y.val[x^.conval^.intval+i] := SHORT(apar^.conval^.intval);
									ELSE  err(64) END;
							| y : OPT.ConstArrOfInt DO (* типы с размером в 4 байта *)
									IF x^.conval^.intval+i < LEN(y.val^) THEN
										y.val[x^.conval^.intval+i] := apar^.conval^.intval;
									ELSE  err(64) END;
							END;
						END;
						INC(i);
					ELSE err(64)
					END ;
					IF sym = comma THEN OPS.Get(sym)
					ELSIF (lparen <= sym) & (sym <= ident) THEN err(comma)
					ELSE EXIT
					END
				END;
			END;
			IF i # n THEN
				IF (i=1) & (typ^.BaseTyp^.form IN {Char,Byte} ) THEN
				(* один символ (мы его уже считали) может означать целую строку *)
				ELSE
			 		err(65)
				END
			END;
			INC(x^.conval^.intval, n); (* учли прочитанные элементы, даже если они не  прочитаны *)
		ELSIF  (typ^.BaseTyp^.form =15) & (typ^.BaseTyp^.comp = 2) THEN (* массив из массивов *)
			IF sym # rparen THEN
				LOOP ConstArray (x, typ^.BaseTyp);  (* рекурсивная обработка подмассива *)
					IF i < n THEN
						(*	??? проверим на совместимость *)
						INC(i);
					ELSE err(64)
					END ;
					IF sym = comma THEN OPS.Get(sym)
					ELSIF (lparen <= sym) & (sym <= ident) THEN err(comma)
					ELSE EXIT
					END
				END;
			END;
			IF i # n THEN  err(65)
			END;
		ELSE  err(51)
		END;
		CheckSym(rparen); (* скобка ) *)
		END;
	END ConstArray;

PROCEDURE Factor(VAR x: OPT.Node);
		VAR fpar, id: OPT.Object; apar: OPT.Node;
	BEGIN
		IF sym < lparen THEN err(13);
			REPEAT OPS.Get(sym) UNTIL sym >= lparen
		END;
		IF sym = ident THEN
			qualident(id); x := OPB.NewLeaf(id); selector(x);
			IF (x^.class = Nproc) & (x^.obj^.mode = SProc) THEN StandProcCall(x)	(* x may be NIL *)
			ELSIF sym = lparen THEN
				IF (x^.obj^.mode = Typ)  & (x^.obj^.typ^.form = Comp) & (x^.obj^.typ^.comp = Array)   THEN
				(* константный массив верхнего уровня*)
					OPB.NewArrConst(x);
					IF x^.obj^.typ^.BaseTyp^.form IN {Char,Byte} THEN (* ARRAY OF CHAR *)

					END;
					ConstArray(x, x^.obj^.typ);
					IF sym # semicolon THEN err (semicolon) END; (* на всякий случай проверим, что в конце ; *)
				ELSE
					OPS.Get(sym); OPB.PrepCall(x, fpar);
					ActualParameters(apar, fpar);
					OPB.Call(x, apar, fpar);
					CheckSym(rparen);
				END;
				IF level > 0 THEN OPT.topScope^.link^.leaf := FALSE END
			END
		ELSIF sym = number THEN
			CASE OPS.numtyp OF
			   char: x := OPB.NewIntConst(OPS.intval); x^.typ := OPT.chartyp
			| integer: x := OPB.NewIntConst(OPS.intval)
			| real: x := OPB.NewRealConst(OPS.realval, OPT.realtyp)
			| longreal: x := OPB.NewRealConst(OPS.lrlval, OPT.lrltyp)
			END ;
			OPS.Get(sym)
		ELSIF sym = string THEN
			x := OPB.NewString(OPS.str, OPS.intval); OPS.Get(sym)
		ELSIF sym = nil THEN
			x := OPB.Nil(); OPS.Get(sym)
		ELSIF sym = lparen THEN
			OPS.Get(sym); Expression(x); CheckSym(rparen)
		ELSIF sym = lbrak THEN
			OPS.Get(sym); err(lparen); Expression(x); CheckSym(rparen)
		ELSIF sym = lbrace THEN OPS.Get(sym); Sets(x)
		ELSIF sym = not THEN
			OPS.Get(sym); Factor(x); OPB.MOp(not, x)
		ELSE err(13); OPS.Get(sym); x := NIL
		END ;
		IF x = NIL THEN x := OPB.NewIntConst(1); x^.typ := OPT.undftyp END
	END Factor;

	PROCEDURE Term(VAR x: OPT.Node);
		VAR y: OPT.Node; mulop: BYTE;
	BEGIN Factor(x);
		WHILE (times <= sym) & (sym <= and) DO
			mulop := sym; OPS.Get(sym);
			Factor(y); OPB.Op(mulop, x, y)
		END
	END Term;

	PROCEDURE SimpleExpression(VAR x: OPT.Node);
		VAR y: OPT.Node; addop: BYTE;
	BEGIN
		IF sym = minus THEN OPS.Get(sym); Term(x); OPB.MOp(minus, x)
		ELSIF sym = plus THEN OPS.Get(sym); Term(x); OPB.MOp(plus, x)
		ELSE Term(x)
		END ;
		WHILE (plus <= sym) & (sym <= or) DO
			addop := sym; OPS.Get(sym);
			Term(y); OPB.Op(addop, x, y)
		END
	END SimpleExpression;

	PROCEDURE Expression(VAR x: OPT.Node);
		VAR y: OPT.Node; obj: OPT.Object; relation: BYTE;
	BEGIN SimpleExpression(x);
		IF (eql <= sym) & (sym <= geq) THEN
			relation := sym; OPS.Get(sym);
			SimpleExpression(y); OPB.Op(relation, x, y)
		ELSIF sym = in THEN
			OPS.Get(sym); SimpleExpression(y); OPB.In(x, y)
		ELSIF sym = is THEN
			OPS.Get(sym);
			IF sym = ident THEN
				qualident(obj);
				IF obj^.mode = Typ THEN OPB.TypTest(x, obj, FALSE)
				ELSE err(52)
				END
			ELSE err(ident)
			END
		END
	END Expression;

	PROCEDURE Receiver(VAR mode, vis: BYTE; VAR name: OPS.Name; VAR typ, rec: OPT.Struct);
		VAR obj: OPT.Object;
	BEGIN typ := OPT.undftyp; rec := NIL; vis := 0;
		IF sym = var THEN OPS.Get(sym); mode := VarPar
		ELSIF sym = in THEN OPS.Get(sym); mode := VarPar; vis := inPar	(* ??? *)
		ELSE mode := Var END ;
		name := OPS.name; CheckSym(ident); CheckSym(colon);
		IF sym = ident THEN OPT.Find(obj); OPS.Get(sym);
			IF obj = NIL THEN err(0)
			ELSIF obj^.mode # Typ THEN err(72)
			ELSE typ := obj^.typ; rec := typ;
				IF rec^.form = Pointer THEN rec := rec^.BaseTyp END ;
				IF ~((mode = Var) & (typ^.form = Pointer) & (rec^.comp = Record) OR
					(mode = VarPar) & (typ^.comp = Record)) THEN err(70); rec := NIL END ;
				IF (rec # NIL) & (rec^.mno # level) THEN err(72); rec := NIL END
			END
		ELSE err(ident)
		END ;
		CheckSym(rparen);
		IF rec = NIL THEN rec := OPT.NewStr(Comp, Record); rec^.BaseTyp := NIL END
	END Receiver;

	PROCEDURE Extends(x, b: OPT.Struct): BOOLEAN;
	BEGIN
		IF (b^.form = Pointer) & (x^.form = Pointer) THEN b := b^.BaseTyp; x := x^.BaseTyp END ;
		IF (b^.comp = Record) & (x^.comp = Record) THEN
			REPEAT x := x^.BaseTyp UNTIL (x = NIL) OR (x = b)
		END ;
		RETURN x = b
	END Extends;

	PROCEDURE ProcedureDeclaration(VAR x: OPT.Node);
		VAR proc, fwd: OPT.Object;
			name: OPS.Name;
			mode, vis: BYTE;
			forward: BOOLEAN;
			sys: SHORTINT;

		PROCEDURE GetCode;
			VAR ext: OPT.ConstExt; n: SHORTINT; c: INTEGER;
		BEGIN
			ext := OPT.NewExt(); proc^.conval^.ext := ext; n := 0;
			IF sym = string THEN
				WHILE OPS.str[n] # 0X DO ext[n+1] := OPS.str[n]; INC(n) END ;
				ext^[0] := SHORT(CHR(n)); OPS.Get(sym)
			ELSE
				LOOP
					IF sym = number THEN c := OPS.intval; INC(n);
						IF (c < 0) OR (c > 255) OR (n = OPT.MaxConstLen) THEN
							err(64); c := 1; n := 1
						END ;
						OPS.Get(sym); ext^[n] := SHORT(CHR(c))
					END ;
					IF sym = comma THEN OPS.Get(sym)
					ELSIF sym = number THEN err(comma)
					ELSE ext^[0] := SHORT(CHR(n)); EXIT
					END
				END
			END ;
			INCL(proc^.conval^.setval, hasBody)
		END GetCode;

		PROCEDURE GetParams;
		BEGIN
			proc^.vis := vis; proc^.mode := mode; proc^.typ := OPT.notyp;
			proc^.sysflag := SHORT(sys);
			proc^.conval := OPT.NewConst(); proc^.conval^.setval := {};
			IF sym = lparen THEN
				OPS.Get(sym); FormalParameters(proc^.link, proc^.typ)
			END ;
			IF fwd # NIL THEN
				OPB.CheckParameters(proc^.link, fwd^.link, TRUE);
				IF proc^.typ # fwd^.typ THEN err(117) END ;
				proc := fwd; OPT.topScope := proc^.scope;
				IF mode = IProc THEN proc^.mode := IProc END
			END
		END GetParams;

		PROCEDURE Body;
			VAR procdec, statseq: OPT.Node; c: INTEGER;
		BEGIN
			c := OPM.errpos;
			INCL(proc^.conval^.setval, hasBody);
			CheckSym(semicolon); Block(procdec, statseq);
			OPB.Enter(procdec, statseq, proc); x := procdec;
			x^.conval := OPT.NewConst(); x^.conval^.intval := c;
			IF sym = ident THEN
				IF OPS.name # proc^.name THEN err(4) END ;
				OPS.Get(sym)
			ELSE err(ident)
			END
		END Body;

		PROCEDURE TProcDecl;
			VAR baseProc: OPT.Object;
				objTyp, recTyp: OPT.Struct;
				objMode, objVis: BYTE;
				objName: OPS.Name;
		BEGIN
			OPS.Get(sym); mode := TProc;
			IF level > 0 THEN err(73) END ;
			Receiver(objMode, objVis, objName, objTyp, recTyp);
			IF sym = ident THEN
				name := OPS.name; CheckMark(vis);
				OPT.FindField(name, recTyp, fwd);
				OPT.FindField(name, recTyp^.BaseTyp, baseProc);
				IF (baseProc # NIL) & (baseProc^.mode # TProc) THEN baseProc := NIL END ;
				IF fwd = baseProc THEN fwd := NIL END ;
				IF (fwd # NIL) & (fwd^.mnolev # level) THEN fwd := NIL END ;
				IF (fwd # NIL) & (fwd^.mode = TProc) & ~(hasBody IN fwd^.conval^.setval) THEN
					(* there exists a corresponding forward declaration *)
					proc := OPT.NewObj(); proc^.leaf := TRUE;
					IF fwd^.vis # vis THEN err(118) END
				ELSE
					IF fwd # NIL THEN err(1); fwd := NIL END ;
					OPT.OpenScope(0, NIL); OPT.topScope^.right := recTyp^.link; OPT.Insert(name, proc);
					recTyp^.link := OPT.topScope^.right; OPT.CloseScope;
				END ;
				INC(level); OPT.OpenScope(level, proc);
				OPT.Insert(objName, proc^.link); proc^.link^.mode := objMode; proc^.link^.vis := objVis; proc^.link^.typ := objTyp;
				GetParams;
				IF baseProc # NIL THEN
					IF (objMode # baseProc^.link^.mode) OR ~Extends(objTyp, baseProc^.link^.typ) THEN err(115) END ;
					OPB.CheckParameters(proc^.link^.link, baseProc^.link^.link, FALSE);
					IF proc^.typ # baseProc^.typ THEN err(117) END ;
					IF (baseProc^.vis = external) & (proc^.vis = internal) &
						(recTyp^.strobj # NIL) & (recTyp^.strobj^.vis = external) THEN err(109)
					END ;
					INCL(proc^.conval^.setval, isRedef)
				END ;
				IF ~forward THEN Body END ;
				DEC(level); OPT.CloseScope
			ELSE err(ident)
			END
		END TProcDecl;

	BEGIN proc := NIL; forward := FALSE; x := NIL; mode := LProc; sys := 0;
		IF (sym # ident) & (sym # lparen) THEN
			CheckSysFlag(sys, 0);
			IF sys # 0 THEN
				IF ~OPT.SYSimported THEN err(135) END;
				IF sym = minus THEN mode := CProc; OPS.Get(sym) END
			ELSE
				IF sym = times THEN	(* mode set later in OPB.CheckAssign *)
				ELSIF sym = arrow THEN forward := TRUE
				ELSIF sym = plus THEN mode := IProc
				ELSIF sym = minus THEN mode := CProc
				ELSE err(ident)
				END ;
				IF (mode IN {IProc, CProc}) & ~OPT.SYSimported THEN err(135) END ;
				OPS.Get(sym)
			END
		END ;
		IF sym = lparen THEN TProcDecl
		ELSIF sym = ident THEN OPT.Find(fwd);
			name := OPS.name; CheckMark(vis);
			IF (vis # internal) & (mode = LProc) THEN mode := XProc END ;
			IF (fwd # NIL) & ((fwd^.mnolev # level) OR (fwd^.mode = SProc)) THEN fwd := NIL END ;
			IF (fwd # NIL) & (fwd^.mode IN {LProc, XProc}) & ~(hasBody IN fwd^.conval^.setval) THEN
				(* there exists a corresponding forward declaration *)
				proc := OPT.NewObj(); proc^.leaf := TRUE;
				IF fwd^.vis # vis THEN err(118) END
			ELSE
				IF fwd # NIL THEN err(1); fwd := NIL END ;
				OPT.Insert(name, proc)
			END ;
			IF (mode # LProc) & (level > 0) THEN err(73) END ;
			INC(level); OPT.OpenScope(level, proc);
			proc^.link := NIL; GetParams;
			IF mode = CProc THEN GetCode
			ELSIF ~forward THEN Body
			END ;
			DEC(level); OPT.CloseScope
		ELSE err(ident)
		END
	END ProcedureDeclaration;

	PROCEDURE CaseLabelList(VAR lab: OPT.Node; LabelForm: SHORTINT; VAR n: SHORTINT; VAR tab: CaseTable);
		VAR x, y, lastlab: OPT.Node; i, f: SHORTINT; xval, yval: INTEGER;
	BEGIN lab := NIL; lastlab := NIL;
		LOOP ConstExpression(x); f := x^.typ^.form;
			IF f IN intSet + {Char} THEN  xval := x^.conval^.intval
			ELSE err(61); xval := 1
			END ;
			IF (f IN intSet) # (LabelForm IN intSet) THEN err(60) END;
			IF sym = upto THEN
				OPS.Get(sym); ConstExpression(y); yval := y^.conval^.intval;
				IF (y^.typ^.form # f) & ~((f IN intSet) & (y^.typ^.form IN intSet)) THEN err(60) END ;
				IF yval < xval THEN err(63); yval := xval END
			ELSE yval := xval
			END ;
			x^.conval^.intval2 := yval;
			(*enter label range into ordered table*)  i := n;
			IF i < OPM.MaxCases THEN
				LOOP
					IF i = 0 THEN EXIT END ;
					IF tab[i-1].low <= yval THEN
						IF tab[i-1].high >= xval THEN err(62) END ;
						EXIT
					END ;
					tab[i] := tab[i-1]; DEC(i)
				END ;
				tab[i].low := xval; tab[i].high := yval; INC(n)
			ELSE err(213)
			END ;
			OPB.Link(lab, lastlab, x);
			IF sym = comma THEN OPS.Get(sym)
			ELSIF (sym = number) OR (sym = ident) THEN err(comma)
			ELSE EXIT
			END
		END
	END CaseLabelList;

	PROCEDURE StatSeq(VAR stat: OPT.Node);
		VAR fpar, id, t, obj: OPT.Object; idtyp: OPT.Struct; e: BOOLEAN; L: LONGINT;
				s, x, y, z, apar, last, lastif: OPT.Node; pos: INTEGER; name: OPS.Name;

		PROCEDURE CasePart(VAR x: OPT.Node);
			VAR n: SHORTINT; low, high: INTEGER; e: BOOLEAN;
					tab: CaseTable; cases, lab, y, lastcase: OPT.Node;
		BEGIN
			Expression(x); pos := OPM.errpos;
			IF (x^.class = Ntype) OR (x^.class = Nproc) THEN err(126)
			ELSIF ~(x^.typ^.form IN {Byte, Char..LInt}) THEN err(125)
			END ;
			CheckSym(of); cases := NIL; lastcase := NIL; n := 0;
			LOOP
				IF sym < bar THEN
					CaseLabelList(lab, x^.typ^.form, n, tab);
					CheckSym(colon); StatSeq(y);
					OPB.Construct(Ncasedo, lab, y); OPB.Link(cases, lastcase, lab)
				END ;
				IF sym = bar THEN OPS.Get(sym) ELSE EXIT END
			END ;
			IF n > 0 THEN low := tab[0].low; high := tab[n-1].high;
				IF high - low > OPM.MaxCaseRange THEN err(209) END
			ELSE low := 1; high := 0
			END ;
			e := sym = else;
			IF e THEN OPS.Get(sym); StatSeq(y) ELSE y := NIL END ;
			OPB.Construct(Ncaselse, cases, y); OPB.Construct(Ncase, x, cases);
			cases^.conval := OPT.NewConst();
			cases^.conval^.intval := low; cases^.conval^.intval2 := high;
			IF e THEN cases^.conval^.setval := {1} ELSE cases^.conval^.setval := {} END
		END CasePart;

		PROCEDURE SetPos(x: OPT.Node);
		BEGIN
			x^.conval := OPT.NewConst(); x^.conval^.intval := pos
		END SetPos;

		PROCEDURE CheckBool(VAR x: OPT.Node);
		BEGIN
			IF (x^.class = Ntype) OR (x^.class = Nproc) THEN err(126); x := OPB.NewBoolConst(FALSE)
			ELSIF x^.typ^.form # Bool THEN err(120); x := OPB.NewBoolConst(FALSE)
			END ;
			pos := OPM.errpos
		END CheckBool;

		PROCEDURE For0Original;
		BEGIN
			OPS.Get(sym);
			IF sym = ident THEN qualident(id);
				IF ~(id^.typ^.form IN intSet) THEN err(68) END ;
				CheckSym(becomes); Expression(y); pos := OPM.errpos;
				x := OPB.NewLeaf(id); OPB.Assign(x, y); SetPos(x);
				CheckSym(to); Expression(y); pos := OPM.errpos;
				IF y^.class # Nconst THEN
					name := "@@"; OPT.Insert(name, t); t^.name := "@for";	(* avoid err 1 *)
					t^.mode := Var; t^.typ := x^.left^.typ;
					obj := OPT.topScope^.scope;
					IF obj = NIL THEN OPT.topScope^.scope := t
					ELSE
						WHILE obj^.link # NIL DO obj := obj^.link END ;
						obj^.link := t
					END ;
					z := OPB.NewLeaf(t); OPB.Assign(z, y); SetPos(z); OPB.Link(stat, last, z);
					y := OPB.NewLeaf(t)
				ELSIF (y^.typ^.form < SInt) OR (y^.typ^.form > x^.left^.typ^.form) THEN err(113)
				END ;
				OPB.Link(stat, last, x);
				IF sym = by THEN OPS.Get(sym); ConstExpression(z) ELSE z := OPB.NewIntConst(1) END ;
				pos := OPM.errpos; x := OPB.NewLeaf(id);
				IF z^.conval^.intval > 0 THEN OPB.Op(leq, x, y)
				ELSIF z^.conval^.intval < 0 THEN OPB.Op(geq, x, y)
				ELSE err(63); OPB.Op(geq, x, y)
				END ;
				CheckSym(do); StatSeq(s);
				y := OPB.NewLeaf(id); OPB.StPar1(y, z, incfn); SetPos(y);
				IF s = NIL THEN s := y
				ELSE z := s;
					WHILE z^.link # NIL DO z := z^.link END ;
					z^.link := y
				END ;
				CheckSym(end); OPB.Construct(Nwhile, x, s)
			ELSE err(ident)
			END
		END For0Original;

		PROCEDURE CalcNumIters(A, B: OPT.Node; Step: INTEGER);
		(* Вычисляем количество итераций цикла FOR. Результат:
		e=TRUE, если L = точное количество итераций
		e=FALSE, если L -верхняя граница кол-ва итераций (количество итераций <=L) *)
			VAR Nmin, Nmax: INTEGER;
		BEGIN
			IF Step>0 THEN
				IF A^.class = Nconst THEN Nmin := A^.conval^.intval
											ELSE Nmin := OPB.Min(id^.typ^.form)
				END;
				IF B^.class = Nconst THEN Nmax := B^.conval^.intval
											ELSE Nmax := OPB.Max(id^.typ^.form)
				END;
				L := MC.Div (LONG(Nmax) - Nmin, Step ) + 1;
			ELSE
				IF A^.class = Nconst THEN Nmax := A^.conval^.intval
											ELSE Nmax := OPB.Max(id^.typ^.form)
				END;
				IF B^.class = Nconst THEN Nmin:= B^.conval^.intval
											ELSE Nmin:= OPB.Min(id^.typ^.form)
				END;
				L := MC.Div (LONG(Nmax) - Nmin, -LONG(Step)) + 1
			END;
			e := (A^.class = Nconst) & (B^.class = Nconst);
			(* если обе константы, то можно вычислить точно *)
		END CalcNumIters;

		PROCEDURE CalcNlast(A, B: OPT.Node; Step: INTEGER): INTEGER;
		(* Вычисляем значение переменной цикла на последней итерации *)
		BEGIN
			ASSERT( (A^.class = Nconst) & (B^.class = Nconst) );
			RETURN SHORT(A^.conval^.intval + (L-1) * Step); (* пока не ввели 64 разрядность используем SHORT*)
		END CalcNlast;

		PROCEDURE FindCond(A, B: OPT.Node; Step: INTEGER);
		(* найти оптимальное условие id?0 для замены цикла FOR
		циклом REPEAT UNTIL id?0 *)
		VAR Nlast, sgnPostLast, sgnPredLast: INTEGER; cond: SHORTINT;
		BEGIN
			cond := null; (* пока условие не нашли *)
			Nlast := CalcNlast(A, B, Step); (* последнее за границы типа не выйдет *)
			(* в отличие от последующего или предыдущего *)
			sgnPostLast := Sign(OPB.Short2Size(SHORT(LONG(Nlast) + Step), id^.typ^.size));
			sgnPredLast := Sign(OPB.Short2Size(SHORT(LONG(Nlast) - Step), id^.typ^.size));
			IF Nlast = 0 THEN cond := -eql
			ELSIF ( Sign(Nlast) = -Sign(Step) ) & (Sign(Nlast) # sgnPostLast )THEN
				CASE sgnPostLast OF
				|-1: cond := lss;
				| 0: cond := eql;
				| 1: cond := geq;
				END
			ELSIF Sign(Nlast) = Sign(Step) THEN
				IF (Sign(Nlast) # sgnPostLast) & (Sign(A^.conval^.intval + Step) # sgnPostLast) THEN
					(* тут можно бы воспользоваться флажком переполнения , если бы из С был доступ *)
					CASE sgnPostLast OF
					|-1: cond := lss;
					| 0: cond := eql;
					| 1: IF A^.conval^.intval + Step = 0 THEN cond := gtr ELSE cond := geq END;
					END;
				END
			END;

			IF ((cond = null) OR (cond = gtr)) & (Sign(Nlast) = Sign(Step)) &
				 (sgnPredLast # Sign(Nlast)) THEN
					CASE Sign(Nlast) OF
					|-1: cond := -lss;
					| 0: cond := -eql;
					| 1: IF sgnPredLast = 0 THEN cond := -gtr ELSE cond := -geq END;
					END
				END;
			IF cond # null THEN
			(* вспомогательная переменная не нужна *)
				t := id;
				apar := OPB.NewLeaf(id);
				IF cond < 0 THEN
					 idtyp := NIL;
					 OPB.Assign(apar, OPB.NewIntConst(0))
				ELSE
					idtyp := id^.typ;
					ASSERT(idtyp # NIL);
					 OPB.Assign(apar, z)
				 END;
				 apar^.subcl := SHORT(SHORT(ABS(cond)))
			ELSE
				t := NIL; (* нужна вспомогательная переменная *)
			END;
			(* если t=id то можно обойтись без вспомогательной переменной, применив
			либо REPEAT INC(id,Step); тело цикла; UNTIL id cond 0	(при idtyp=NIL)
			либо REPEAT тело цикла; INC(id,Step) UNTIL id cond 0	(при idtyp # NIL)
			где cond задается как apar^.subcl *)
		END FindCond;

		PROCEDURE For1Extended; (* Implemented by Oleg Komlev (Saferoll) *)
		BEGIN (* построение синтаксического дерева для FOR *)
			OPS.Get(sym);	(* взять символ после FOR *)
			IF sym = ident THEN qualident(id);	(* если это идентификатор, уточнить его *)
				IF ~(id^.typ^.form IN intSet) THEN err(68) END;	(* он должен быть целого типа*)
				CheckSym(becomes); Expression(apar); pos := OPM.errpos;	(* потом д б  «:=  А » *)
				x := OPB.NewLeaf(id); OPB.Assign(x, apar); SetPos(x);	(* строим узел х=«id :=  А» *)
				(* apar = "A" *)
				CheckSym(to); Expression(y); pos := OPM.errpos;	(* далее д б ТО выражение (В) *)
				IF sym = by THEN	(* если далее задан шаг BY Step,*)
					OPS.Get(sym); ConstExpression(z)	(* то читаем его как константное выражение*)
				ELSE
					z := OPB.NewIntConst(1)	(*иначе берем константу 1*)
				END ;

				(* x = "id:=A"; y=B; z=Step; apar="A" *)
				obj := NIL;	(* по умолчанию IF нужен*)
				IF (y^.class = Nconst) THEN	(* если В - константа *)
					IF (x^.left^.typ^.form = Byte) & (( y^.conval^.intval < OPB.Min(Byte)) OR ( y^.conval^.intval > OPB.Max(Byte)))
					 OR  (x^.left^.typ^.form # Byte)&((y^.typ^.form < SInt) OR (y^.typ^.form > x^.left^.typ^.form)) THEN
						err(113); y := OPB.NewIntConst(0) (* то это д б константа совместимого типа *)
					END;
					IF apar^.class = Nconst THEN (* А и В - константы*)
						IF (z^.conval^.intval > 0) & (apar^.conval^.intval <= y^.conval^.intval ) OR
							(z^.conval^.intval < 0) & (apar^.conval^.intval >= y^.conval^.intval ) THEN
								obj := id; (* условие IF не нужно*)
						ELSE
								id := NIL (* цикл не выполнится ни разу *)
						END
					ELSE	(* В - константа, А -нет*)
							IF (z^.conval^.intval > 0) & (y^.conval^.intval = OPB.Max(id^.typ^.form)) OR
							(z^.conval^.intval < 0) & (y^.conval^.intval = OPB.Min(id^.typ^.form)) THEN
								obj := id; (* условие IF не нужно*)
							END
					END
				ELSE
					IF apar^.class = Nconst THEN	(* A - константа, B -нет*)
						IF (z^.conval^.intval > 0) & (apar^.conval^.intval = OPB.Min(id^.typ^.form)) OR
						(z^.conval^.intval < 0) & (apar^.conval^.intval = OPB.Max(id^.typ^.form)) THEN
							obj := id; (* условие IF не нужно*)
						END
					END
				END;

				IF id # NIL THEN
					CalcNumIters(apar, y, z^.conval^.intval) ;
					IF y^.class # Nconst THEN SetPos(y) END (* запомнить позицию В ??? *)
				END;
				pos := OPM.errpos;	(* теперь указатель ошибок указывает на Step*)
				IF (z^.conval^.intval = 0) OR (z^.typ^.form < SInt) OR (z^.typ^.form > x^.left^.typ^.form) THEN
					IF (x^.left^.typ^.form # Byte) OR (z^.conval^.intval < OPB.Min(Byte)) OR ( z^.conval^.intval > OPB.Max(Byte))
					THEN err(63); z := OPB.NewIntConst(1)
					END;
				END;

				(* id = NIL - цикл не выполнится ни разу *)
				(* иначе: x = "id:=A"; z=Step; apar="A"; y = "B" *)
				(* если e=TRUE, то L = кол-во итераций иначе L>=кол-во итераций *)
				IF id # NIL THEN
					IF L # 1 THEN (* попытаться обойтись без вспомогательной переменной*)
						IF e THEN
							FindCond(apar, y, z^.conval^.intval);
						ELSE
							(* если точно не подсчитать, то нужна вспомогательная переменная
							хотя в некоторых случаях можно и тут обойтись без нее*)
							IF z^.conval^.intval = OPB.Min(id^.typ^.form) THEN
								t := id; (* вспомогательная переменная не нужна *)
								idtyp := id^.typ; (* инкремент в конце цикла *)
								apar := OPB.NewLeaf(id);
								OPB.Assign(apar, z);
								apar^.subcl := geq (* условие id>=0 *)
							ELSE
								t := NIL; (* нужна вспомогательная переменная *)
							END
						END
					END;
					IF t # id THEN (* заводим вспомогательную переменную*)
						name := "@@"; OPT.Insert(name, t); t^.name := "@for";	(* avoid err 1 *)
						t^.mode := Var; t^.typ := x^.left^.typ;
						fpar := OPT.topScope^.scope;
						IF fpar = NIL THEN OPT.topScope^.scope := t
						ELSE
							WHILE fpar^.link # NIL DO fpar := fpar^.link END ;
							fpar^.link := t
						END ;
						apar := OPB.NewLeaf(t);
						OPB.Assign(apar, OPB.NewIntConst(-1));
						apar^.subcl := eql;	(* условие t=0 *)
					END;

					IF (t = id) & (idtyp = NIL) THEN	(* а вот тут возможен выход константы за границы типа,
					поэтому свернем ее *)
						x^.right := OPB.NewShortConst(LONG(x^.right^.conval^.intval) - z^.conval^.intval, id^.typ^.size);
					END;
					OPB.Link(stat, last, x); x := NIL;	(* добавим в дерево id := A или id := A -Step *)

					IF (t # id) & (L # 1) THEN (* строим t:= колво итераций *)
						IF e THEN
							s := OPB.NewShortConst(L, id^.typ^.size)
						ELSE
							IF (obj = NIL) & ( y^.class # Nconst) THEN (* нужно t := B*)
								pos := y^.conval^.intval; (* восстановим позицию *)
								x := OPB.NewLeaf(t); OPB.Assign(x, y); SetPos(x); (* t:=B *)
								OPB.Link(stat, last, x); (* добавим в дерево t:= B *)
								y := x;
							END;
							pos := OPM.errpos;
							x := OPB.NewLeaf(id);
							IF y^.class = Nconst	THEN s := OPB.NewIntConst(y^.conval^.intval)
							ELSIF y^.class = Nassign THEN s := OPB.NewLeaf(t)
							ELSE s := y
							END;
							IF z^.conval^.intval > 0 THEN	(* подготовим выражение "кол-во повторений" *)
								OPB.Op(minus, s, x); (* B-x *)
								OPB.MOp(unsgn, s);
								OPB.Op(div, s, OPB.NewIntConst( z^.conval^.intval));	(* (B-x) div Step *)
								OPB.Op(plus, s, OPB.NewIntConst(1));	(* (B-x) div Step +1 *)
							ELSE	(* в зависимости от знака Step*)
								OPB.Op(minus, x, s); (* x-B *)
								OPB.MOp(unsgn, x);
								OPB.Op(div, x, OPB.NewIntConst( -z^.conval^.intval));	(* (x-B) div Step *)
								OPB.Op(plus, x, OPB.NewIntConst(1));	(* (x-B) div Step +1 *)
								s := x;
							END;
						END;
						x := OPB.NewLeaf(t); OPB.Assign(x, s); SetPos(x);(* строим узел х=«t:= колво итераций*)
					ELSE	(* фиктивный узел id=0 *)
						x := OPB.NewLeaf(id); OPB.Assign(x, OPB.NewIntConst(0)); SetPos(x);
					END
				ELSE x := NIL  (* от цикла не остается ничего *)
				END; (* id # NIL   *)

				CheckSym(do); StatSeq(s); CheckSym(end);

				IF id # NIL THEN
					(* х=«t:= колво итераций; z=Step; apar="A"; y = "t := B"(или просто "В"); s="тело цикла" *)
					IF (t = id) & (idtyp = NIL) THEN
						x^.link := s;
						s := OPB.NewLeaf(id); OPB.StPar1(s, z, incfn); SetPos(s);	(* s= "INC(id,Step)" *)
						IF x^.link # NIL THEN s^.link := x^.link; x^.link := NIL END;
					END;
					(* s="INC(id,Step)";тело цикла" *)

					IF L # 1 THEN
						(* строим спецусловие для ForUntil: y=«id := z» или «id := 0» *)
						SetPos(apar);
						OPB.Construct(Nrepeat, s, apar); (* постоили REPEAT UNTIL*)
						SetPos(s);
						IF t # id THEN (* нужен инкремент id*)
							apar := OPB.NewLeaf(id); OPB.StPar1(apar, z, incfn);
							SetPos(apar);	(* apar= "INC(id,Step)" *)
							x^.link := apar;

							apar := s^.left; (* тело цикла*)
							IF apar = NIL THEN s^.left := x^.link
							ELSE
								WHILE apar^.link # NIL DO apar := apar^.link END ;
								(* apar - последний оператор в теле цикла*)
								apar^.link := x^.link;
							END;
							x^.link := NIL;
						END;

						(* х="t:= колво итераций"; z=Step; y = "t:= B" ( "B") ; s="REPAT... UNTIL" *)
						IF t # id THEN x^.link := s;	(* х="t:= колво итераций; REPAT... UNTIL" *)
									ELSE x := s
						END;

					ELSE
						x := s;
					END;

					(* строим IF *)
					IF obj = NIL	(* цикл может не исполниться ни разу*) THEN (* нужен IF *)
						IF y^.class = Nassign THEN y := OPB.NewLeaf(t) END; (* присваивали t:=B*)
						s := OPB.NewLeaf(id);
						IF z^.conval^.intval > 0 THEN OPB.Op(leq,s,y)
														ELSE OPB.Op(geq,s,y)
						END;
						OPB.Construct(Nif, s, x); SetPos(s); lastif := s;	(* строим IF id<=B THEN ...*)
						OPB.Construct(Nifelse, s, NIL);						(* ветви ELSE нет*)
						OPB.OptIf(s);
						x := s;
					END;
					pos := OPM.errpos;
				ELSE
					x := NIL;	(* от цикла не остается ничего *)
				END; (* id # NIL*)
			ELSE err(ident)
			END
		END For1Extended;

		PROCEDURE For2Experimental; (* Implemented by Oleg Komlev (Saferoll) too *)
		BEGIN (* построение синтаксического дерева для FOR *)
			OPS.Get(sym);				(* взять символ после FOR *)
			IF sym = ident THEN qualident(id); (* если это идентификатор, уточнить его *)
				IF ~(id^.typ^.form IN intSet) THEN err(68) END ;(* он должен быть целого типа*)
				CheckSym(becomes); Expression(y); pos := OPM.errpos; 	(* потом д б  «:=  выражение  (А)» *)
				x := OPB.NewLeaf(id); OPB.Assign(x, y); SetPos(x);(* вставим в дерево «id :=  А» *)
				apar:=y;	(* apar  = "A" *)
				CheckSym(to); Expression(y); pos := OPM.errpos;   			(* далее д б ТО выражение (В) *)
				IF sym = by THEN (* если далее задан шаг BY Step,*)
						OPS.Get(sym); ConstExpression(z) (* то читаем его как константное выражение*)
				ELSE
						z := OPB.NewIntConst(1) (*иначе берем константу 1*)
				END ;
				(*  x = "id:=A"   y=B  z=Step  apar="A" *)
				e := FALSE;  (* в общем случае IF не отбрасывается*)
				IF (y^.class = Nconst) THEN
					obj := id; t := id;
					IF (y^.typ^.form < SInt) OR (y^.typ^.form > x^.left^.typ^.form) THEN
								err(113); t :=  id; obj := NIL; (* то это д б константа совместимого типа *)
					END;
					IF obj # NIL THEN
						IF apar^.class = Nconst THEN (*  A и B - константы *)
							IF 	 (z^.conval^.intval # 0) & (apar^.conval^.intval = y^.conval^.intval) THEN
								e:= FALSE; t:= NIL; (* цикл выполнится ровно 1 раз*)
							ELSIF 	(z^.conval^.intval > 0) & (apar^.conval^.intval <= y^.conval^.intval)  OR
									(z^.conval^.intval < 0) &(apar^.conval^.intval >= y^.conval^.intval) THEN
								e:= TRUE; t  :=id; (* цикл заведомо выполнится хотя бы раз, *)
					    	ELSIF (z^.conval^.intval > 0) & (apar^.conval^.intval >= y^.conval^.intval)  OR
									(z^.conval^.intval < 0) &(apar^.conval^.intval <= y^.conval^.intval) THEN
								t := NIL; e:= TRUE; (* цикл не выполнится ни разу*)
							END ;
						END;
						IF t # NIL THEN
							IF 	(ABS(z^.conval^.intval)=1)& (ABS(y^.conval^.intval) <= 1)  THEN
										(* частные случаи, когда переменная t не нужна*)
										t := id; (* вместо t используем id *)
										IF y^.conval^.intval = 0 THEN obj := NIL
										ELSIF  y^.conval^.intval = z^.conval^.intval THEN obj := id
										ELSE obj := z.typ.strobj;  ASSERT( obj # id);
										END
							ELSE
										name := "@@"; OPT.Insert(name, t);	(* нужна переменная  t*)
							END;
						END;
					END;
				ELSE	(*  B - не константа  *)
								name := "@@"; OPT.Insert(name, t); 	(* нужна переменная  t*)
				END;

				IF (t # NIL) OR (e=FALSE)  THEN
					 OPB.Link(stat, last, x); 	 (* добавим в дерево id := A*)
					IF ( t # id) & (t # NIL) THEN
						 t^.name := "@for" ; t^.mode := Var; t^.typ := x^.left^.typ;		(* в таблице имен создаем*)
						obj := OPT.topScope^.scope;							(*вспомогательную *)
						IF obj = NIL THEN OPT.topScope^.scope := t 				(* переменную t для счетчика повторений*)
						ELSE
									WHILE obj^.link # NIL DO obj := obj^.link END ;
									obj^.link := t
						END ;
						obj:= t;  ASSERT( obj # id);
					END;
					(* Переменные t и obj задают 4 случая цикла *)
					IF (t # id) & (t # NIL) THEN
						IF TRUE THEN
							x:= OPB.NewLeaf(t); OPB.Assign(x, y); SetPos(x); (* t := B*)
							OPB.Link(stat, last, x);
						END;
						pos := OPM.errpos; (* теперь ошибка будет указывать на шаг*)
						x := OPB.NewLeaf(id);
						(* z = "Step", x = "id", y = "B" *)
							IF z^.conval^.intval > 0 THEN (* подготовим выражение "кол-во повторений" *)
							IF FALSE THEN
								x := OPB.NewIntConst( (y^.conval^.intval -apar^.conval^.intval) DIV z^.conval^.intval + 1)
							ELSE
								y:=OPB.NewLeaf(t);
								OPB.Op(minus, y,x); (* B-x *)
								OPB.Op(div, y, OPB.NewIntConst( z^.conval^.intval)); (* (B-x) div Step *)
								OPB.Op(plus, y, OPB.NewIntConst(1)); (* (B-x) div Step +1 *)
								x := y;
							END
						ELSIF z^.conval^.intval < 0 THEN (* в зависимости от знака Step*)
							IF FALSE THEN
								x := OPB.NewIntConst( (apar^.conval^.intval -y^.conval^.intval) DIV (-z^.conval^.intval) + 1)
							ELSE
								y:=OPB.NewLeaf(t);
								OPB.Op(minus, x, y); (* x-B *)
								OPB.Op(div, x, OPB.NewIntConst( -z^.conval^.intval)); (* (x-B) div Step *)
								OPB.Op(plus, x, OPB.NewIntConst(1)); (* (x-B) div Step +1 *)
							END
						ELSE err(63); OPB.Op(minus, x, y)
						END;
						(* x =    " (B-x) div Step +1"   или   "(x-В) div Step +1 "  *)
						y := OPB.NewLeaf(t);OPB.Assign(y, x); SetPos(y);  (*  y = "t :=  (B-x) div Step +1 " *)
					ELSE (* без переменной t *)
						IF t # NIL THEN
							IF  (obj = NIL) OR (obj = id) THEN  (* нужно нейтрализовать первый инкремент*)
								y := OPB.NewLeaf(id); OPB.StPar1(y, z, decfn); SetPos(y);  (* x= "DEC(id,Step)"*)
							ELSE
								y := OPB.NewLeaf(id);  (*  y = "id"  - фиктивный оператор, т.к. нейтрализация не нужна *)
							END
						END
					END;
					pos := OPM.errpos;
					IF z^.conval^.intval=0 THEN err(63) END; (* нулевой шаг недопустим*)
					IF t # NIL THEN
						x := OPB.NewLeaf(id); OPB.StPar1(x, z, incfn); SetPos(x);  (* x= "INC(id,Step)"*)
						y^.link := x;  (* t :=  (B-x) div Step +1; INC(id,Step) *)
						IF t # id THEN (* добавить DEC(t) *)
							x := OPB.NewLeaf(t); OPB.StPar1(x, OPB.NewIntConst(1), decfn); SetPos(x);
							y^.link^.link := x;  (* t :=  (B-x) div Step +1; INC(id,Step); DEC(t) *)
						END;
					END;
					CheckSym(do); StatSeq(s);	(* дальше д б DO и тело цикла s *)
					IF t # NIL THEN
						IF s = NIL THEN
							s := y^.link; 	(* если тело цикла — пусто, то там д б только  INC(id,Step); [ DEC(t) ] *)
						ELSIF (obj=NIL) OR (obj = id) THEN
							y^.link^.link := s;  s:=y^.link;	(*  добавляем в начало тела цикла   INC(n,Step); *)
						ELSE  (*  ищем  конец тела цикла  *)
						 	x := s;
							WHILE x^.link # NIL DO x := x^.link END ;
							x^.link := y^.link;	(*  добавляем в конец тела цикла  INC(n,Step); [ DEC(t) ]*)
						END;
					END;
					CheckSym(end); 	(* в конце д б END *)
					IF t # NIL THEN
						IF obj # id THEN
							x := OPB.NewLeaf(t);  OPB.Op(eql, x, OPB.NewIntConst(0)); (* условие "t = 0"   "(id = 0)" *)
						ELSE
							x := OPB.NewLeaf(id);
							IF z^.conval^.intval > 0 THEN OPB.Op(gtr, x, OPB.NewIntConst(0))  (* условие id > 0 *)
							ELSIF z^.conval^.intval < 0 THEN OPB.Op(lss, x, OPB.NewIntConst(0)) (* или id < 0 *)
							END
						END;
						OPB.Construct(Nrepeat, s,x);SetPos(s); (* REPEAT ...UNTIL t=0; (id >0, id <0)*)
						y^.link := s; (*  t :=  (B-x) div Step +1; REPEAT ... UNTIL t=0;   *)
						IF y^.class = Nvar THEN (* фиктивный оператор *)
							s:=y^.link ; (* отбросим фиктивный оператор*)
						ELSE
							s:=y
						END;
						x := OPB.NewLeaf(id); (*начинаем строить условие IF*)
						IF t # id THEN y := OPB.NewLeaf(t)
						ELSIF obj = NIL THEN  y := OPB.NewIntConst(0)
						ELSIF obj = id  THEN y := z
						ELSE y:= OPB.NewIntConst( -z^.conval^.intval )
						END;
					END;
					(*  y =   <B>  = "t" или "+-Step"  (для условия IF)  *)
					IF e  OR (t = NIL) THEN x :=  OPB.NewBoolConst(TRUE)
					ELSIF z^.conval^.intval > 0 THEN OPB.Op(leq, x, y)  (* условие id <= B *)
					ELSE  OPB.Op(geq, x, y); (* или id >= B*)
					END ;
				ELSE  (* цикл не выполнится ни разу*)
						CheckSym(do); StatSeq(s);	(* дальше д б DO и тело цикла s *)
						x :=  OPB.NewBoolConst(FALSE);
						CheckSym(end);
				END;
				OPB.Construct(Nif, x, s); SetPos(x); lastif := x;    (* строим IF id<=B THEN ...*)
				OPB.Construct(Nifelse, x, NIL); 			(* ветви ELSE нет*)
				OPB.OptIf(x); pos := OPM.errpos
			ELSE err(ident)
			END
		END For2Experimental;

	BEGIN stat := NIL; last := NIL;
		LOOP x := NIL;
			IF sym < ident THEN err(14);
				REPEAT OPS.Get(sym) UNTIL sym >= ident
			END ;
			IF sym = ident THEN
				qualident(id); x := OPB.NewLeaf(id); selector(x);
				IF sym = becomes THEN
					OPS.Get(sym); Expression(y); OPB.Assign(x, y)
				ELSIF sym = eql THEN
					err(becomes); OPS.Get(sym); Expression(y); OPB.Assign(x, y)
				ELSIF (x^.class = Nproc) & (x^.obj^.mode = SProc) THEN
					StandProcCall(x);
					IF (x # NIL) & (x^.typ # OPT.notyp) THEN err(55) END
				ELSE OPB.PrepCall(x, fpar);
					IF sym = lparen THEN
						OPS.Get(sym); ActualParameters(apar, fpar); CheckSym(rparen)
					ELSE apar := NIL;
						IF fpar # NIL THEN err(65) END
					END ;
					OPB.Call(x, apar, fpar);
					IF x^.typ # OPT.notyp THEN err(55) END ;
					IF level > 0 THEN OPT.topScope^.link^.leaf := FALSE END
				END ;
				pos := OPM.errpos
			ELSIF sym = if THEN
				OPS.Get(sym); Expression(x); CheckBool(x); CheckSym(then); StatSeq(y);
				OPB.Construct(Nif, x, y); SetPos(x); lastif := x;
				WHILE sym = elsif DO
					OPS.Get(sym); Expression(y); CheckBool(y); CheckSym(then); StatSeq(z);
					OPB.Construct(Nif, y, z); SetPos(y); OPB.Link(x, lastif, y)
				END ;
				IF sym = else THEN OPS.Get(sym); StatSeq(y) ELSE y := NIL END ;
				OPB.Construct(Nifelse, x, y); CheckSym(end); OPB.OptIf(x); pos := OPM.errpos
			ELSIF sym = case THEN
				OPS.Get(sym); CasePart(x); CheckSym(end)
			ELSIF sym = while THEN
				OPS.Get(sym); Expression(x); CheckBool(x); CheckSym(do); StatSeq(y);
				OPB.Construct(Nwhile, x, y); CheckSym(end)
			ELSIF sym = repeat THEN
				OPS.Get(sym); StatSeq(x);
				IF sym = until THEN OPS.Get(sym); Expression(y); CheckBool(y)
				ELSE err(until)
				END ;
				OPB.Construct(Nrepeat, x, y)
			ELSIF sym = for THEN
				CASE OPM.ForN OF 1: For1Extended | 2: For2Experimental ELSE For0Original END
			ELSIF sym = loop THEN
				OPS.Get(sym); INC(LoopLevel); StatSeq(x); DEC(LoopLevel);
				OPB.Construct(Nloop, x, NIL); CheckSym(end); pos := OPM.errpos
			ELSIF sym = with THEN
				OPS.Get(sym); idtyp := NIL; x := NIL;
				LOOP
					IF sym = ident THEN
						qualident(id); y := OPB.NewLeaf(id);
						IF (id # NIL) & (id^.typ^.form = Pointer) & ((id^.mode = VarPar) OR ~id^.leaf) THEN
							err(245)	(* jt: do not allow WITH on non-local pointers *)
						END ;
						CheckSym(colon);
						IF sym = ident THEN qualident(t);
							IF t^.mode = Typ THEN
								IF id # NIL THEN
									idtyp := id^.typ; OPB.TypTest(y, t, FALSE); id^.typ := t^.typ
								ELSE err(130)
								END
							ELSE err(52)
							END
						ELSE err(ident)
						END
					ELSE err(ident)
					END ;
					pos := OPM.errpos; CheckSym(do); StatSeq(s); OPB.Construct(Nif, y, s); SetPos(y);
					IF idtyp # NIL THEN id^.typ := idtyp; idtyp := NIL END ;
					IF x = NIL THEN x := y; lastif := x ELSE OPB.Link(x, lastif, y) END ;
					IF sym = bar THEN OPS.Get(sym) ELSE EXIT END
				END;
				e := sym = else;
				IF e THEN OPS.Get(sym); StatSeq(s) ELSE s := NIL END ;
				OPB.Construct(Nwith, x, s); CheckSym(end);
				IF e THEN x^.subcl := 1 END
			ELSIF sym = exit THEN
				OPS.Get(sym);
				IF LoopLevel = 0 THEN err(46) END ;
				OPB.Construct(Nexit, x, NIL);
				pos := OPM.errpos
			ELSIF sym = return THEN OPS.Get(sym);
				IF sym < semicolon THEN Expression(x) END ;
				IF level > 0 THEN OPB.Return(x, OPT.topScope^.link)
				ELSE (* not standard Oberon *) OPB.Return(x, NIL)
				END ;
				pos := OPM.errpos
			END ;
			IF x # NIL THEN SetPos(x); OPB.Link(stat, last, x) END ;
			IF sym = semicolon THEN OPS.Get(sym)
			ELSIF (sym <= ident) OR (if <= sym) & (sym <= return) THEN err(semicolon)
			ELSE EXIT
			END
		END
	END StatSeq;

	PROCEDURE Block(VAR procdec, statseq: OPT.Node);
		VAR typ: OPT.Struct;
			obj, first, last: OPT.Object;
			x, lastdec: OPT.Node;
			i: SHORTINT;

	BEGIN first := NIL; last := NIL; nofFwdPtr := 0;
		LOOP
			IF sym = const THEN
				OPS.Get(sym);
				WHILE sym = ident DO
					OPT.Insert(OPS.name, obj); CheckMark(obj^.vis);
					obj^.typ := OPT.sinttyp; obj^.mode := Var;	(* Var to avoid recursive definition *)
					IF sym = eql THEN
						OPS.Get(sym);
						ConstExpression(x)
					ELSIF sym = becomes THEN
						err(eql); OPS.Get(sym); ConstExpression(x)
					ELSE err(eql); x := OPB.NewIntConst(1)
					END ;
					obj^.mode := Con; obj^.typ := x^.typ; obj^.conval := x^.conval; (* ConstDesc ist not copied *)
					IF obj^.conval^.arr # NIL THEN  (* константный массив *)
						obj^.mode := VarPar; (* преобразуем в переменную-параметр *)
						obj^.vis := inPar; (* причём входной параметр*)
						IF x^.obj # NIL THEN obj^.typ :=  x^.obj^.typ END;
						obj^.typ^.pvused := TRUE; (* не знаю, нужно ли это *)
						IF last = NIL THEN OPT.topScope^.scope := obj ELSE last^.link := obj END ;
						last := obj;
						first := NIL;
					END;
					CheckSym(semicolon)
				END
			END;
			IF sym = type THEN
				OPS.Get(sym);
				WHILE sym = ident DO
					OPT.Insert(OPS.name, obj); obj^.mode := Typ; obj^.typ := OPT.undftyp;
					CheckMark(obj^.vis);
					IF sym = eql THEN
						OPS.Get(sym); TypeDecl(obj^.typ, obj^.typ)
					ELSIF (sym = becomes) OR (sym = colon) THEN
						err(eql); OPS.Get(sym); TypeDecl(obj^.typ, obj^.typ)
					ELSE err(eql)
					END ;
					IF obj^.typ^.strobj = NIL THEN obj^.typ^.strobj := obj END ;
					IF obj^.typ^.comp IN {Record, Array, DynArr} THEN
						i := 0;
						WHILE i < nofFwdPtr DO typ := FwdPtr[i]; INC(i);
							IF typ^.link^.name = obj^.name THEN typ^.BaseTyp := obj^.typ; typ^.link^.name := "" END
						END
					END ;
					CheckSym(semicolon)
				END
			END ;
			IF sym = var THEN
				OPS.Get(sym);
				WHILE sym = ident DO
					LOOP
						IF sym = ident THEN
							OPT.Insert(OPS.name, obj); CheckMark(obj^.vis);
							obj^.mode := Var; obj^.link := NIL; obj^.leaf := obj^.vis = internal; obj^.typ := OPT.undftyp;
							IF first = NIL THEN first := obj END ;
							IF last = NIL THEN OPT.topScope^.scope := obj ELSE last^.link := obj END ;
							last := obj
						ELSE err(ident)
						END ;
						IF sym = comma THEN OPS.Get(sym)
						ELSIF sym = ident THEN err(comma)
						ELSE EXIT
						END
					END ;
					CheckSym(colon); Type(typ, OPT.notyp);
					typ^.pvused := TRUE;
					IF typ^.comp = DynArr THEN typ := OPT.undftyp; err(88) END ;
					WHILE first # NIL DO first^.typ := typ; first := first^.link END ;
					CheckSym(semicolon)
				END
			END ;
			IF (sym < const) OR (sym > var) THEN EXIT END ;
		END ;
		i := 0;
		WHILE i < nofFwdPtr DO
			IF FwdPtr[i]^.link^.name # "" THEN err(128) END ;
			FwdPtr[i] := NIL;	(* garbage collection *)
			INC(i)
		END ;
		OPT.topScope^.adr := OPM.errpos;
		procdec := NIL; lastdec := NIL;
		WHILE sym = procedure DO
			OPS.Get(sym); ProcedureDeclaration(x);
			IF x # NIL THEN
				IF lastdec = NIL THEN procdec := x ELSE lastdec^.link := x END ;
				lastdec := x
			END ;
			CheckSym(semicolon)
		END ;
		IF sym = begin THEN OPS.Get(sym); StatSeq(statseq)
		ELSE statseq := NIL
		END ;
		IF (level = 0) & (TDinit # NIL) THEN
			lastTDinit^.link := statseq; statseq := TDinit
		END ;
		CheckSym(end)
	END Block;

	PROCEDURE Module*(VAR prog: OPT.Node; opt: SET);
		VAR impName, aliasName: OPS.Name;
				procdec, statseq: OPT.Node;
				c: INTEGER; done: BOOLEAN;
	BEGIN
		OPS.Init; LoopLevel := 0; level := 0; OPS.Get(sym);
		IF sym = module THEN OPS.Get(sym) ELSE err(16) END ;
		IF sym = ident THEN
			OPM.LogW(" "); OPM.LogWStr(OPS.name);
			OPT.Init(OPS.name, opt); OPS.Get(sym); CheckSym(semicolon);
			IF sym = import THEN OPS.Get(sym);
				LOOP
					IF sym = ident THEN
						aliasName := OPS.name$; impName := aliasName$; OPS.Get(sym);
						IF sym = becomes THEN OPS.Get(sym);
							IF sym = ident THEN impName := OPS.name$; OPS.Get(sym) ELSE err(ident) END
						END ;
						OPT.Import(aliasName, impName, done)
					ELSE err(ident)
					END ;
					IF sym = comma THEN OPS.Get(sym)
					ELSIF sym = ident THEN err(comma)
					ELSE EXIT
					END
				END ;
				CheckSym(semicolon)
			END ;
			IF OPM.noerr THEN TDinit := NIL; lastTDinit := NIL; c := OPM.errpos;
				Block(procdec, statseq); OPB.Enter(procdec, statseq, NIL); prog := procdec;
				prog^.conval := OPT.NewConst(); prog^.conval^.intval := c;
				IF sym = ident THEN
					IF OPS.name # OPT.SelfName THEN err(4) END ;
					OPS.Get(sym)
				ELSE err(ident)
				END ;
				IF sym # period THEN err(period) END
			END
		ELSE err(ident)
		END ;
		TDinit := NIL; lastTDinit := NIL
	END Module;

END OfrontOPP.
