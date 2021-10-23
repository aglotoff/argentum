#include <syscall.h>
#include <user.h>

int
cwrite(const char *s, size_t n)
{
  return syscall(SYS_cwrite, (uint32_t) s, n, 0);
}

void
exit(void)
{
  syscall(SYS_exit, 0, 0, 0);
}
