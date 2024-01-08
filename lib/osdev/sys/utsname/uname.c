#include <sys/syscall.h>
#include <sys/utsname.h>

int
uname(struct utsname *name)
{
  return __syscall(__SYS_UNAME, (uintptr_t) name, 0, 0, 0, 0, 0);
}
