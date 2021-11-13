#include <stdio.h>
#include <time.h>

char *
asctime(const struct tm *timeptr)
{
  static char result[26];

  if (strftime(result, sizeof(result), "%a %b %d", timeptr) > 0)
    return result;
  return NULL;
}

//   snprintf(result, sizeof(result), "%s %s %d %02d:%02d:%02d %d",
//            daynames[timeptr->tm_wday], monthnames[timeptr->tm_mon],
//            timeptr->tm_mday, timeptr->tm_hour, timeptr->tm_min, timeptr->tm_sec,
//            1900 + timeptr->tm_year);
//   return result;
// }
