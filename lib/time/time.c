#include <stddef.h>
#include <syscall.h>
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
  time_t ret;

  ret = __syscall(__SYS_TIME, 0, 0, 0);

  if (tloc != NULL)
    *tloc = ret;

  return ret;
}
