#include <time.h>

/**
 * Convert broken-down time into time since the Epoch.
 * 
 * @param timeptr Pointer to a tm structure that contains a rokwen-down time.
 * 
 * @return A time_t value corresponding to the specified time.
 */
time_t
mktime(struct tm *timeptr)
{
  return timeptr->tm_sec +
         timeptr->tm_min * 60 +
         timeptr->tm_hour * 3600 +
         timeptr->tm_yday * 86400 +
         (timeptr->tm_year - 70) * 31536000 +
         // add a day every 4 years starting in 1973
         ((timeptr->tm_year - 69) / 4) * 86400 -
         // subtract a day back out every 100 years starting in 2001
         ((timeptr->tm_year - 1) / 100) * 86400 +
         // add a day back in every 400 years starting in 2001
         ((timeptr->tm_year + 299) / 400) * 86400;
}
