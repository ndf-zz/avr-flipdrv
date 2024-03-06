// SPDX-License-Identifier: MIT

/*
 * Drive a row of flippnlr4x5 via SPI
 */

#include "display.h"
#include <avr/io.h>
#include "util.h"
#include "font.h"

#define DISPLAY_BPP	5
#define SPI_CS		2
#define SPI_COPI	3
#define SPI_SCK		5
#define DISPLAY_COLPWR	3
#define DISPLAY_COLOVER (DISPLAY_COLS + DISPLAY_COLPWR + 1)

struct display_stat display;

/* fetch the byte offset in request for the provided group, panel and line */
uint8_t req_offset(uint8_t group, uint8_t panel, uint8_t line)
{
	uint8_t goft = (uint8_t) ((DISPLAY_GROUPS - UINT8_C(1)) - group);
	uint8_t poft = (uint8_t) ((DISPLAY_PPG - UINT8_C(1)) - panel);
	uint8_t loft = (uint8_t) ((DISPLAY_BPP - UINT8_C(1)) - line);
	/* number of panels to skip + line offset */
	return (uint8_t) ((goft * DISPLAY_PPG + poft) * DISPLAY_BPP + loft);
}

/* Send byte to display register */
void shift_byte(uint8_t val)
{
	SPDR = val;
	loop_until_bit_is_set(SPSR, SPIF);
}

/* transfer request buffer to display */
void req_transfer(void)
{
	uint8_t cnt = 0;
	do {
		shift_byte(display.req[cnt]);
		cnt++;
	} while (cnt < DISPLAY_REQLEN);
}

/* latch display request register to coils */
void req_latch(void)
{
	PORTB |= _BV(SPI_CS);
	PORTB &= (uint8_t) ~ _BV(SPI_CS);
}

/* return an 8 bit set/clear pattern for the bottom 4 bits of val */
uint8_t setclr_pattern(uint8_t val, uint8_t mask)
{
	uint8_t cnt = 0x0U;
	uint8_t ret = 0x0U;
	do {
		ret = (uint8_t) (ret << 2);
		if (mask & 0x08) {
			if (val & 0x08)
				ret |= 0x1;
			else
				ret |= 0x2;
		}
		mask = (uint8_t) (mask << 1);
		val = (uint8_t) (val << 1);
		cnt++;
	} while (cnt < 4);
	return (uint8_t) ret;
}

void update_group(uint8_t goft, uint8_t line)
{
	uint8_t bufoft = (uint8_t) (line * DISPLAY_GROUPS + goft);
	uint8_t src = display.buf[bufoft];
	uint8_t mask = 0xff;
	uint8_t roft;

	roft = req_offset(goft, 0, line);
	display.req[roft] = setclr_pattern(src, mask);
	roft = req_offset(goft, 1, line);
	display.req[roft] = setclr_pattern(src >> 4, mask >> 4);
}

void update_column(uint8_t col)
{
	uint8_t goft = col >> 3;	/* group offset */
	uint8_t coft = col & 0x7U;	/* column offset in group */
	uint8_t poft = coft >> 2;	/* panel offset in group */
	uint8_t shift = col & 0x4U;	/* src shift for panel data */
	uint8_t srcmask = (uint8_t) (0x1U << coft);	/* column to set/clear in src */
	uint8_t srcoft;
	uint8_t src;
	uint8_t roft;
	uint8_t mask;
	uint8_t line = 0U;
	do {
		srcoft = (uint8_t) (line * DISPLAY_GROUPS + goft);
		src = display.buf[srcoft];
		mask = srcmask;
		roft = req_offset(goft, poft, line);
		display.req[roft] |=
		    setclr_pattern((uint8_t) (src >> shift),
				   (uint8_t) (mask >> shift));
		line++;
	} while (line < DISPLAY_LINES);

}

/* transfer a single line of changes from buf into req */
void req_setline(uint8_t line)
{
	/* for each group of panels */
	uint8_t goft = 0;
	do {
		update_group(goft, line);
		goft++;
	} while (goft < DISPLAY_GROUPS);
}

/* transfer a single column of changes from buf into req */
void req_setcol(uint8_t col)
{
	if (col < DISPLAY_COLS) {
		update_column(col);
	}
}

/* clear single line in display request */
void req_clearline(uint8_t line)
{
	/* for each group of panels */
	uint8_t roft;
	uint8_t goft = 0;
	do {
		/* for each panel in group */
		uint8_t poft = 0;
		do {
			roft = req_offset(goft, poft, line);
			display.req[roft] = 0x0U;
			poft++;
		} while (poft < DISPLAY_PPG);
		goft++;
	} while (goft < DISPLAY_GROUPS);
}

