// SPDX-License-Identifier: MIT

/*
 * AVR m328p (Nano) Serial Flipdot Display Interface 
 */
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "util.h"
#include "display.h"

#define NAK 0x15;
#define BUFLEN 0x20
#define BUFMASK (BUFLEN-1)
uint8_t rdbuf[BUFLEN];
volatile uint8_t ri;
volatile uint8_t wi;

ISR(TIMER0_COMPA_vect)
{
	++GPIOR1;
}

ISR(USART_RX_vect)
{
	uint8_t status = UCSR0A;
	barrier();
	uint8_t tmp = UDR0;
	uint8_t look = (uint8_t) ((wi + 1) & BUFMASK);
	if (look != ri) {
		if (status & (_BV(FE0) | _BV(DOR0))) {
			rdbuf[look] = NAK;
		} else {
			rdbuf[look] = tmp;
		}
		barrier();
		wi = look;
	}	// ignore overrun input
}

/* Write byte to serial output */
void send_serial(uint8_t ch)
{
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = ch;
}

/* Handle text inputs */
void place_text(uint8_t msg)
{
	static uint8_t pos = 0U;

	if (pos == 0U) {
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
		pos = 0U;
		display_trigger();
		break;
	case 0x08:
		// Backspace
		if (pos) --pos;
		break;
	case 0x09:
		// Tab
		pos = (uint8_t)(pos + 4U);
		break;
	case 0x0a:
		// Line Feed
		pos = 0U;
		display_trigger();
		break;
	case 0x0c:
		// Form Feed
		pos = 0U;
		display_clear();
		display_trigger();
		break;
	case 0x0d:
		// Carriage Return
		pos = 0U;
		break;
	case 0x20:
		// Space
		++pos;
		break;
	default:
		if (msg > 0x20 && msg < 0x7f) {
			display_char(msg, pos);
			pos = (uint8_t) (pos + 4U);
		}
		break;
	}
}

/* Read and process next byte from read buffer */
void read_input(void)
{
	uint8_t ch;
	if (ri != wi) {
		uint8_t look = (uint8_t) ((ri + 1) & BUFMASK);
		ch = rdbuf[look];
		barrier();
		ri = look;
		place_text(ch);
		barrier();
		send_serial(ch);
	}
}

void main(void)
{
	uint8_t lt = 0U;

	/* init timer */
	OCR0A = 95U;
	TCCR0A = _BV(WGM01);
	TCCR0B = _BV(CS02) | _BV(CS00);
	TIMSK0 |= _BV(OCIE0A);

	/* init 9600,8n1 serial I/O w/ interrupt receive */
	UBRR0L = 12U;
	UCSR0B = _BV(RXCIE0) | _BV(RXEN0) | _BV(TXEN0);
	UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);

	sei();

	display_init();
	display_trigger();
	do {
		sleep_mode();
		if (GPIOR1 != lt) {
			lt = GPIOR1;
			display_tick();
		}
		if (!DISPLAY_STAT) {
			read_input();
		}
	} while (1);
}
