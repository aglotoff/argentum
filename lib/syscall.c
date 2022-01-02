#include <syscall.h>
#include <user.h>

int
cwrite(const char *s, size_t n)
{
  return __syscall(__SYS_CWRITE, (uint32_t) s, n, 0);
}
