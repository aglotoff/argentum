#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H

#include <sys/types.h>

#define PROT_NONE     0
#define PROT_READ     (1 << 0)
#define PROT_WRITE    (1 << 1)
#define PROT_EXEC     (1 << 2)
#define PROT_NOCACHE  (1 << 3)
#define _PROT_USER    (1 << 4)
#define _PROT_COW     (1 << 5)
#define _PROT_PAGE    (1 << 6)

#define MAP_FIXED   (1 << 0)
#define MAP_PRIVATE (1 << 1)
#define MAP_SHARED  (1 << 2)

void  *mmap(void *, size_t, int, int, int, off_t);
int    mprotect(void *, size_t, int);
int    munmap(void *, size_t);

#endif /* !_SYS_MMAN_H */
