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

/* during update sweep, keep this many columns powered at a time */
#define DISPLAY_COLPOWER 4
#define DISPLAY_COLOVER (DISPLAY_COLS + DISPLAY_COLPOWER)

struct display_stat display;

/* fetch the byte offset in request for the provided group, panel and line */
uint8_t req_offset(uint8_t group, uint8_t panel, uint8_t line)
{
	uint8_t poft =
	    (uint8_t) ((DISPLAY_PANELS - 1) - (DISPLAY_PPG * group + panel));
	uint8_t loft = (uint8_t) ((DISPLAY_BPP - 1) - line);
	return (uint8_t) (poft * DISPLAY_BPP + loft);
}

/* Send byte to display via SPI */
void shift_byte(uint8_t val)
{
	SPDR = val;
	loop_until_bit_is_set(SPSR, SPIF);
}

/* send current request buffer to display */
void req_send(void)
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

/* write group column updates to request */
void update_column(uint8_t col)
{
	uint8_t goft = col >> 3;	/* group offset */
	uint8_t coft = col & 0x7U;	/* column offset in group */
	uint8_t poft = coft >> 2;	/* panel offset in group */
	uint8_t shift = col & 0x4U;	/* src shift for panel data */
	uint8_t srcmask = (uint8_t) (0x1U << coft);
	uint8_t srcoft;
	uint8_t src;
	uint8_t roft;
	uint8_t mask;
	uint8_t line = 0U;
	do {
		srcoft = (uint8_t) (line * DISPLAY_GROUPS + goft);
		src = display.buf[srcoft];
		mask = srcmask & (src ^ display.cur[srcoft]);
		roft = req_offset(goft, poft, line);
		display.req[roft] |=
		    setclr_pattern((uint8_t) (src >> shift),
				   (uint8_t) (mask >> shift));
		display.cur[srcoft] &= (uint8_t) (~srcmask);
		display.cur[srcoft] |= src & srcmask;
		line++;
	} while (line < DISPLAY_LINES);

}

/* transfer a single column of changes from buf into req */
void req_power_col(uint8_t col)
{
	if (col < DISPLAY_COLS) {
		update_column(col);
	}
}

/* relax a single column in display request */
void req_relax_col(uint8_t col)
{
	if (col < DISPLAY_COLS) {
		uint8_t goft = col >> 3;	/* group offset */
		uint8_t coft = col & 0x7U;	/* column offset in group */
		uint8_t poft = coft >> 2;	/* panel offset in group */
		uint8_t pcoft = coft & 0x3U;	/* column offset on panel */
		uint8_t shift = (uint8_t) (pcoft << 1);
		uint8_t mask = (uint8_t) ~ (0x3U << shift);
		uint8_t line = 0U;
		uint8_t roft;
		do {
			roft = req_offset(goft, poft, line);
			display.req[roft] &= (uint8_t) mask;
			line++;
		} while (line < DISPLAY_LINES);
	}
}

/* relax all coils in display request */
void req_relax(void)
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

/* Invalidate all pixels to force update */
void display_invalidate(void)
{
	uint8_t i = 0;
	do {
		display.cur[i] = (uint8_t) ~ display.buf[i];
		i++;
	} while (i < DISPLAY_BUFLEN);
}

/* prepare a full display relax request */
void display_relax(void)
{
	req_relax();
	req_send();
	req_latch();
}

/* animate changes onto display as required */
void display_tick(void)
{
	static uint8_t ck = 0U;
	if (bit_is_set(DISPLAY_STAT, DISBSY)) {
		if (ck > DISPLAY_COLOVER || bit_is_set(DISPLAY_STAT, DISABRT)) {
			req_relax();
			if (bit_is_set(DISPLAY_STAT, DISABRT)) {
				display_clear();
			}
			DISPLAY_STAT = 0U;
		} else {
			req_power_col(ck);
			if (ck >= DISPLAY_COLPOWER)
				req_relax_col((uint8_t)
					      (ck - DISPLAY_COLPOWER));
		}
		req_send();
		req_latch();
		ck++;
	} else {
		if (bit_is_set(DISPLAY_STAT, DISUPD)) {
			if (bit_is_set(DISPLAY_STAT, DISFSH))
				display_invalidate();
			DISPLAY_STAT = _BV(DISBSY);
			ck = 0U;
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

/* Draw raw data at column */
void display_data(uint8_t data, uint8_t col)
{
	uint8_t group;
	uint8_t mask = (uint8_t) (1U << (col & 0x7));
	data &= 0x1f;
	uint8_t poft;
	uint8_t row;
	if (col < DISPLAY_COLS) {
		group = col >> 3U;
		row = 4U;
		do {
			poft = (uint8_t) (group + row * DISPLAY_GROUPS);
			if (data & 0x1)
				display.buf[poft] |= mask;
			data = data >> 1U;
			row--;
		} while (row < DISPLAY_LINES);
	}
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
				    (uint8_t) (display.buf[poft] | tmp <<
					       pshift);
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
					tmp = (uint8_t) ((Font_5x4[foft] & mask)
							 >> cshift);
					display.buf[poft] =
					    (uint8_t) (display.buf[poft] | tmp);
					row++;
				} while (row < DISPLAY_LINES);
			}
		}
	}
}
