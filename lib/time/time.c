#include <stddef.h>
#include <time.h>

/**
 * Determine the current calendar time in seconds since the Epoch.
 * 
 * @param timer Pointer to an area to store the time value or NULL
 *
 * @return The current calendar time in seconds since the Epoch. The value
 *         (time_t)-1 is returned if the calendar time is not available.
 */
time_t
time(time_t *tloc)
{
  struct timespec t;
  int ret;

  ret = clock_gettime(CLOCK_REALTIME, &t);

  if (ret == 0 && tloc != NULL)
    *tloc = t.tv_sec;

  return ret == 0 ? t.tv_sec : -1;
}
