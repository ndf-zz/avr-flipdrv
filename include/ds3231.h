// SPDX-License-Identifier: MIT

/*
 * Minimal blocking TWI Master interface to DS 3231 RTC
 */
#ifndef DS3231_H
#define DS3231_H
#include <stdint.h>

struct ds3231_stat {
	uint8_t	minute;
	uint8_t	hour;
	int8_t	temp;
};

/* Read current values into structure and clear alarm flag */
uint8_t ds3231_read(struct ds3231_stat *stat);

/* Clear SDA and prepare TWI peripheral */
void ds3231_init(void);

/* set RTC seconds */
void ds3231_seconds(uint8_t seconds);

/* set RTC minutes */
void ds3231_minutes(uint8_t minutes);

/* set RTC hours */
void ds3231_hours(uint8_t hours);

#endif /* DS3231_H */
