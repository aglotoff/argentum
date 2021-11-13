
#include <stdio.h>
#include <time.h>

// Days to start of month.
static const int daysto[][12] = {
  { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 },  // non-leap year
  { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 },  // leap year
};

/**
 * Convert a time value to a broken-down UTC time.
 * 
 * @param timer Pointer to an area that contains the time in seconds since the
 *              Epoch.
 * 
 * @return A pointer to a tm structure with its memebers filled with values
 *         that correspong to Coordinated Universal Time (UTC).
 */
struct tm *
gmtime(const time_t *timer)
{
  static struct tm ts;

  int days, year, is_leap, mon, secs;

  days = *timer / 86400;
  secs = *timer % 86400;

  ts.tm_wday = (days + 4) % 7;

  is_leap = 0;
  year = 1970;
  while (days >= 365) {
    days -= is_leap ? 366 : 365;
    year++;
    is_leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
  }

  ts.tm_year = year - 1900;
  ts.tm_yday = days;
  
  for (mon = 0; (mon < 11) && (days >= daysto[is_leap][mon + 1]); mon++)
    ;
  ts.tm_mon  = mon;
  ts.tm_mday = days - daysto[is_leap][mon] + 1;

  ts.tm_hour = secs / 3600;
  ts.tm_min  = (secs / 60) % 60;
  ts.tm_sec  = secs % 60;

  ts.tm_isdst = 0;

  return &ts;
}
