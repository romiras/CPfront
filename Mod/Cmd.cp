MODULE OfrontCmd;	(* J. Templ 3.2.95 *)

	IMPORT
		OPP := OfrontOPP, OPB := OfrontOPB, OPT := OfrontOPT,
		OPV := OfrontOPV, OPC := OfrontOPC, OPM := OfrontOPM;

	PROCEDURE Module*(VAR done: BOOLEAN);
		VAR ext, new: BOOLEAN; p: OPT.Node;
	BEGIN
		OPP.Module(p, OPM.opt);
		IF OPM.noerr THEN
			OPV.Init;
			OPV.AdrAndSize(OPT.topScope);
			OPT.Export(ext, new);
			IF OPM.noerr THEN
				OPM.OpenFiles(OPT.SelfName);
				OPC.Init;
				OPV.Module(p);
				IF OPM.noerr THEN
					IF (OPM.mainprog IN OPM.opt) & (OPM.modName # "SYSTEM") THEN
						OPM.DeleteNewSym; OPM.LogWStr("  main program")
					ELSE
						IF new THEN OPM.LogWStr("  new symbol file"); OPM.RegisterNewSym
						ELSIF ext THEN OPM.LogWStr("  extended symbol file"); OPM.RegisterNewSym
						END
					END
				ELSE OPM.DeleteNewSym
				END
			END
		END ;
		OPM.CloseFiles;
		IF OPT.topScope # NIL THEN OPT.Close END; (* sonst Trap nach Fehler *)
		OPM.LogWLn; done := OPM.noerr
	END Module;

	PROCEDURE InitParams(list: BOOLEAN);
	BEGIN
		OPM.OpenPar(list);
		OPT.bytetyp.size := OPM.ByteSize;
		OPT.sysptrtyp.size := OPM.PointerSize;
		OPT.chartyp.size := OPM.CharSize;
		OPT.settyp.size := OPM.SetSize;
		OPT.realtyp.size := OPM.RealSize;
		OPT.inttyp.size := OPM.IntSize;
		OPT.linttyp.size := OPM.LIntSize;
		OPT.lrltyp.size := OPM.LRealSize;
		OPT.sinttyp.size := OPM.SIntSize;
		OPT.booltyp.size := OPM.BoolSize;
	END InitParams;

	PROCEDURE Translate*;
		VAR done: BOOLEAN;
	BEGIN
		InitParams(FALSE);
		OPM.Init(done);
		OPM.InitOptions();
		IF done THEN Module(done) END
	END Translate;

	PROCEDURE TranslateDone* (): BOOLEAN;
		VAR done: BOOLEAN;
	BEGIN
		InitParams(FALSE);
		OPM.Init(done);
		OPM.InitOptions();
		IF done THEN Module(done) END;
	RETURN done
	END TranslateDone;

	PROCEDURE TranslateModuleList*;
		VAR done: BOOLEAN;
	BEGIN
		InitParams(TRUE);
		LOOP
			OPM.Init(done);
			IF ~done THEN EXIT END ;
			OPM.InitOptions;
			Module(done);
			IF ~done THEN EXIT END
		END
	END TranslateModuleList;

BEGIN
	OPB.typSize := OPV.TypSize; OPT.typSize := OPV.TypSize
END OfrontCmd.
