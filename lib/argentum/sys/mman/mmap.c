#include <stdio.h>
#include <sys/mman.h>
#include <sys/syscall.h>

void *
mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off)
{
  int r = __syscall_r(__SYS_MMAP, (uintptr_t) addr, len, prot, flags, fildes,
                      off);

  if ((r < 0) && (r > -__ELASTERROR)) {
    errno = -r;
    return (void *) -1;
  }

  return (void *) r;
}
