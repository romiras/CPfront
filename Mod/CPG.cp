MODULE OfrontCPG;
(*
	C source code generator
	based on OfrontOPM by J. Templ
	9.12.99 bh
*)

	IMPORT Files, Strings, Dialog, Dates, Stores, Models, Views, TextModels, TextControllers, TextMappers, DevCPM;

	CONST
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

		HeaderFile* = 0;
		BodyFile* = 1;
		HeaderInclude* = 2;

		RecSize* = 1;
		RecAlign* = 1;

		OptionChar* = "/";
		BFext = ".c";	(* body file extension *)
		HFext = ".h";	(* header file extension *)

		maxLine = 100;	(* maximum line length in output file *)
		maxLogicLine = 30000;	(* maximum length of logic line in output file *)

	VAR
		opt*: SET;

		modName*: ARRAY 32 OF SHORTCHAR;
		HFile, BFile, HIFile: Files.File;

		R: ARRAY 3 OF Files.Writer;
		currFile*: INTEGER;	(* current output file *)
		level*: INTEGER;	(* procedure nesting level *)
		pos, logicPos: ARRAY 3 OF INTEGER;

		options*: RECORD
			mainprog*, ansii*, include0*, vcpp*: BOOLEAN;
			dirs*: ARRAY 256 OF CHAR;
		END ;

		locate*: RECORD
			num*: INTEGER
		END ;


	(* -------------------------  lookup with search path ------------------------- *)

	PROCEDURE Scan(VAR dirs, dir: ARRAY OF CHAR; VAR pos: INTEGER);
		VAR i: INTEGER; ch: CHAR;
	BEGIN ch := dirs[pos]; i := 0;
		WHILE (ch = " ") OR (ch = 9X) DO INC(pos); ch := dirs[pos] END ;
		WHILE ch > " " DO dir[i] := ch; INC(i); INC(pos); ch := dirs[pos] END ;
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


	(* ------------------------- Write Header & Body Files ------------------------- *)

	PROCEDURE WriteLn* ();
	BEGIN
		R[currFile].WriteByte(0DH); R[currFile].WriteByte(0AH);
		pos[currFile] := 0; logicPos[currFile] := 0
	END WriteLn;

	PROCEDURE Write*(ch: SHORTCHAR);
		VAR l: INTEGER;
	BEGIN
		IF (pos[currFile] > maxLine) & (ch = " ") THEN	(* break line *)
			IF logicPos[currFile] <= maxLogicLine THEN
				R[currFile].WriteByte(ORD(" "));
				R[currFile].WriteByte(ORD("\"));
				l := logicPos[currFile]; WriteLn; logicPos[currFile] := l + 4
			ELSE
				WriteLn
			END
		ELSE
			R[currFile].WriteByte(SHORT(ORD(ch)));
			INC(pos[currFile]); INC(logicPos[currFile])
		END
	END Write;

	PROCEDURE WriteOrigString*(s: ARRAY OF SHORTCHAR);
		VAR i: INTEGER; ch: SHORTCHAR;
	BEGIN i := 0; ch := s[0];
		WHILE ch # 0X DO Write(ch); INC(i); ch := s[i] END
	END WriteOrigString;

	PROCEDURE WriteString*(s: ARRAY OF SHORTCHAR);
		VAR i: INTEGER; ch: SHORTCHAR;
	BEGIN
		IF s = "CHAR" THEN WriteOrigString("_CHAR")
		ELSIF s = "BYTE" THEN WriteOrigString("_BYTE")
		ELSIF s = "BOOLEAN" THEN WriteOrigString("_BOOLEAN")
		ELSIF s = "Windows" THEN WriteOrigString("_Windows")
		ELSE
			i := 0; ch := s[0];
			WHILE ch # 0X DO
				IF ch = "^" THEN Write("_"); Write("_"); Write("r"); Write("e"); Write("c")
				ELSE Write(ch)
				END;
				INC(i); ch := s[i]
			END
		END
	END WriteString;

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

	PROCEDURE WriteHexInt* (i: INTEGER);
	BEGIN
		Write("0"); Write("x");
		WriteHex(i DIV 1000000H MOD 256);
		WriteHex(i DIV 10000H MOD 256);
		WriteHex(i DIV 100H MOD 256);
		WriteHex(i MOD 256)
	END WriteHexInt;

	PROCEDURE WriteInt* (i: INTEGER);
		VAR s: ARRAY 20 OF CHAR; i1, k: INTEGER;
	BEGIN
		IF i = MIN(INTEGER) THEN Write("("); WriteInt(i+1); WriteString("-1)")	(* requires special bootstrap for 64 bit *)
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
		IF ABS(r) > 1.0E+308 THEN Write("("); WriteReal(r / 2, suffx); WriteString(" * 2)"); RETURN END;
(*should be improved *)
		IF (r < MAX(INTEGER)) & (r > MIN(INTEGER)) & (r = SHORT(ENTIER(r))) THEN
			IF suffx = "f" THEN WriteString("(SHORTREAL)") ELSE WriteString("(REAL)") END ;
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

	PROCEDURE WriteTime* ();
		VAR  t: Dates.Time; d: Dates.Date;
	BEGIN
		Dates.GetDate(d); Dates.GetTime(t);
		WriteInt(d.year); WriteString(", ");
		WriteInt(d.month); WriteString(", ");
		WriteInt(d.day); WriteString(", ");
		WriteInt(t.hour); WriteString(", ");
		WriteInt(t.minute); WriteString(", ");
		WriteInt(t.second)
	END WriteTime;

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

	PROCEDURE MakeFileName(IN name: ARRAY OF SHORTCHAR; VAR FName: Files.Name; ext: ARRAY OF CHAR);
		VAR i, j: INTEGER; ch: CHAR;
	BEGIN
		i := 0;
		LOOP ch := name[i];
			IF ch = 0X THEN EXIT END ;
			FName[i] := ch; INC(i)
		END ;
		FName[i] := 0X;
		IF FName = "Windows" THEN FName := "_Windows"; INC(i) END;
		j := 0;
		REPEAT ch := ext[j]; FName[i] := ch; INC(i); INC(j)
		UNTIL ch = 0X;
	END MakeFileName;

	PROCEDURE OpenFiles*(VAR moduleName: ARRAY OF SHORTCHAR);
		VAR FName: Files.Name;
	BEGIN
		modName := moduleName$;
		HFile := FilesNew();
		IF HFile # NIL THEN R[HeaderFile] := HFile.NewWriter(NIL) ELSE DevCPM.err(153) END;
		MakeFileName(moduleName, FName, BFext);
		BFile := FilesNew();
		IF BFile # NIL THEN R[BodyFile] := BFile.NewWriter(NIL) ELSE DevCPM.err(153) END ;
		MakeFileName(moduleName, FName, HFext);
		HIFile := FilesNew();
		IF HIFile # NIL THEN R[HeaderInclude] := HIFile.NewWriter(NIL) ELSE DevCPM.err(153) END ;
		IF include0 IN opt THEN
			MakeFileName(moduleName, FName, ".h0"); Append(R[HeaderInclude], FilesOld(FName));
			MakeFileName(moduleName, FName, ".c0"); Append(R[BodyFile], FilesOld(FName))
		END;
		pos[HeaderFile] := 0; pos[BodyFile] := 0; pos[HeaderInclude] := 0;
		logicPos[HeaderFile] := 0; logicPos[BodyFile] := 0; logicPos[HeaderInclude] := 0
	END OpenFiles;

	PROCEDURE CloseFiles*;
		VAR FName: Files.Name; res: INTEGER;
	BEGIN
		IF DevCPM.noerr THEN DevCPM.LogWStr("    "); DevCPM.LogWNum(R[BodyFile].Pos(), 0); Dialog.ShowStatus("#Ofront:ok")
		ELSE Dialog.ShowStatus("#Ofront:error")
		END;
		IF DevCPM.noerr THEN
			IF modName = "SYSTEM" THEN
				IF ~(mainprog IN opt) THEN MakeFileName(modName, FName, BFext); BFile.Register(FName, "*", Files.ask, res) END
			ELSIF ~(mainprog IN opt) THEN
				Append(R[HeaderInclude], HFile);
				MakeFileName(modName, FName, HFext); HIFile.Register(FName, "*", Files.ask, res);
				MakeFileName(modName, FName, BFext); BFile.Register(FName, "*", Files.ask, res)
			ELSE
				MakeFileName(modName, FName, HFext); FilesDelete(FName);
				MakeFileName(modName, FName, BFext); BFile.Register(FName, "*", Files.ask, res)
			END
		END ;
		HFile := NIL; BFile := NIL; HIFile := NIL;
		R[0] := NIL; R[1] := NIL; R[2] := NIL
	END CloseFiles;

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
			ELSE
				DevCPM.LogWStr("  warning: option "); DevCPM.LogW(OptionChar);
				DevCPM.LogW(SHORT(s[i])); DevCPM.LogWStr(" ignored"); DevCPM.LogWLn
			END ;
			INC(i)
		END
	END ScanOptions;

	PROCEDURE OpenPar*;	(* prepare for a sequence of translations *)
	BEGIN
		opt := {};
		IF options.mainprog THEN INCL(opt, mainprog) END ;
		IF options.vcpp THEN opt := opt + {vcpp, ansi} END ;
		IF options.include0 THEN INCL(opt, include0) END ;
		IF options.ansii THEN INCL(opt, ansi) END ;
	END OpenPar;

	PROCEDURE GetDefaultOptions;
		VAR optstr: ARRAY 32 OF CHAR; defopt: SET;
	BEGIN
		Dialog.MapString("#Ofront:options", optstr);
		IF optstr = "options" THEN optstr := "xatp" END ;
		defopt := {}; ScanOptions(optstr, defopt);
		options.include0 := include0 IN defopt;
		options.mainprog := mainprog IN defopt;
		Dialog.MapString( "#Ofront:directories", options.dirs)
	END GetDefaultOptions;


BEGIN
	locate.num := 0; GetDefaultOptions
END OfrontCPG.

