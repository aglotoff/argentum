#include <stdio.h>
#include <sys/mman.h>
#include <sys/syscall.h>

int
munmap(void *addr, size_t len)
{
  return __syscall2(__SYS_MUNMAP, addr, len);
}