/* clear a single column in display request */
void req_clearcol(uint8_t col)
{
	if (col < DISPLAY_COLS) {
		uint8_t goft = col >> 3;	/* group offset col/8 */
		uint8_t coft = col & 0x7U;	/* column offset in group col%8 */
		uint8_t poft = coft >> 2;	/* panel offset in group */
		uint8_t pcoft = coft & 0x3U;	/* column offset on panel */
		uint8_t shift = (uint8_t) (pcoft << 1);	/* shift req mask */
		uint8_t mask = (uint8_t) ~ (0x3U << shift);	/* req byte mask */
		uint8_t line = 0U;
		uint8_t roft;
		do {
			roft = req_offset(goft, poft, line);
			display.req[roft] &= (uint8_t) mask;
			line++;
		} while (line < DISPLAY_LINES);
	}
}

/* clear display request */
void req_clear(void)
{
	uint8_t cnt = 0;
	do {
		display.req[cnt] = 0x0;
		cnt++;
	} while (cnt < DISPLAY_REQLEN);
}

/* clear display buffer */
void display_clear(void)
{
	display_fill(0U);
}

/* prepare a full display update request */
void display_set(void)
{
	/* for each row */
	uint8_t row = 0;
	do {
		req_setline(row);
		row++;
	} while (row < DISPLAY_LINES);
	req_transfer();
	req_latch();
}

/* prepare a full display relax request */
void display_relax(void)
{
	req_clear();
	req_transfer();
	req_latch();
}

/* animate changes onto display as required */
void display_tick(void)
{
	static uint8_t ck = 0U;
	if (bit_is_set(DISPLAY_STAT, DISBSY)) {
		req_setcol(ck);
		req_clearcol((uint8_t) (ck - DISPLAY_COLPWR));
		req_transfer();
		req_latch();
		ck++;
		if (ck >= DISPLAY_COLOVER) {
			DISPLAY_STAT = 0U;
		}
	} else {
		if (bit_is_set(DISPLAY_STAT, DISUPD)) {
			ck = 0U;
			DISPLAY_STAT = _BV(DISBSY);
		}
	}
}

/* initialise h/w & buffer, relax all coils */
void display_init(void)
{
	/* Init SPI output */
	DDRB = _BV(SPI_COPI) | _BV(SPI_SCK) | _BV(SPI_CS);
	PORTB &= (uint8_t) ~ _BV(SPI_CS);
	SPCR = _BV(SPE) | _BV(DORD) | _BV(MSTR);
	SPSR |= _BV(SPI2X);

	/* clear buffers and relax coils */
	display_clear();
	display_relax();
}

/* Set all display buffers to the provided value */
void display_fill(uint8_t ch)
{
        uint8_t i = 0;
        do { 
                display.buf[i] = ch;
                i++;
        } while (i < DISPLAY_BUFLEN);
}

/* Draw character at column */
void display_char(uint8_t ch, uint8_t col)
{
	uint8_t oft;
	uint8_t cshift;
	uint8_t mask;
	uint8_t group;
	uint8_t pshift;
	uint8_t row;
	uint8_t tmp;
	uint8_t poft;
	uint8_t foft;

	if (col < DISPLAY_COLS) {
		oft = 0U;
		if (ch >= 0x20 && ch < 0x80) {
			if (ch & 0x40)
				ch &= 0x5f;
			ch = (uint8_t) (ch - 0x20);
			if (ch >= 0x20) {
				mask = 0xf0;
				cshift = 4U;
				ch = (uint8_t) (ch - 0x20);
			} else {
				mask = 0x0f;
				cshift = 0;
			}
			oft = (uint8_t) (FONT_5X4_CHARH * ch);
			/* first part */
			group = col >> 3U;
			pshift = col & 0x7;
			row = 0;
			do {
				poft = (uint8_t) (group + row * DISPLAY_GROUPS);
				foft = (uint8_t) (oft + row);
				tmp =
				    (uint8_t) ((Font_5x4[foft] & mask) >>
					       cshift);
				display.buf[poft] =
				    (uint8_t) (display.
					       buf[poft] | tmp << pshift);
				row++;
			} while (row < DISPLAY_LINES);
			/* remainder */
			if (pshift >= 4) {
				group++;
				cshift =
				    (uint8_t) (cshift + (4 - (pshift - 4)));
				row = 0;
				do {
					poft =
					    (uint8_t) (group +
						       row * DISPLAY_GROUPS);
					foft = (uint8_t) (oft + row);
					tmp =
					    (uint8_t) ((Font_5x4[foft] & mask)
						       >> cshift);
					display.buf[poft] =
					    (uint8_t) (display.buf[poft] | tmp);
					row++;
				} while (row < DISPLAY_LINES);
			}
		}
	}
}
