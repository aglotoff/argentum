#include <syscall.h>
#include <unistd.h>

/**
 * @brief Create a new process.
 */
pid_t
fork(void)
{
  return syscall(SYS_fork, 0, 0, 0);
}
