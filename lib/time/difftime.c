#include <time.h>

/**
 * Compute the difference between two calendar dates.
 * 
 * @param time1 The first date
 * @param time2 The second date
 * 
 * @return  The difference expressed in seconds as a double.
 */
double
difftime(time_t time1, time_t time2)
{
  return time1 >= time2 ? time1 - time2 : -(double)(time2 - time1);
}
