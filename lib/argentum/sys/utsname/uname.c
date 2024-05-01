#include <sys/syscall.h>
#include <sys/utsname.h>

int
uname(struct utsname *name)
{
  return __syscall1(__SYS_UNAME, name);
}
