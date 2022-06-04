#include <syscall.h>
#include <sys/utsname.h>

/**
 * Get the name of the current system.
 * 
 * @param name Pointer to the structure to store the system information.
 * 
 * @returns 0 on success, -1 otherwise.
 */
int
uname(struct utsname *name)
{
  return __syscall(__SYS_UNAME, (uintptr_t) name, 0, 0);
}
