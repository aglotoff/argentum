#include <stddef.h>
#include <time.h>

/**
 * Get time.
 * 
 * @param tloc Pointer to an area where the time value is stored. If NULL, no
 *             value is stored.
 *
 * @return The value of time in seconds since the Epoch.
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
