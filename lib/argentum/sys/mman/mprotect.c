#include <stdio.h>
#include <sys/mman.h>
#include <sys/syscall.h>

int
mprotect(void *addr, size_t len, int prot)
{
  return __syscall3(__SYS_MPROTECT, addr, len, prot);
}
