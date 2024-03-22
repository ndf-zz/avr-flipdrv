// SPDX-License-Identifier: MIT

/*
 * AVR m328p (Nano) Serial Flipdot Display and Clock
 */
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/atomic.h>
#include "util.h"
#include "display.h"
#include "ds3231.h"

#define SYSTICK EEARL
#define EXTERNAL EEARH		// EEAR8 only
#define BHOUR 3			// PORTD.3
#define BMIN 7			// PORTD.7
#define RTCINT 3		// PORTC.3

#define NAK 0x15;
#define BUFLEN 0x20
#define BUFMASK (BUFLEN-1)
#define BUFWI GPIOR1
#define BUFRI GPIOR2
uint8_t rdbuf[BUFLEN];

ISR(TIMER0_COMPA_vect)
{
	++SYSTICK;
}

ISR(USART_RX_vect)
{
	uint8_t status = UCSR0A;
	barrier();
	uint8_t tmp = UDR0;
	uint8_t look = (uint8_t) ((BUFWI + 1) & BUFMASK);
	if (look != BUFRI) {
		if (status & (_BV(FE0) | _BV(DOR0))) {
			rdbuf[look] = NAK;
		} else {
			rdbuf[look] = tmp;
		}
		barrier();
		BUFWI = look;
	}			// ignore overrun input
	// Flag external control of display
	EXTERNAL = 1;
}

/* Handle text input */
void handle_text(uint8_t msg)
{
	static uint8_t pos = 0;

	if (pos == 0) {
		display_clear();
	}
	switch (msg) {
	case 0x04:
		// EOT
		display_trigger();
		break;
	case 0x07:
		// Bell
		display_fill(0xff);
		display_flush();
		pos = 0;
		display_trigger();
		break;
	case 0x08:
		// Backspace
		if (pos)
			--pos;
		break;
	case 0x09:
		// Tab
		pos = (uint8_t) (pos + 4);
		break;
	case 0x0a:
		// Line Feed
		pos = 0;
		display_trigger();
		break;
	case 0x0c:
		// Form Feed
		pos = 0;
		display_clear();
		display_flush();
		display_trigger();
		break;
	case 0x0d:
		// Carriage Return
		pos = 0;
		break;
	case 0x10:
		// Data Link Escape
		display_flush();
		break;
	case 0x20:
		// Space
		++pos;
		break;
	default:
		if (msg > 0x20 && msg < 0x7f) {
			// Printable text
			display_char(msg, pos);
			pos = (uint8_t) (pos + 4);
		} else if ((msg & 0xe0) == 0x80) {
			// Raw bits
			display_data(msg, pos);
			++pos;
		} else if ((msg & 0xe0) == 0xc0) {
			// Column offset
			pos = msg & 0x1f;
		}
		break;
	}
}

/* Debounce push buttons on port D and return flags:
 *
 *  Bit		Event
 *  0		Minute Release
 *  1		Minute Press
 *  2		Hour Press
 *  3		Hour Release
 */
uint8_t debounce(void)
{
	static uint8_t bprev = _BV(BHOUR) | _BV(BMIN);
	static uint8_t bstate = _BV(BHOUR) | _BV(BMIN);
	uint8_t flags = 0;
	uint8_t tmp = PIND & (_BV(BHOUR) | _BV(BMIN));
	if ((tmp ^ bprev) == 0) {
		uint8_t mask = tmp ^ bstate;
		if (mask & _BV(BMIN)) {
			if (tmp & _BV(BMIN)) {
				flags |= _BV(0);	// release
			} else {
				flags |= _BV(1);	// press
				EXTERNAL = 0;
			}
		}
		if (mask & _BV(BHOUR)) {
			if (tmp & _BV(BHOUR)) {
				flags |= _BV(2);
			} else {
				flags |= _BV(3);
				EXTERNAL = 0;
			}
		}
		bstate = tmp;
	}
	bprev = tmp;
	return flags;
}

/* Write byte to serial output */
void send_serial(uint8_t ch)
{
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = ch;
}

/* Write byte to input queue */
void queue_input(uint8_t ch)
{
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		uint8_t look = (uint8_t) ((BUFWI + 1) & BUFMASK);
		if (look != BUFRI) {
			rdbuf[look] = ch;
			barrier();
			BUFWI = look;
		}		// ignore overrun
	}
}

