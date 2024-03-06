// SPDX-License-Identifier: MIT

/*
 * 5x4 Uppercase ASCII Font
 *
 * Charset:
 * 
 * 	 !"#$%&'()*+,-./
 *	0123456789:;<=>?
 *	@ABCDEFGHIJKLMNO
 *	PQRSTUVWXYZ[\]^_
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
