// SPDX-License-Identifier: MIT

/*
 * 5x4 Uppercase ASCII Font
 *
 * Charset:
 * 
 * 	 !"#$%+'()*+,-./	[1,2]
 *	0123456789:;<=>?
 *	@ABCDEFGHIJKLMNO
 *	PQRSTUVWXYZ[|]^_	[3]
 *
 * Notes:
 *	1. Hash is represented by inverse checker board fill
 *	2. Ampersand is represented by plus '+'
 * 	3. Vertical bar '|' replaces backslash '\'
 *
 * Layout:
 *
 *		  Nibble
 *	Offset	Low	High
 * 	0	SP	@
 *	5	!	A
 *	10	"	B
 *		  [...]
 *	145	=	]
 *	150	>	^
 *	155	?	_
 *
 * [From 5x4_ascii.xbm]
 */
#ifndef FONT_H
#define FONT_H
#include <stdint.h>

#define FONT_5X4_CHARH 5
#define FONT_5X4_WIDTH 8
#define FONT_5X4_HEIGHT 160
extern uint8_t Font_5x4[];

#endif /* FONT_H */
