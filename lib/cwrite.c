#include <argentum/syscall.h>

void
cwrite(const void *src, size_t n)
{
  __syscall(__SYS_CWRITE, (uintptr_t) src, n, 0);
}
