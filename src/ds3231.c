// SPDX-License-Identifier: MIT

/*
 * Minimal blocking TWI Master interface to DS 3231 RTC on
 * Jaycar XC9044 module with /INT conected to PORTC.3
 *
 * Note: Minimal error checking aborts failed transactions
 */

#include <avr/io.h>
#include "ds3231.h"

#define SLA_W 0xd0;
#define SLA_R 0xd1;

void i2c_start(void)
{
	TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
	loop_until_bit_is_set(TWCR, TWINT);
}

void i2c_slaw(void)
{
	TWDR = SLA_W;
	TWCR = _BV(TWINT) | _BV(TWEN);
	loop_until_bit_is_set(TWCR, TWINT);
}

void i2c_slar(void)
{
	TWDR = SLA_R;
	TWCR = _BV(TWINT) | _BV(TWEN);
	loop_until_bit_is_set(TWCR, TWINT);
}

/* send data byte */
void i2c_data(uint8_t ch)
{
	TWDR = ch;
	TWCR = _BV(TWINT) | _BV(TWEN);
	loop_until_bit_is_set(TWCR, TWINT);
}

/* recv data byte with ack */
uint8_t i2c_dack(void)
{
	TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWEA);
	loop_until_bit_is_set(TWCR, TWINT);
	return TWDR;
}

/* recv data byte with nack */
uint8_t i2c_dnack(void)
{
	TWCR = _BV(TWINT) | _BV(TWEN);
	loop_until_bit_is_set(TWCR, TWINT);
	return TWDR;
}

/* send i2c stop */
void i2c_stop(void)
{
	TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO);
}

/* send len bytes from buf to slave addr */
void i2c_send(uint8_t addr, uint8_t * buf, uint8_t len)
{
	i2c_start();
	if ((TWSR & 0xf8) == 0x08) {
		i2c_slaw();
		if ((TWSR & 0xf8) == 0x18) {
			i2c_data(addr);
			while (len) {
				i2c_data(*buf++);
				--len;
			}
		}
	}
	i2c_stop();
}

/* read len bytes into buf */
void i2c_recv(uint8_t * buf, uint8_t len)
{
	i2c_start();
	if ((TWSR & 0xf8) == 0x08) {
		i2c_slar();
		if ((TWSR & 0xf8) == 0x40) {
			while (len > 1) {
				*buf++ = i2c_dack();
				--len;
			}
			if (len) {
				*buf++ = i2c_dnack();
			}
		}
	}
	i2c_stop();
}

uint8_t ds3231_read(struct ds3231_stat *stat)
{
	uint8_t cmd[7];
	cmd[0] = 0x00;
	i2c_send(0x0f, &cmd[0], 1);
	i2c_recv(&cmd[0], 7);
	if (cmd[6]) {
		stat->hour = cmd[5];
		stat->minute = cmd[4];
		stat->temp = (int8_t) cmd[1];
	} else {
		stat->hour = 0x1f;
		stat->minute = 0xff;
		stat->temp = 0;
	}
	return cmd[6];
}

void ds3231_hours(uint8_t hours)
{
	i2c_send(0x02, &hours, 1U);
}

void ds3231_minutes(uint8_t minutes)
{
	i2c_send(0x01, &minutes, 1U);
}

void ds3231_seconds(uint8_t seconds)
{
	i2c_send(0x00, &seconds, 1U);
}

void ds3231_init(void)
{
	uint8_t cmd[5];
	struct ds3231_stat stat;

	/* Clear I2C state, ref: ds3231 datasheet */
	PORTC = _BV(3);		// Pull up /INT input
	DDRC = _BV(5);
	do {
		PINC |= _BV(5);
	} while (bit_is_clear(PINC, 4));

	/* re-configure Port C */
	DDRC = 0;

	/* initialise RTC and Alarm 2 Mask bits for "once per minute"  */
	cmd[0] = 0x80;		// A2M2
	cmd[1] = 0x80;		// A2M3
	cmd[2] = 0x80;		// A2M4
	cmd[3] = 0x06;		// INTCN | A2F
	cmd[4] = 0x00;
	i2c_send(0x0b, &cmd[0], 5);

	/* read out current values and configure 12h clock if required */
	if (ds3231_read(&stat)) {
		if (!((stat.hour) & 0x40)) {
			uint8_t t1 = stat.hour & 0x3f;
			uint8_t t2;
			if (t1 == 0U) {
				t1 = 0x12;
			} else if (t1 > 0x12) {
				t2 = (uint8_t) (10U * (t1 >> 4U) + (t1 & 0x0f) -
						12U);
				t1 = 0U;
				if (t2 > 9) {
					t1 |= 0x10;
					t2 = (uint8_t) (t2 - 10U);
				}
				t1 |= t2;
			}
			/* force 12 hr clock */
			cmd[0] = 0x40 | t1;
			i2c_send(0x02, &cmd[0], 1U);
		}
	}
}
