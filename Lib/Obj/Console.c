/*  Ofront 1.2 -xtspkae */
#include "SYSTEM.h"




export void Console_Bool (BOOLEAN b);
export void Console_Char (CHAR ch);
export void Console_Flush (void);
export void Console_Hex (LONGINT i);
export void Console_Int (LONGINT i, LONGINT n);
export void Console_Ln (void);
export void Console_Read (CHAR *ch);
export void Console_ReadLine (CHAR *line, LONGINT line__len);
export void Console_String (CHAR *s, LONGINT s__len);

#define Console_fflushstdout()	fflush(stdout)
#include <stdio.h>
#define Console_read(ch)	read(0/*stdin*/, ch, 1)
#define Console_writeCh(ch)	printf("%c", ch)
#define Console_writeLn()	printf("\n")
#define Console_writeStr(str, str__len)	printf("%s", str)

/*============================================================================*/

void Console_Flush (void)
{
	Console_fflushstdout();
}

/*----------------------------------------------------------------------------*/
void Console_Char (CHAR ch)
{
	Console_writeCh(ch);
	if (ch == 0x0a) {
		Console_fflushstdout();
	}
}

/*----------------------------------------------------------------------------*/
void Console_String (CHAR *s, LONGINT s__len)
{
	Console_writeStr(s, s__len);
}

/*----------------------------------------------------------------------------*/
void Console_Int (LONGINT i, LONGINT n)
{
	CHAR s[32];
	LONGINT i1, k;
	if (i == __LSHL(1, 31, LONGINT)) {
		__MOVE("8463847412", s, 11);
		k = 10;
	} else {
		i1 = __ABS(i);
		s[0] = (CHAR)(__MOD(i1, 10) + 48);
		i1 = __DIV(i1, 10);
		k = 1;
		while (i1 > 0) {
			s[__X(k, 32)] = (CHAR)(__MOD(i1, 10) + 48);
			i1 = __DIV(i1, 10);
			k += 1;
		}
	}
	if (i < 0) {
		s[__X(k, 32)] = '-';
		k += 1;
	}
	while (n > k) {
		Console_Char(' ');
		n -= 1;
	}
	while (k > 0) {
		k -= 1;
		Console_Char(s[__X(k, 32)]);
	}
}

/*----------------------------------------------------------------------------*/
void Console_Ln (void)
{
	Console_writeLn();
	Console_fflushstdout();
}

/*----------------------------------------------------------------------------*/
void Console_Bool (BOOLEAN b)
{
	if (b) {
		Console_String((CHAR*)"TRUE", (LONGINT)5);
	} else {
		Console_String((CHAR*)"FALSE", (LONGINT)6);
	}
}

/*----------------------------------------------------------------------------*/
void Console_Hex (LONGINT i)
{
	LONGINT k, n;
	k = -28;
	while (k <= 0) {
		n = __MASK(__ASH(i, k), -16);
		if (n <= 9) {
			Console_Char((CHAR)(48 + n));
		} else {
			Console_Char((CHAR)(55 + n));
		}
		k += 4;
	}
}

/*----------------------------------------------------------------------------*/
void Console_Read (CHAR *ch)
{
	LONGINT n;
	Console_Flush();
	n = Console_read(&*ch);
	if (n != 1) {
		*ch = 0x00;
	}
}

/*----------------------------------------------------------------------------*/
void Console_ReadLine (CHAR *line, LONGINT line__len)
{
	LONGINT i;
	CHAR ch;
	Console_Flush();
	i = 0;
	Console_Read(&ch);
	while ((i < line__len - 1 && ch != 0x0a) && ch != 0x00) {
		line[__X(i, line__len)] = ch;
		i += 1;
		Console_Read(&ch);
	}
	line[__X(i, line__len)] = 0x00;
}

/*----------------------------------------------------------------------------*/

export void *Console__init(void)
{
	__DEFMOD;
	__REGMOD("Console", 0);
	__REGCMD("Flush", Console_Flush);
	__REGCMD("Ln", Console_Ln);
/* BEGIN */
	__ENDMOD;
}
