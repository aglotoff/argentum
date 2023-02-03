#include <time.h>

/**
 * Determine the processor time used.
 *
 * @return The processor time used by the program since the Epoch. To determine
 *         the time in seconds, this value should be divided by CLOCKS_PER_SEC.
 *         If the processor time used is not available or its value cannot be
 *         represented, the value (clock_t)-1 is returned.
 */
clock_t
clock(void)
{
  struct timespec t;
  int ret;

  // TODO: replace by CLOCK_MONOTONIC
  if ((ret = clock_gettime(CLOCK_REALTIME, &t)) != 0)
    return -1;

  return (clock_t) t.tv_sec * CLOCKS_PER_SEC + t.tv_nsec / 1000;
}
