#include "ds1338.h"

/*******************************************************************************
 * Maxim DS1338 RTC driver.
 *
 * Note: this code works in QEMU but hasn't been tested on real hardware!
 * 
 * For more info, see the Maxim DS1338 RTC data sheet.
 ******************************************************************************/

// RTC Registers
enum {
  SECONDS = 0x00,
  MINUTES = 0x01,
  HOUR    = 0x02,
  DAY     = 0x03,
  DATE    = 0x04,
  MONTH   = 0x05,
  YEAR    = 0x06,
  CONTROL = 0x07,
};

// Hour register bits
enum {
  HOUR_12 = (1 << 7),
  HOUR_PM = (1 << 6),
};

/**
 * Initialize the RTC driver.
 * 
 * @param ds1338  Pointer to the RTC driver instance
 * @param i2c     Pointer to the I2C driver instance used to access the RTC
 * @param address Serial interface address of the RTC
 */
void
ds1338_init(struct DS1338 *ds1338, struct SBCon *i2c, uint8_t address)
{
  ds1338->i2c = i2c;
  ds1338->address = address;
}

static uint8_t
bcd2bin(uint8_t value)
{
  return ((value >> 4) & 0xF) * 10 + (value & 0xF);
}

static uint8_t
bin2bcd(uint8_t value)
{
  return ((value / 10) << 4) | (value % 10);
}

/**
 * Get current time.
 * 
 * @param ds1338  Pointer to the RTC driver instance
 * @param tm      Pointer to a tm structure to store the result.
 */
void
ds1338_get_time(struct DS1338 *ds1338, struct tm *tm)
{
  // Days to start of month
  static const int daysto[][12] = {
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 },  // non-leap
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 },  // leap
  };

  int sec, min, hour, mday, mon, year, wday, is_leap;
  uint8_t buf[7];

  // Read the contents of the time and calendar registers
  sbcon_read(ds1338->i2c, ds1338->address, SECONDS, buf, 7);

  sec = bcd2bin(buf[SECONDS] & 0x7F);
  min = bcd2bin(buf[MINUTES] & 0xFF);

  if (buf[HOUR] & HOUR_12) {
    hour = bcd2bin(buf[HOUR] & 0x1F) % 12;
    if (buf[HOUR] & HOUR_PM)
      hour += 12;
  } else {
    hour = bcd2bin(buf[HOUR] & 0x3F);
  }

  mday = bcd2bin(buf[DATE]);
  mon  = bcd2bin(buf[MONTH]);
  year = bcd2bin(buf[YEAR]);
  wday = bcd2bin(buf[DAY]);

  is_leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);

  tm->tm_sec   = sec;
  tm->tm_min   = min;
  tm->tm_hour  = hour;
  tm->tm_mday  = mday;
  tm->tm_mon   = mon - 1;
  tm->tm_year  = year + (2000 - 1900);
  tm->tm_yday  = daysto[is_leap][mon - 1] + mday - 1;
  tm->tm_wday  = wday - 1;
  tm->tm_isdst = 0;
}

/**
 * Set current time.
 * 
 * @param ds1338  Pointer to the RTC driver instance
 * @param tm      Pointer to a tm structure holding the new date and time.
 */
void
ds1338_set_time(struct DS1338 *ds1338, const struct tm *tm)
{
  uint8_t buf[7];

  buf[SECONDS] = bin2bcd(tm->tm_sec);
  buf[MINUTES] = bin2bcd(tm->tm_min);
  buf[HOUR]    = bin2bcd(tm->tm_hour);
  buf[DATE]    = bin2bcd(tm->tm_mday);
  buf[MONTH]   = bin2bcd(tm->tm_mon + 1);
  buf[YEAR]    = bin2bcd(tm->tm_year - (2000 - 1900));
  buf[DAY]     = bin2bcd(tm->tm_wday + 1);

  // Update the contents of the time and calendar registers
  sbcon_write(ds1338->i2c, ds1338->address, SECONDS, buf, 7);
}
