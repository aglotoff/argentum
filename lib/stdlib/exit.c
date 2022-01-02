#include <syscall.h>
#include <user.h>

/**
 * Terminate a process.
 * 
 * @param status Exit status of the process.
 * 
 * @sa abort
 */
void
exit(int status)
{
  __syscall(__SYS_EXIT, status, 0, 0);
}
