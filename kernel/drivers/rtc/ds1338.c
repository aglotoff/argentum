// For more info, see the Maxim DS1338 RTC data sheet.

#include "ds1338.h"

// RTC Registers
#define DS1338_SECONDS   0x00
#define DS1338_MINUTES   0x01
#define DS1338_HOURS     0x02
#define DS1338_DAY       0x03
#define DS1338_DATE      0x04
#define DS1338_MONTH     0x05
#define DS1338_YEAR      0x06
#define DS1338_CONTROL   0x07

void
ds1338_init(struct DS1338 *ds1338, struct SBCon *i2c, uint8_t address)
{
  ds1338->i2c = i2c;
  ds1338->address = address;
}

static int
ds1338_read(struct DS1338 *ds1338, uint8_t reg)
{
  return sbcon_read(ds1338->i2c, ds1338->address, reg);
}

void
ds1338_get_time(struct DS1338 *ds1338, struct tm *tm)
{
  // Days to start of month
  static const int daysto[][12] = {
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 },  // non-leap
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 },  // leap
  };

  int sec, min, hour, mday, mon, year, wday, is_leap;

  sec = ds1338_read(ds1338, DS1338_SECONDS);
  sec = ((sec >> 4) & 0x7) * 10 + (sec & 0xF);

  min = ds1338_read(ds1338, DS1338_MINUTES);
  min = ((min >> 4) & 0x7) * 10 + (min & 0xF);

  hour = ds1338_read(ds1338, DS1338_HOURS);
  if (hour & 0x40) {
    hour = ((hour >> 4) & 0x1) * 10 + (hour & 0xF) + (hour & 0x20 ? 12 : 0);
    hour = (hour % 12 == 0) ? hour - 12 : hour;
  } else {
    hour = ((hour >> 4) & 0x3) * 10 + (hour & 0xF);
  }

  mday = ds1338_read(ds1338, DS1338_DATE);
  mday = ((mday >> 4) & 0x3) * 10 + (mday & 0xF);

  mon = ds1338_read(ds1338, DS1338_MONTH);
  mon = ((mon >> 4) & 0x1) * 10 + (mon & 0xF);

  year = ds1338_read(ds1338, DS1338_YEAR);
  year = ((year >> 4) & 0xF) * 10 + (year & 0xF);

  wday = ds1338_read(ds1338, DS1338_DAY);
  wday = (wday & 0x7);

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
