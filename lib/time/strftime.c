#include <time.h>

static const char *wday_short[] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char *wday_long[] = {
  "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

static const char *mon_short[] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

static const char *mon_long[] = {
  "January", "February", "March",     "April",   "May",      "June",
  "July",    "August",   "September", "October", "November", "December",
};

static void
putstr(const char *s, char *buf, size_t *i, size_t maxsize)
{
  size_t j;
  
  for (j = *i; (j < maxsize) && (*s != '\0'); j++)
    buf[j] = *s++;
  *i = j;
}

size_t
strftime(char *s, size_t maxsize, const char *format, const struct tm *timeptr)
{
  size_t i;
  
  for (i = 0; i < maxsize; format++) {
    // Ordinary characters are printed literally
    while ((i < maxsize) && *format && (*format != '%')) {
      s[i++] = *format++;
    }

    if ((*format++ == '\0') || (i >= maxsize))
      break;

    // Do conversion
    switch (*format) {
    case 'a':
      putstr(wday_short[timeptr->tm_wday % 7], s, &i, maxsize);
      break;
    case 'A':
      putstr(wday_long[timeptr->tm_wday % 7], s, &i, maxsize);
      break;
    case 'b':
      putstr(mon_short[timeptr->tm_mon % 12], s, &i, maxsize);
      break;
    case 'B':
      putstr(mon_long[timeptr->tm_mon % 12], s, &i, maxsize);
      break;
    case '%':
      s[i++] = '%';
      break;
    default:
      s[i++] = '%';
      if (i < maxsize)
        s[i++] = *format;
      break;
    }
  }

  if (i < maxsize) {
    s[i] = '\0';
    return i;
  }

  return 0;
}