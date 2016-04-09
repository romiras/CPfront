MODULE OfrontOPM;	(* RC 6.3.89 / 28.6.89, J.Templ 10.7.89 / 8.5.95*)
(* constants needed for C code generation *)
(*
2002-04-11  jt: Number changed for BlackBox 1.4: SHORT(-e)
2002-08-15  jt: Scan: path separator changed from blank to semicolon
2002-08-21  jt: CloseFiles: inC tested for NIL
*)

	IMPORT SYSTEM, Strings, Files, Models, Out, Stores, TextControllers, TextModels, TextMappers, 
		TextViews, Views, Dialog, Fonts, Ports, Controllers, Properties, DevMarkers;

	CONST
		OptionChar* = "/";

		(* compiler options; don't change the encoding *)
		inxchk* = 0;	(* index check on *)
		vcpp* = 1;	(* VC++ support on; former ovflchk; neither used nor documented *)
		ranchk* = 2;	(* range check on *)
		typchk* = 3;	(* type check on *)
		newsf* = 4;	(* generation of new symbol file allowed *)
		ptrinit* = 5;	(* pointer initialization *)
		ansi* = 6;	(* ANSI or K&R style prototypes *)
		assert* = 7;	(* assert evaluation *)
		include0* = 8;	(* include M.h0 in header file and M.c0 in body file if such files exist *)
		extsf* = 9;	(* extension of old symbol file allowed *)
		mainprog* = 10;	(* translate module body into C main function *)
		lineno* = 11;	(* emit line numbers rather than text positions in error messages *)
		defopt* = {inxchk, typchk, ptrinit, assert};	(* default options *)

		nilval* = 0;
(*
		MinRealPat = 0FF7FFFFFH;	(* most  negative, 32-bit pattern, -3.40282346E38 *)
		MinLRealPatL = 0FFFFFFFFH;	(* most  negative, lower 32-bit pattern *)
		MinLRealPatH = 0FFEFFFFFH;	(* most  negative, higher 32-bit pattern *)
		MaxRealPat = 7F7FFFFFH; (*3.40282346E38*)
		MaxLRealPatL = -1;
		MaxLRealPatH = 7FEFFFFFH;
*)

		MaxRExp* = 38;	MaxLExp* = 308;	MaxHDig* = 8;

		MinHaltNr* = 0;
		MaxHaltNr* = 255;
		MaxSysFlag* = 1;

		MaxCC* = -1;	(* SYSTEM.CC, GETREG, PUTREG; not implementable in Ofront *)
		MinRegNr* = 0;
		MaxRegNr* = -1;

		LANotAlloc* = -1;	(* XProc link adr initialization *)
		ConstNotAlloc* = -1;	(* for allocation of string and real constants *)
		TDAdrUndef* = -1;	(* no type desc allocated *)

		MaxCases* = 128;
		MaxCaseRange* = 512;

		MaxStruct* = 255;

		(* maximal number of pointer fields in a record: *)
		MaxPtr* = MAX(INTEGER);

		(* maximal number of global pointers per module: *)
		MaxGPtr* = MAX(INTEGER);

		(* maximal number of hidden fields in an exported record: *)
		MaxHdFld* = 512;
		
		HdPtrName* = "@ptr";
		HdProcName* = "@proc";
		HdTProcName* = "@tproc";

		ExpHdPtrFld* = TRUE;
		ExpHdProcFld* = FALSE;
		ExpHdTProc* = FALSE;

		NEWusingAdr* = FALSE;

		Eot* = 0X;

		SFext = ".sym";	(* symbol file extension *)
		BFext = ".c";	(* body file extension *)
		HFext = ".h";	(* header file extension *)
		SFtag = 0F7X;	(* symbol file tag *)

		HeaderFile* = 0;
		BodyFile* = 1;
		HeaderInclude* = 2;

	TYPE
		FileName = ARRAY 32 OF CHAR;
		
		BYTEPTR = POINTER TO ARRAY MAX(INTEGER) OF BYTE;

	VAR
		ByteSize*, CharSize*, BoolSize*, SIntSize*, IntSize*,
		LIntSize*, SetSize*, RealSize*, LRealSize*, PointerSize*, ProcSize*, RecSize*,
		CharAlign*, BoolAlign*, SIntAlign*, IntAlign*,
		LIntAlign*, SetAlign*, RealAlign*, LRealAlign*, PointerAlign*, ProcAlign*, RecAlign*,
		ByteOrder*, BitOrder*, MaxSet*: INTEGER;
		MinSInt*, MinInt*, MinLInt*, MaxSInt*, MaxInt*, MaxLInt*, MaxIndex*: INTEGER;
		MinReal*, MaxReal*, MinLReal*, MaxLReal*: REAL;

		noerr*: BOOLEAN;
		curpos*, errpos*: INTEGER;	(* character and error position in source file *)
		breakpc*: INTEGER;	(* set by OPV.Init *)
		currFile*: INTEGER;	(* current output file *)
		level*: INTEGER;	(* procedure nesting level *)
		pc*, entno*: INTEGER;  (* entry number *)
		modName*: ARRAY 32 OF SHORTCHAR;
		objname*: ARRAY 64 OF SHORTCHAR;

		opt*, glbopt*: SET;

		lasterrpos: INTEGER;
		inR: TextModels.Reader;
		inC: TextControllers.Controller; 
		script: Stores.Operation; (* for undoing insert error marks *)
		inS: TextMappers.Scanner;
		inV: Views.View; 
		inLoc: Files.Locator;
		inName: Files.Name;
		end: INTEGER;
		translateList: BOOLEAN;
		oldSF: Stores.Reader; newSF: Stores.Writer;
		R: ARRAY 3 OF Files.Writer;
		oldSFile, newSFile, HFile, BFile, HIFile: Files.File;
		stop, useLineNo: BOOLEAN;	(*useLineNo not supported under Windows *)

		options*: RECORD
			mainprog*, newsf*, extsf*, ansii*, include0*, vcpp*, inxchk*, assert*, ranchk*, typchk*, ptrinit*: BOOLEAN;
			dirs*: ARRAY 256 OF CHAR;
		END ;

		locate*: RECORD
			num*: INTEGER
		END ;

	(* -------------------------  lookup with search path ------------------------- *)

	PROCEDURE Scan(VAR dirs, dir: ARRAY OF CHAR; VAR pos: INTEGER);
		VAR i: INTEGER; ch: CHAR;
	BEGIN ch := dirs[pos]; i := 0;
		WHILE ch = ';' DO INC(pos); ch := dirs[pos] END ;
		WHILE (ch # 0X) & (ch # ';') DO dir[i] := ch; INC(i); INC(pos); ch := dirs[pos] END ;
		dir[i] := 0X
	END Scan;

	PROCEDURE FilesNew(): Files.File;
		VAR dirs, dir: ARRAY 256 OF CHAR; pos: INTEGER;
	BEGIN
		Dialog.MapString(options.dirs, dirs);
		pos := 0; Scan(dirs, dir, pos);
		RETURN Files.dir.New(Files.dir.This(dir), Files.ask)
	END FilesNew;

	PROCEDURE FilesOld(name: Files.Name): Files.File;
		VAR dirs, dir: ARRAY 256 OF CHAR; pos: INTEGER; F: Files.File;
	BEGIN
		Dialog.MapString(options.dirs, dirs);
		pos := 0; Scan(dirs, dir, pos);
		REPEAT F := Files.dir.Old(Files.dir.This(dir), name, Files.shared); Scan(dirs, dir, pos) UNTIL (F # NIL) OR (dir = "");
		RETURN F
	END FilesOld;

	PROCEDURE FilesDelete(name: Files.Name);
		VAR dirs, dir: ARRAY 256 OF CHAR; pos: INTEGER;
	BEGIN
		Dialog.MapString(options.dirs, dirs);
		pos := 0; Scan(dirs, dir, pos);
		Files.dir.Delete(Files.dir.This(dir), name)
	END FilesDelete;

	PROCEDURE ViewsOld(name: ARRAY OF CHAR): Views.View;
		VAR dirs, dir: ARRAY 256 OF CHAR; pos: INTEGER;
	BEGIN
		Dialog.MapString(options.dirs, dirs);
		pos := 0; Scan(dirs, dir, pos);
		inName := name$;
		REPEAT
			inLoc := Files.dir.This(dir);
			inV := Views.OldView(inLoc, inName);
			Scan(dirs, dir, pos)
		UNTIL (inV # NIL) OR (dir = "");
		RETURN inV
	END ViewsOld;

	(* ------------------------- Log Output ------------------------- *)

	PROCEDURE LogW*(ch: CHAR);
	BEGIN
		Out.Char(ch)
	END LogW;

	PROCEDURE LogWStr*(s: ARRAY OF SHORTCHAR);
	BEGIN
		Out.String(LONG(s));
	END LogWStr;
	
	PROCEDURE LogWNum*(i, len: INTEGER);
	BEGIN
		Out.Int(i, len)
	END LogWNum;

	PROCEDURE LogWLn*;
	BEGIN
		Out.Ln
	END LogWLn;


	(* ------------------------- parameter handling -------------------------*)

	PROCEDURE ScanOptions(VAR s: ARRAY OF CHAR; VAR opt: SET);
		VAR i: INTEGER;
	BEGIN i := 0; 
		WHILE s[i] # 0X DO
			CASE s[i] OF
			| "e": opt := opt / {extsf}
			| "s": opt := opt / {newsf}
			| "m": opt := opt / {mainprog}
			| "x": opt := opt / {inxchk}
			| "v": opt := opt / {vcpp, ansi};
			| "r": opt := opt / {ranchk}
			| "t": opt := opt / {typchk}
			| "a": opt := opt / {assert}
			| "k": opt := opt / {ansi}
			| "p": opt := opt / {ptrinit}
			| "i": opt := opt / {include0}
			| "l": opt := opt / {lineno}
			ELSE LogWStr("  warning: option "); LogW(OptionChar); LogW(s[i]); LogWStr(" ignored"); LogWLn
			END ;
			INC(i)
		END
	END ScanOptions;

	PROCEDURE ^GetProperties;

	PROCEDURE OpenPar*(list: BOOLEAN);	(* prepare for a sequence of translations *)
		VAR ch: CHAR; pos, beg: INTEGER; c: TextControllers.Controller;
	BEGIN
		GetProperties;
		translateList := list; stop := FALSE;
		c := TextControllers.Focus();  (* get the focus controller, if any *)
		IF c # NIL THEN
			IF translateList THEN
				IF c.HasSelection() THEN c.GetSelection(beg, end);
					inS.ConnectTo(c.text); inS.SetPos(beg); inS.Scan;
					IF (inS.type = TextMappers.char) & (inS.char = "/") THEN
						pos := inS.start; glbopt := defopt; inS.Scan;
						IF (inS.start = pos + 1) & (inS.type = TextMappers.string) THEN 
							ScanOptions(inS.string, glbopt); inS.Scan
						END
					END
				ELSE stop := TRUE
				END
			ELSE
				glbopt := {};
				IF options.mainprog THEN INCL(glbopt, mainprog) END ;
				IF options.extsf THEN INCL(glbopt, extsf) END ;
				IF options.newsf THEN INCL(glbopt, newsf) END ;
				IF options.vcpp THEN glbopt := glbopt + {vcpp, ansi} END ;
				IF options.include0 THEN INCL(glbopt, include0) END ;
				IF options.ansii THEN INCL(glbopt, ansi) END ;
				IF options.inxchk THEN INCL(glbopt, inxchk) END ;
				IF options.assert THEN INCL(glbopt, assert) END ;
				IF options.ptrinit THEN INCL(glbopt, ptrinit) END ;
				IF options.ranchk THEN INCL(glbopt, ranchk) END ;
				IF options.typchk THEN INCL(glbopt, typchk) END ;
			END
		ELSE stop := TRUE
		END ;
	END OpenPar;

	PROCEDURE InitOptions*;	(* get the options for one translation *)
		VAR pos: INTEGER;
	BEGIN
		opt := glbopt;
		IF translateList & (inS.start < end) & (inS.type = TextMappers.char) & (inS.char = "/") THEN
			pos := inS.start; glbopt := defopt; inS.Scan;
			IF (inS.start = pos + 1) & (inS.type = TextMappers.string) THEN 
				ScanOptions(inS.string, opt); inS.Scan
			END
		END;
		IF lineno IN opt THEN useLineNo := TRUE; curpos := 256; errpos := curpos; lasterrpos := curpos - 10
		ELSE useLineNo := FALSE;
		END
	END InitOptions;

	PROCEDURE Init*(VAR done: BOOLEAN);	(* get the source for one translation *)
		VAR v: Views.View; m: Models.Model;
			source: TextModels.Model; FName: Files.Name;
			E: Views.View; pos: INTEGER;
	BEGIN
		done := FALSE; curpos := 0;
		IF stop THEN RETURN END;
		IF translateList THEN
			IF (inS.start < end) & (inS.type = TextMappers.string) THEN
				LogWStr(SHORT(inS.string));
				v := ViewsOld(inS.string);
				IF v # NIL THEN
					m := v.ThisModel();
					IF (m # NIL) & (m IS TextModels.Model) THEN
						LogWStr("  translating");
						source := m(TextModels.Model);
						inR := source.NewReader(inR); inR.SetPos(0);
						done := TRUE; inS.Scan
					ELSE LogWStr(" not a text document"); LogWLn
					END
				ELSE LogWStr(" not found"); LogWLn
				END
			END
		ELSE
			inC := TextControllers.Focus();  (* get the focus controller, if any *)
			IF inC # NIL THEN  (* a text view must be focus *)
				LogWStr("translating");
				inR := inC.text.NewReader(inR); inR.SetPos(0); inR.ReadView(E);
				WHILE E # NIL DO pos := inR.Pos();
					IF E IS DevMarkers.View THEN
						inC.text.Delete(pos - 1, pos);
						inR.SetPos(pos-1)
					END;
					inR.ReadView(E)
				END ;
				inR.SetPos(0);
				Models.BeginScript(inC.text, "#ofront:insertMarkers", script);
				done := TRUE; stop := TRUE
			END
		END ;
		level := 0; noerr := TRUE; errpos := curpos; lasterrpos := curpos - 10
	END Init;

	PROCEDURE AppInt(x, pos: INTEGER; VAR s: ARRAY OF CHAR);
	BEGIN
		IF x > 100 THEN s[pos] := CHR(ORD("0") + x DIV 100); INC(pos) END ;
		IF x > 10 THEN s[pos] := CHR(ORD("0") + x MOD 100 DIV 10); INC(pos) END ;
		s[pos] := CHR(ORD("0") + x MOD 10); INC(pos); s[pos] := 0X
	END AppInt;

	
	PROCEDURE LocatePos*;
		VAR C: TextControllers.Controller; pos: INTEGER;
	BEGIN
		C := TextControllers.Focus();
		IF C = NIL THEN RETURN END ;
		IF locate.num < 0 THEN pos := 0; Dialog.Beep
		ELSIF locate.num > C.text.Length() THEN pos := C.text.Length(); Dialog.Beep
		ELSE pos := locate.num
		END ;
		C.view.ShowRange(pos, pos, FALSE);
		C.SetCaret(pos)
	END LocatePos;

	PROCEDURE LocateLine*;
		VAR C: TextControllers.Controller; R: TextModels.Reader; ch: CHAR; line, pos: INTEGER;
	BEGIN
		C := TextControllers.Focus();
		IF C = NIL THEN RETURN END ;
		R := C.text.NewReader(NIL);
		R.SetPos(0); line := 0;
		REPEAT
			R.ReadChar(ch);
			IF ch = TextModels.line THEN INC(line) END
		UNTIL R.eot OR (line = locate.num);
		IF R.eot THEN pos := C.text.Length(); Dialog.Beep
		ELSIF locate.num < 0 THEN pos := 0; Dialog.Beep
		ELSE pos := R.Pos() - 1
		END ;
		C.view.ShowRange(pos, pos, FALSE);
		C.SetCaret(pos)
	END LocateLine;


	(* ------------------------- read source text -------------------------*)
	
	PROCEDURE Get*(VAR ch: SHORTCHAR);	(* read next character from source text, 0X if eof *)
	BEGIN 
		inR.Read();
		IF inR.eot THEN ch := 0X
		ELSE ch := SHORT(inR.char); INC(curpos)
		END
	END Get;
	
	PROCEDURE MakeFileName(IN name: ARRAY OF SHORTCHAR; VAR FName: Files.Name; ext: ARRAY OF CHAR);
		VAR i, j: INTEGER; ch: CHAR;
	BEGIN i := 0;
		LOOP ch := name[i];
			IF ch = 0X THEN EXIT END ;
			FName[i] := ch; INC(i)
		END ;
		j := 0;
		REPEAT ch := ext[j]; FName[i] := ch; INC(i); INC(j)
		UNTIL ch = 0X
	END MakeFileName;

	PROCEDURE Mark*(n: INTEGER; pos: INTEGER);
		VAR E: DevMarkers.View; T: TextModels.Model; W: TextModels.Writer; s: ARRAY 256 OF CHAR;
	BEGIN
		IF n >= 0 THEN
			IF pos < 0 THEN pos := 0 END ;
			IF (pos < lasterrpos) OR (lasterrpos + 9 < pos) THEN lasterrpos := pos;
				IF n < 249 THEN
					E := DevMarkers.dir.New(DevMarkers.message);
					Strings.IntToString(n, s); Dialog.MapString("#ofront:"+s, s);
					E.InitErr(n); E.InitMsg(s);
					T := inR.Base(); W := T.NewWriter(NIL); W.SetPos(pos);
					W.WriteView(E, 10*Ports.point, 10 * Ports.point);
					INC(curpos); inR.SetPos(curpos); 
					IF noerr THEN
						IF translateList THEN Views.Open(inV, inLoc, inName, NIL);
							inV(TextViews.View).ShowRange(pos, pos, FALSE)
						ELSE inC.view.ShowRange(pos, pos, FALSE)
						END
					END
				ELSE 
					s := "#ofront:"; AppInt(n, 8, s);
					Dialog.MapParamString(s, LONG(objname), "", "", s);
					LogWLn; LogWStr("  "); LogWStr(SHORT(s))
				END
			END ;
			noerr := FALSE
		ELSE
			IF pos >= 0 THEN LogWLn; LogWStr("  pos"); LogWNum(pos, 6) END ;
			LogWStr("  warning  "); s := "#ofront:"; AppInt(-n, 8, s);
			Dialog.MapString(s, s); LogWStr('"'); LogWStr(SHORT(s)); LogWStr('"');
			IF pos < 0 THEN LogWLn END
		END
	END Mark;


	PROCEDURE err*(n: INTEGER);
	BEGIN Mark(n, errpos)
	END err;

	PROCEDURE FPrint*(VAR fp: INTEGER; val: INTEGER);
	BEGIN
		fp := SYSTEM.ROT(SYSTEM.VAL(INTEGER, SYSTEM.VAL(SET, fp) / SYSTEM.VAL(SET, val)), 1)
	END FPrint;

	PROCEDURE FPrintSet*(VAR fp: INTEGER; set: SET);
	BEGIN FPrint(fp, SYSTEM.VAL(INTEGER, set))
	END FPrintSet;

	PROCEDURE FPrintReal*(VAR fp: INTEGER; real: SHORTREAL);
	BEGIN FPrint(fp, SYSTEM.VAL(INTEGER, real))
	END FPrintReal;

	PROCEDURE FPrintLReal*(VAR fp: INTEGER; lr: REAL);
		VAR l, h: INTEGER;
	BEGIN
		SYSTEM.GET(SYSTEM.ADR(lr), l); SYSTEM.GET(SYSTEM.ADR(lr)+4, h);
		FPrint(fp, l); FPrint(fp, h)
	END FPrintLReal;

	(* ------------------------- initialization ------------------------- *)

	PROCEDURE GetProperty(R: Files.Reader; name: ARRAY OF CHAR; VAR size, align: INTEGER);
		VAR ch: SHORTCHAR; i: INTEGER; s: ARRAY 32 OF CHAR;
	BEGIN
		ch := " "; i := 0;
		WHILE (~R.eof) & (ch <= " ") DO R.ReadByte(SYSTEM.VAL(BYTE, ch)) END ;
		WHILE (~R.eof) & (ch >" ") DO s[i] := ch; INC(i); R.ReadByte(SYSTEM.VAL(BYTE, ch)) END ;
		s[i] := 0X;
		IF s = name THEN i := 0;
			WHILE (~R.eof) & (ch <= " ") DO R.ReadByte(SYSTEM.VAL(BYTE, ch)) END ;
			IF (~R.eof) & (ch >= "0") & (ch <= "9") THEN
				WHILE (~R.eof) & (ch >= "0") & (ch <= "9") DO i := i * 10 + ORD(ch) - ORD("0"); R.ReadByte(SYSTEM.VAL(BYTE, ch)) END ;
				size := i; i := 0;
				WHILE (~R.eof) & (ch <= " ") DO R.ReadByte(SYSTEM.VAL(BYTE, ch)) END ;
				IF (~R.eof) & (ch >= "0") & (ch <= "9") THEN
					WHILE (~R.eof) & (ch >= "0") & (ch <= "9") DO i := i * 10 + ORD(ch) - ORD("0"); R.ReadByte(SYSTEM.VAL(BYTE, ch)) END ;
					align := i;
				ELSE Mark(-157, -1)
				END
			ELSE Mark(-157, -1)
			END
		ELSE Mark(-157, -1)
		END
	END GetProperty;

	PROCEDURE GetProperties();
		VAR F: Files.File; R: Files.Reader;
	BEGIN
		(* default characteristics *)
		ByteSize := 1; CharSize := 1; BoolSize := 1; SIntSize := 1; IntSize := 2; LIntSize := 4;
		SetSize := 4; RealSize := 4; LRealSize := 8; ProcSize := 4; PointerSize := 4; RecSize := 1;
		CharAlign := 1; BoolAlign := 1; SIntAlign := 1; IntAlign := 2; LIntAlign := 4;
		SetAlign := 4; RealAlign := 4; LRealAlign := 8; ProcAlign := 4; PointerAlign := 4; RecAlign := 1;
		MinSInt := -80H; MinInt := -8000H; MinLInt :=  80000000H;	(*-2147483648*)
		MaxSInt := 7FH; MaxInt := 7FFFH; MaxLInt := 7FFFFFFFH;	(*2147483647*)
		MaxSet := 31;
		(* read Ofront.par *)
		F := FilesOld("Ofront.par");
		IF F # NIL THEN
			R := F.NewReader(NIL);
			R.SetPos(0);
			GetProperty(R, "CHAR", CharSize, CharAlign);
			GetProperty(R, "BOOLEAN", BoolSize, BoolAlign);
			GetProperty(R, "SHORTINT", SIntSize, SIntAlign);
			GetProperty(R, "INTEGER", IntSize, IntAlign);
			GetProperty(R, "LONGINT", LIntSize, LIntAlign);
			GetProperty(R, "SET", SetSize, SetAlign);
			GetProperty(R, "REAL", RealSize, RealAlign);
			GetProperty(R, "LONGREAL", LRealSize, LRealAlign);
			GetProperty(R, "PTR", PointerSize, PointerAlign);
			GetProperty(R, "PROC", ProcSize, ProcAlign);
			GetProperty(R, "RECORD", RecSize, RecAlign);
			(* Size = 0: natural size aligned to next power of 2 up to RecAlign; e.g. i960
				Size = 1; size and alignment follows from field types but at least RecAlign; e.g, SPARC, MIPS, PowerPC
			*)
			GetProperty(R, "ENDIAN", ByteOrder, BitOrder);	(*currently not used*)
		ELSE Mark(-156, -1)
		END ;
		IF RealSize = 4 THEN MaxReal := 3.40282346E38
		ELSIF RealSize = 8 THEN MaxReal := 1.7976931348623157E307 * 9.999999
			(*should be 1.7976931348623157D308 *)
		END ;
		IF LRealSize = 4 THEN MaxLReal := 3.40282346E38
		ELSIF LRealSize = 8 THEN MaxLReal := 1.7976931348623157E307 * 9.999999
			(*should be 1.7976931348623157D308 *)
		END ;
		MinReal := -MaxReal;
		MinLReal := -MaxLReal;
		IF IntSize = 4 THEN MinInt := MinLInt; MaxInt := MaxLInt END ;
		MaxSet := SetSize * 8 - 1;
		MaxIndex := MaxLInt
	END GetProperties;

	(* ------------------------- Read Symbol File ------------------------- *)

	PROCEDURE SymRCh*(VAR ch: SHORTCHAR);
	BEGIN
		oldSF.ReadSChar(ch)
	END SymRCh;
	
	PROCEDURE SymRInt* (): INTEGER;
		VAR s, byte: BYTE; ch: CHAR; n, x: INTEGER;
	BEGIN s := 0; n := 0; oldSF.ReadByte(byte); ch := SHORT(CHR(byte));
		WHILE ORD(ch) >= 128 DO INC(n, ASH(ORD(ch) - 128, s) ); INC(s, 7); 
			oldSF.ReadByte(byte); ch := SHORT(CHR(byte))
		END;
		INC(n, ASH(ORD(ch) MOD 64 - ORD(ch) DIV 64 * 64, s) );
		RETURN n
	END SymRInt;

	PROCEDURE SymRSet*(VAR s: SET);
	BEGIN
		s := SYSTEM.VAL(SET, SymRInt())
	END SymRSet;
	
	PROCEDURE SymRReal*(VAR r: SHORTREAL);
	BEGIN
		oldSF.ReadSReal(r)
	END SymRReal;
	
	PROCEDURE SymRLReal*(VAR lr: REAL);
	BEGIN
		oldSF.ReadReal(lr)
	END SymRLReal;
	
	PROCEDURE CloseOldSym*;
	BEGIN oldSFile.Close; oldSFile := NIL; oldSF.ConnectTo(NIL)
	END CloseOldSym;

	PROCEDURE OldSym*(VAR modName: ARRAY OF SHORTCHAR; VAR done: BOOLEAN);
		VAR ch: SHORTCHAR; fileName: Files.Name;
	BEGIN MakeFileName(modName, fileName, SFext);
		oldSFile := FilesOld(fileName); done := oldSFile # NIL;
		IF done THEN
			oldSF.ConnectTo(oldSFile); oldSF.ReadSChar(ch);
			IF ch # SFtag THEN err(-306);  (*possibly a symbol file from another Oberon implementation, e.g. HP-Oberon*)
				CloseOldSym; done := FALSE
			END
		END
	END OldSym;
	
	PROCEDURE eofSF*(): BOOLEAN;
	BEGIN 
		RETURN oldSF.rider.eof
	END eofSF;
	
	(* ------------------------- Write Symbol File ------------------------- *)
	
	PROCEDURE SymWCh*(ch: SHORTCHAR);
	BEGIN 
		newSF.WriteSChar(ch)
	END SymWCh;

	PROCEDURE SymWInt* (x: INTEGER);
	BEGIN
		WHILE (x < - 64) OR (x > 63) DO newSF.WriteByte(SHORT(SHORT(x MOD 128 + 128))); x := x DIV 128 END;
		newSF.WriteByte(SHORT(SHORT(x MOD 128)))
	END SymWInt;

	PROCEDURE SymWSet*(s: SET);
	BEGIN
		SymWInt(SYSTEM.VAL(INTEGER, s))
	END SymWSet;

	PROCEDURE SymWReal*(r: SHORTREAL);
	BEGIN
		newSF.WriteSReal(r)
	END SymWReal;
	
	PROCEDURE SymWLReal*(lr: REAL);
	BEGIN
		newSF.WriteReal(lr)
	END SymWLReal;
	
	PROCEDURE RegisterNewSym*;
		VAR fileName: Files.Name; res: INTEGER;
	BEGIN
		IF (modName # "SYSTEM") OR (mainprog IN opt) THEN 
			MakeFileName(modName, fileName, SFext);
			newSFile.Register(fileName, "*", Files.dontAsk, res)
		END
	END RegisterNewSym;
	
	PROCEDURE DeleteNewSym*;
	END DeleteNewSym;

	PROCEDURE NewSym*(VAR modName: ARRAY OF SHORTCHAR);
		VAR fileName: Files.Name;
	BEGIN MakeFileName(modName, fileName, SFext);
		newSFile := FilesNew();
		IF newSFile # NIL THEN
			newSF.ConnectTo(newSFile); newSF.WriteSChar(SFtag)
		ELSE err(153)
		END
	END NewSym;

	(* ------------------------- Write Header & Body Files ------------------------- *)

	PROCEDURE Write*(ch: SHORTCHAR);
	BEGIN
		R[currFile].WriteByte(SYSTEM.VAL(BYTE, ch))
	END Write;

	PROCEDURE WriteString*(s: ARRAY OF SHORTCHAR);
		VAR i: INTEGER; ch: SHORTCHAR;
	BEGIN i := 0; ch := s[0];
	WHILE ch # 0X DO Write(ch); INC(i); ch := s[i] END ;
(*		
		p := SYSTEM.VAL(BYTEPTR, SYSTEM.ADR(s));
		R[currFile].WriteBytes(p^, 0, LEN(s$))
*)
	END WriteString;

	PROCEDURE WriteStringVar*(VAR s: ARRAY OF SHORTCHAR);
		VAR i: INTEGER; ch: SHORTCHAR;
	BEGIN i := 0; ch := s[0];
	WHILE ch # 0X DO Write(ch); INC(i); ch := s[i] END ;
(*
		p := SYSTEM.VAL(BYTEPTR, SYSTEM.ADR(s));
		R[currFile].WriteBytes(p^, 0, LEN(s$))
*)
	END WriteStringVar;

	PROCEDURE WriteHex* (i: INTEGER);
		VAR s: ARRAY 3 OF SHORTCHAR;
			digit : INTEGER;
	BEGIN
		digit := SHORT(i) DIV 16;
		IF digit < 10 THEN s[0] := SHORT(CHR (ORD ("0") + digit)); ELSE s[0] := SHORT(CHR (ORD ("a") - 10 + digit)); END;
		digit := SHORT(i) MOD 16;
		IF digit < 10 THEN s[1] := SHORT(CHR (ORD ("0") + digit)); ELSE s[1] := SHORT(CHR (ORD ("a") - 10 + digit)); END;
		s[2] := 0X;
		WriteString(s)
	END WriteHex;
	
	PROCEDURE WriteInt* (i: INTEGER);
		VAR s: ARRAY 20 OF CHAR; i1, k: INTEGER;
	BEGIN
		IF i = MinLInt THEN Write("("); WriteInt(i+1); WriteString("-1)")	(* requires special bootstrap for 64 bit *)
		ELSE i1 := ABS(i);
			s[0] := CHR(i1 MOD 10 + ORD("0")); i1 := i1 DIV 10; k := 1;
			WHILE i1 > 0 DO s[k] := CHR(i1 MOD 10 + ORD("0")); i1 := i1 DIV 10; INC(k) END ;
			IF i < 0 THEN s[k] := "-"; INC(k) END ;
			WHILE k > 0 DO  DEC(k); Write(SHORT(s[k])) END
		END ;
	END WriteInt;

	PROCEDURE WriteReal* (r: REAL; suffx: CHAR);
		VAR s: ARRAY 32 OF CHAR; ch: CHAR; i: INTEGER;
			(* W: TextMappers.Formatter; T: TextModels.Model; R: TextModels.Reader; *)
	BEGIN
(*should be improved *)
		IF (r < MaxLInt) & (r > MinLInt) & (r = SHORT(ENTIER(r))) THEN 
			IF suffx = "f" THEN WriteString("(REAL)") ELSE WriteString("(LONGREAL)") END ;
			WriteInt(SHORT(ENTIER(r)))
		ELSE
			IF suffx = "f" THEN Strings.RealToString(SHORT(r), s) ELSE Strings.RealToString(r, s) END;
(* s[i] := suffx; s[i+1] := 0X;
suffix does not work in K&R *)
			i := 0; ch := s[0]; 
			WHILE (ch # "D") & (ch # 0X) DO INC(i); ch := s[i] END ;
			IF ch = "D" THEN s[i] := "e" END ;
			WriteString(SHORT(s))
		END
	END WriteReal;

	PROCEDURE WriteLn* ();
	BEGIN
		R[currFile].WriteByte(0DH); R[currFile].WriteByte(0AH);
	END WriteLn;

	PROCEDURE Append(VAR R: Files.Writer; F: Files.File);
		VAR R1: Files.Reader; buffer: ARRAY 4096 OF BYTE; oldPos: INTEGER;
	BEGIN
		IF F # NIL THEN
			R1 := F.NewReader(NIL); oldPos := R1.Pos(); R1.ReadBytes(buffer, 0, LEN(buffer));
			WHILE R1.Pos() # oldPos DO
				R.WriteBytes(buffer, 0, R1.Pos() - oldPos); oldPos := R1.Pos();
				R1.ReadBytes(buffer, 0, LEN(buffer))
			END
		END
	END Append;

	PROCEDURE OpenFiles*(VAR moduleName: ARRAY OF SHORTCHAR);
		VAR FName: Files.Name;
	BEGIN
		modName := moduleName$;
		HFile := FilesNew();
		IF HFile # NIL THEN R[HeaderFile] := HFile.NewWriter(NIL) ELSE err(153) END;
		MakeFileName(moduleName, FName, BFext);
		BFile := FilesNew();
		IF BFile # NIL THEN R[BodyFile] := BFile.NewWriter(NIL) ELSE err(153) END ;
		MakeFileName(moduleName, FName, HFext);
		HIFile := FilesNew();
		IF HIFile # NIL THEN R[HeaderInclude] := HIFile.NewWriter(NIL) ELSE err(153) END ;
		IF include0 IN opt THEN
			MakeFileName(moduleName, FName, ".h0"); Append(R[HeaderInclude], FilesOld(FName));
			MakeFileName(moduleName, FName, ".c0"); Append(R[BodyFile], FilesOld(FName))
		END
	END OpenFiles;

	PROCEDURE FileDiff(name: Files.Name; new: Files.File): BOOLEAN;
		VAR old: Files.File; RO, RN: Files.Reader;
			bo, bn: BYTE; i, pos: INTEGER;
			dirs, dir: ARRAY 256 OF CHAR;
	BEGIN
		Dialog.MapString(options.dirs, dirs); pos := 0; Scan(dirs, dir, pos);
		old := Files.dir.Old(Files.dir.This(dir), name, Files.shared);
		IF (old = NIL) OR (old.Length() # new.Length()) THEN RETURN TRUE END ;
		RO := old.NewReader(NIL); RN := new.NewReader(NIL);
		FOR i := 0 TO old.Length() - 1 DO
			RO.ReadByte(bo); RN.ReadByte(bn);
			IF bo # bn THEN RETURN TRUE END
		END ;
		RETURN FALSE
	END FileDiff;
	
	PROCEDURE CloseFiles*;
		VAR FName: Files.Name; res: INTEGER;
	BEGIN
		IF noerr THEN LogWStr("    "); LogWNum(R[BodyFile].Pos(), 0); Dialog.ShowStatus("#ofront:ok")
		ELSE Dialog.ShowStatus("#ofront:error")
		END;
		IF noerr THEN
			IF modName = "SYSTEM" THEN
				IF ~(mainprog IN opt) THEN MakeFileName(modName, FName, BFext); BFile.Register(FName, "*", Files.dontAsk, res) END
			ELSIF ~(mainprog IN opt) THEN
				Append(R[HeaderInclude], HFile);
				MakeFileName(modName, FName, HFext); 
				IF FileDiff(FName, HIFile) THEN HIFile.Register(FName, "*", Files.dontAsk, res) END ; 
				MakeFileName(modName, FName, BFext); BFile.Register(FName, "*", Files.dontAsk, res)
			ELSE
				MakeFileName(modName, FName, HFext); FilesDelete(FName);
				MakeFileName(modName, FName, SFext); FilesDelete(FName);
				MakeFileName(modName, FName, BFext); BFile.Register(FName, "*", Files.dontAsk, res)
			END
		END ;
		HFile := NIL; BFile := NIL; HIFile := NIL; newSFile := NIL; oldSFile := NIL;
		R[0] := NIL; R[1] := NIL; R[2] := NIL; newSF.ConnectTo(NIL); oldSF.ConnectTo(NIL);
		IF inC # NIL THEN Models.EndScript(inC.text, script); inC := NIL END
	END CloseFiles;

	PROCEDURE GetDefaultOptions;
		VAR optstr: ARRAY 32 OF CHAR; defopt: SET;
	BEGIN
		Dialog.MapString("#ofront:options", optstr);
		IF optstr = "options" THEN optstr := "xatp" END ;
		defopt := {}; ScanOptions(optstr, defopt);
		options.mainprog := mainprog IN defopt;
		options.newsf := newsf IN defopt;
		options.extsf := extsf IN defopt;
		options.ansii := (ansi IN defopt) OR (vcpp IN defopt);
		options.include0 := include0 IN defopt;
		options.ranchk := ranchk IN defopt;
		options.vcpp := vcpp IN defopt;
		options.inxchk := inxchk IN defopt;
		options.assert := assert IN defopt;
		options.typchk := typchk IN defopt;
		options.ptrinit := ptrinit IN defopt;
		Dialog.MapString( "#ofront:directories", options.dirs)
	END GetDefaultOptions;
	
	PROCEDURE PromoteIntConstToLInt*();
	BEGIN
		(* ANSI C does not need explicit promotion.
			K&R C implicitly promotes integer constants to type int in parameter lists.
			if the formal parameter, however, is of type long, appending "L" is required in ordere to promote
			the parameter explicitly to type long (if LONGINT corresponds to long, which we do not really know).
			It works for all known K&R versions of ofront and K&R is dying out anyway.
			A cleaner solution would be to cast with type (LONGINT), but this requires a bit more changes.
		*)
		IF ~(ansi IN opt) THEN Write("L") END
	END PromoteIntConstToLInt;

BEGIN
	locate.num := 0; GetDefaultOptions
END OfrontOPM.


OfrontCmd OfrontOPP OfrontOPB OfrontOPC OfrontOPV OfrontOPT OfrontOPS OfrontOPM 
OfrontBrowser
