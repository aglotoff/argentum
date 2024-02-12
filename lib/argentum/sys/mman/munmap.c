#include <stdio.h>
#include <sys/mman.h>
#include <sys/syscall.h>

int
munmap(void *addr, size_t len)
{
  return __syscall(__SYS_MUNMAP, (uintptr_t) addr, len, 0, 0, 0, 0);
}
