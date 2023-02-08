#include <stdio.h>
#include <string.h>
#include <time.h>

static size_t put_time(char *, size_t, const char *, const struct tm *);

/**
 * Convert date and time to a string.
 * 
 * The format string consists of zero or more conversion specifiers and ordinary
 * multibyte characters. A conversion specifier consists of a '%' character
 * followed by a character that determines the behavior of the conversion
 * specifier. All ordinary multibyte characters (including the terminating null
 * character) are copied unchanged into the array. If copying takes place
 * between objects that overlap, the behavior is undefined. Each conversion
 * specifier is replaced by appropriate characters as described in the following
 * list:
 * 
 * '%a' is replaced by the locale's abbreviated weekday name;
 * '%A' is replaced by the locale's full wekday name;
 * '%b' is replaced by the locale's abbreviated month name;
 * '%B' is replaced by the locale's full month name;
 * '%c' is replaced by the locale's appropriate date and time representation;
 * '%d' is replaced by the day of month as a decimal number (0-31);
 * '%H' is replaced by the hour (24-hour clock) as a decimal number (00-23);
 * '%I' is replaced by the hour (12-hour clock) as a decimal number (01-12);
 * '%j' is replaced by the day of the year as a decimal number (001-366);
 * '%m' is replaced by the month as a decimal number (01-12);
 * '%M' is replaced by the minute as a decimal number (00-59);
 * '%p' is replaced by the locale's equivalent of the AM/PM designations
 *      associated with a 12-hour clock;
 * '%S' is replaced by the second as a decimal number (00, 60);
 * '%U' is replaced by the week number of the year (the first Sunday as the
 *      first day of week 1) as a decimal number (00-53);
 * '%w' is replaced by the weekday as a decimal number (0-6), where Sunday is 0;
 * '%W' is replaced by the week number of the year (the first Monday as the
 *      first day of week 1) as a decimal number (00-53);
 * '%x' is replaced by the locale's appropriate date representation;
 * '%X' is replaced by the locale's appropriate time representation;
 * '%y' is replaced by the year without century as a decimal number (00-99);
 * '%Y' is replaced by the year with century as a decimal number;
 * '%Z' is replaced by the time zone name or abbreviation, or by no characters
 *      if no time zone is determinable;
 * '%%' is replaced by '%'.
 * 
 * @param s       Pointer to the array to place bytes into.
 * @param maxsize The maximum number of bytes to place to place into s.
 * @param format  The format string.
 * @param timeptr Pointer to the broken-down time structure.
 * 
 * @return If the total number of resulting bytes including the terminating
 *         null byte is not more than maxsize, the number of bytes placed is
 *         returned. Otherwise, 0 is returned.
 */
size_t
strftime(char *s, size_t maxsize, const char *format, const struct tm *timeptr)
{
  size_t i;

  i = put_time(s, maxsize - 1, format, timeptr);

  if (i > 0)
    s[i] = '\0';

  return i;
}

static const char *abday[] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char *day[] = {
  "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

static const char *abmon[] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

static const char *mon[] = {
  "January", "February", "March",     "April",   "May",      "June",
  "July",    "August",   "September", "October", "November", "December",
};

static const char *ampm[] = { "AM", "PM" };

static const char *d_t_fmt = "%a %b %e %H:%M:%S %Y";

static const char *d_fmt = "%m/%d/%y";

static const char *t_fmt = "%H:%M:%S";

// static const char *t_fmt_ampm = "%I:%M:%S %p";

static size_t
put_string(char *p, size_t maxsize, const char *s)
{
  size_t i, len;

  len = strlen(s);
  if (len > maxsize)
    return 0;

  for (i = 0; i < len; i++)
    p[i] = s[i];
  return i;
}

static size_t
put_number(char *p, size_t maxsize, int n, char pad_char, size_t width)
{
  size_t len = 0, i = 0;
  char buf[16];

  do {
    buf[len++] = '0' + (n % 10);
    n /= 10;
  } while (n != 0);

  while (width > len)
    buf[len++] = pad_char;

  if (len > maxsize)
    return 0;

  while (len > 0)
    p[i++] = buf[--len];

  return i;
}

#define DAYS_PER_WEEK 7
#define SUNDAY        0
#define MONDAY        6

static int
week_number(int yday, int wday, int start)
{
  wday = (DAYS_PER_WEEK - start + wday) % DAYS_PER_WEEK;
  return (wday > yday) ? 0 : (yday / DAYS_PER_WEEK + 1);
}

static size_t
do_conversion(char *s, size_t maxsize, char format, const struct tm *timeptr)
{
  size_t i = 0;
  
  switch (format) {
  case 'a':
    return put_string(s, maxsize, abday[timeptr->tm_wday]);
  case 'A':
    return put_string(s, maxsize, day[timeptr->tm_wday]);
  case 'b':
    return put_string(s, maxsize, abmon[timeptr->tm_mon]);
  case 'B':
    return put_string(s, maxsize, mon[timeptr->tm_mon]);
  case 'c':
    return put_time(s, maxsize, d_t_fmt, timeptr);
  case 'd':
    return put_number(s, maxsize, timeptr->tm_mday, '0', 2);
  case 'H':
    return put_number(s, maxsize, timeptr->tm_hour, '0', 2);
  case 'I':
    return put_number(s, maxsize,
                      timeptr->tm_hour % 12 == 0 ? 12 : timeptr->tm_hour % 12,
                      '0', 2);
  case 'j':
    return put_number(s, maxsize, timeptr->tm_yday + 1, '0', 3);
  case 'm':
    return put_number(s, maxsize, timeptr->tm_mon + 1, '0', 2);
  case 'M':
    return put_number(s, maxsize, timeptr->tm_min, '0', 2);
  case 'p':
    return put_string(s, maxsize,
                      ampm[timeptr->tm_hour == 0 || timeptr->tm_hour > 12]);
  case 'S':
    return put_number(s, maxsize, timeptr->tm_sec, '0', 2);
  case 'U':
    return put_number(s, maxsize,
                      week_number(timeptr->tm_yday, timeptr->tm_wday, SUNDAY),
                      '0', 2);
  case 'w':
    return put_number(s, maxsize, timeptr->tm_wday, '0', 1);
  case 'W':
    return put_number(s, maxsize,
                      week_number(timeptr->tm_yday, timeptr->tm_wday, MONDAY),
                      '0', 2);
  case 'x':
    return put_time(s, maxsize, d_fmt, timeptr);;
  case 'X':
    return put_time(s, maxsize, t_fmt, timeptr);
  case 'y':
    return put_number(s, maxsize, timeptr->tm_year % 100, '0', 2);
  case 'Y':
    return put_number(s, maxsize, 1900 + timeptr->tm_year, '0', 4);
  case 'Z':
    return put_string(s, maxsize, "UTC");
  case '%':
    if (i < maxsize)
      s[i++] = '%';
    break;
  default:
    if (i < maxsize - 1) {
      s[i++] = '%';
      s[i++] = format;
    }
    break;
  }

  return i;
}

static size_t
put_time(char *s, size_t maxsize, const char *format, const struct tm *timeptr)
{
  size_t i, len;

  i = 0;
  for (;;) {
    // Print ordinary characters literally
    while ((*format != '\0') && (*format != '%') && (i < maxsize))
      s[i++] = *format++;

    if (*format == '\0')
      break;

    if (i == maxsize)
      return 0;

    if ((len = do_conversion(&s[i], maxsize - i, *++format, timeptr)) == 0)
      return 0;

    i += len;
    format++;
  }

  return i;
}
