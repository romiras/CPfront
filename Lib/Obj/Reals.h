/*  Ofront 1.2 -xtspkae */

#ifndef Reals__h
#define Reals__h

#include "SYSTEM.h"




import void Reals_Convert (REAL x, INTEGER n, CHAR *d, LONGINT d__len);
import void Reals_ConvertH (REAL y, CHAR *d, LONGINT d__len);
import void Reals_ConvertHL (LONGREAL x, CHAR *d, LONGINT d__len);
import void Reals_ConvertL (LONGREAL x, INTEGER n, CHAR *d, LONGINT d__len);
import INTEGER Reals_Expo (REAL x);
import INTEGER Reals_ExpoL (LONGREAL x);
import void Reals_SetExpo (INTEGER e, REAL *x);
import void Reals_SetExpoL (INTEGER e, LONGREAL *x);
import REAL Reals_Ten (INTEGER e);
import LONGREAL Reals_TenL (INTEGER e);
import void *Reals__init(void);


#endif
