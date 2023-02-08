#include <time.h>

/**
 * Convert a time value to a broken-down time.
 * 
 * @param timer Pointer to an area that contains the time in seconds since the
 *              Epoch.
 * 
 * @return A pointer to a tm structure with its memebers filled with values
 *         that correspond to local time.
 */
struct tm *
localtime(const time_t *timer)
{
  // TODO: handle timezone
  return gmtime(timer);
}
