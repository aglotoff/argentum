#include <syscall.h>
#include <time.h>

/**
 * Return the current value for the specified clock.
 * 
 * @param clock_id  The clock ID.
 * @param tp        Pointer where to store the clock value.
 * 
 * @return If successful, 0 is returned. If an error occurs, -1 is returned.
 */
int
clock_gettime(clockid_t clock_id, struct timespec *tp)
{
  return __syscall(__SYS_CLOCK_TIME, clock_id, (uintptr_t) tp, 0);
}
