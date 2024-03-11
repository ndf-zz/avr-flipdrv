// SPDX-License-Identifier: MIT

/*
 * Low-level display SPI interface
 */
#ifndef DISPLAY_H
#define DISPLAY_H
#include <stdint.h>

/*
 * Display layout (viewed from front side)
 *
 *       Display is an integer number of panel groups, each 8 columns wide
 *       by 5 rows high, stored as unsigned 8 bit integers:
 *       +--------+-------
 *       |XXXXXXXX|       
 *       |XXXXXXXX|       
 *       |XXXXXXXX| [...] 
 *       |XXXXXXXX|       
 *       |XXXXXXXX|       
 *       +--------+-------
 *
 *       Display panels form a shift register that chains 2 display
 *       panels per group, left to right:
 *       +---+---+---
 * IN -> |P-P-P-P- [...] -> OUT
 *       +---+---+---
 *
 *       Each panel is a 4x5 array of pixel locations addressed
 *	 sequentially as follows:
 *
 *                 IN
 *                  |
 *                  v
 *	 +------------+
 *       | 4  3  2  1 |
 *       | 8  7  6  5 |
 *       |12 11 10  9 |
 *       |16 15 14 13 |
 *       |20 19 18 17 |
 *	 +------------+
 *         |
 *         v
 *        OUT
 *
 *       Each panel is updated with a 40 bit control message,
 *       sent serially as a string of 8 bit messages (one per row):
 * Bit:	    7   6   5   4   3   2   1   0
 * Byte	 +-------------------------------+
 *    0	 |S17 C17 S18 C18 S19 C19 S20 C20|
 *    1	 |S13 C13 S14 C14 S15 C15 S16 C16|
 *    2	 | S9  C9 S10 C10 S11 C11 S12 C12|
 *    3	 | S5  C5  S6  C6  S7  C7  S8  C8|
 *    4	 | S1  C1  S2  C2  S3  C3  S4  C4|
 *	 +-------------------------------+
 *
 *       The whole display is updated by shifting out panel updates from
 *       right to left and then latching the shift register
 */

/* number of 8bit panel groups (sets max display size) */
#define DISPLAY_GROUPS	4

/* display width & height */
#define DISPLAY_GROUPCOLS	8
#define DISPLAY_COLS	(DISPLAY_GROUPS * DISPLAY_GROUPCOLS)
#define DISPLAY_LINES	5

/* panels per group */
#define DISPLAY_PPG	2

/* max number of panels in display */
#define DISPLAY_PANELS	(DISPLAY_GROUPS * DISPLAY_PPG)

/* size of display pixel buffers */
#define DISPLAY_BUFLEN	(DISPLAY_GROUPS * DISPLAY_LINES)

/* number of 8 bit (row) messages in panel update request string */
#define DISPLAY_REQLEN	(DISPLAY_PANELS * DISPLAY_LINES)

/* status flag register */
#define DISPLAY_STAT GPIOR0
#define DISFSH 5
#define DISUPD 6
#define DISBSY 7

/* display and panel update request data structures */
struct display_stat {
	uint8_t		buf[DISPLAY_BUFLEN];	/* CAIRO_FORMAT_A1 */
	uint8_t		cur[DISPLAY_BUFLEN];
	uint8_t		req[DISPLAY_REQLEN];
};

/* Convenience macros to set flags */
#define display_trigger() do { GPIOR0 |= _BV(DISUPD) ; } while(0)
#define display_flush() do { GPIOR0 |= _BV(DISFSH) ; } while(0)

/* Clear the display buffer */
void display_clear(void);

/* Fill display buffer with ch */
void display_fill(uint8_t ch);

/* Un-power all pixel coils */
void display_relax(void);

/* Advance display updates */
void display_tick(void);

/* Place character at column */
void display_char(uint8_t ch, uint8_t col);

/* Place column of raw data */
void display_data(uint8_t data, uint8_t col);

/* Initialise display and relax all coils */
void display_init(void);

#endif /* DISPLAY_H */