/* Read and process next byte from input queue */
void read_queue(void)
{
	if (BUFRI != BUFWI) {
		uint8_t look = (uint8_t) ((BUFRI + 1) & BUFMASK);
		uint8_t ch = rdbuf[look];
		barrier();
		BUFRI = look;
		handle_text(ch);
		barrier();
		send_serial(ch);
	}
}

/* Update display with current time - cancels display update if in progress */
void update_time(struct ds3231_stat *stat)
{
	if (bit_is_set(DISPLAY_STAT, DISBSY)) {
		display_abort();
		queue_input(0x10);
	}
	if (stat->minute == 0x00) {
		queue_input(0x07);
	}
	queue_input(0x0d);
	if ((stat->hour) & 0x10) {
		queue_input(0x20);
		queue_input(0x31);
	} else {
		queue_input(0xc3);
	}
	queue_input((uint8_t) (0x30 + ((stat->hour) & 0x0f)));
	queue_input(0x08);
	queue_input(0x3a);
	queue_input(0x08);
	queue_input((uint8_t) (0x30 + ((stat->minute) >> 4)));
	queue_input((uint8_t) (0x30 + ((stat->minute) & 0x0f)));
	queue_input(0x0a);
}

/* Read from RTC and update display */
void read_rtc(void)
{
	struct ds3231_stat ds;
	if (ds3231_read(&ds)) {
		if (EXTERNAL) {
			EXTERNAL = 0;
		} else {
			update_time(&ds);
		}
	}
}

/* Increment hour value on RTC, ignoring AM/PM flag */
void increment_hour(void)
{
	struct ds3231_stat stat;
	ds3231_read(&stat);
	uint8_t t1 = stat.hour & 0x1f;
	if (t1 == 0x12) {
		t1 = 0x01;
	} else if ((t1 & 0x0f) == 0x9) {
		t1 = 0x10;
	} else {
		t1++;
	}
	t1 = t1 | 0x40;
	ds3231_hours(t1);
	read_rtc();
}

/* Increment minute value on RTC and zero seconds */
void increment_minute(void)
{
	struct ds3231_stat stat;
	ds3231_read(&stat);
	uint8_t t1 = stat.minute & 0x7f;
	if (t1 == 0x59) {
		t1 = 0x00;
	} else if ((t1 & 0x0f) == 0x9) {
		t1 = (uint8_t) ((t1 & 0x70) + 0x10);
	} else {
		t1++;
	}
	ds3231_seconds(0x00);
	ds3231_minutes(t1);
	read_rtc();
}

/* Handle button press and release events */
void read_buttons(void)
{
	uint8_t flags = debounce();
	if (flags) {
		if (flags == 0x0a) {	// press both
			queue_input(0x0c);
			queue_input(0x10);
		} else if (flags & 0x02) {	// press min
			increment_minute();
		} else if (flags & 0x08) {	// press hr
			increment_hour();
		}
	}
}

void main(void)
{
	uint8_t lt = 0;

	/* init ~20Hz timer */
	OCR0A = 96;
	TCCR0A = _BV(WGM01);
	TCCR0B = _BV(CS02) | _BV(CS00);
	TIMSK0 |= _BV(OCIE0A);

	/* init 9600,8n1 serial I/O w/ interrupt receive */
	UBRR0L = 12;
	UCSR0B = _BV(RXCIE0) | _BV(RXEN0) | _BV(TXEN0);
	UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);

	/* set up push buttons */
	PORTD = _BV(BHOUR) | _BV(BMIN);

	sei();
	ds3231_init();
	display_init();
	display_flush();
	read_rtc();
	display_trigger();
	do {
		sleep_mode();
		if (SYSTICK != lt) {
			lt = SYSTICK;
			display_tick();
			read_buttons();
		}
		if (!(DISPLAY_STAT & (_BV(DISBSY) | _BV(DISUPD)))) {
			if (bit_is_clear(PINC, RTCINT)) {
				read_rtc();
			}
			while (BUFRI != BUFWI
			       && bit_is_clear(DISPLAY_STAT, DISUPD)) {
				read_queue();
			}
		}
	} while (1);
}
