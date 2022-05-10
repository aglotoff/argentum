#include <stdio.h>
#include <time.h>

static const char *monthnames[] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

char *
asctime(const struct tm *timeptr)
{
  static char result[26];

//   if (strftime(result, sizeof(result), "%a %b %d", timeptr) > 0)
//     return result;
//   return NULL;
// }

  snprintf(result, sizeof(result), "%s %2d %02d:%02d",
           monthnames[timeptr->tm_mon],
           timeptr->tm_mday, timeptr->tm_hour, timeptr->tm_min);
  return result;
}
