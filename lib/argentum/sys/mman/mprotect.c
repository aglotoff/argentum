#include <stdio.h>
#include <sys/mman.h>
#include <sys/syscall.h>

int
mprotect(void *addr, size_t len, int prot)
{
  return __syscall(__SYS_MPROTECT, (uintptr_t) addr, len, prot, 0, 0, 0);
}
