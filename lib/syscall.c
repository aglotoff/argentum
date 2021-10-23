#include <syscall.h>

void
cputs(const char *s)
{
  syscall(SYS_cputs, (uint32_t) s, 0, 0);
}

void
exit(void)
{
  syscall(SYS_exit, 0, 0, 0);
}
