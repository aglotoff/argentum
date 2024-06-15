#include <unistd.h>
#include <stdio.h>

long
sysconf(int name)
{
  // TODO: map system page read-only to each process' address space
  switch (name) {
  case _SC_PAGE_SIZE:
    return 4096;
  case _SC_PHYS_PAGES:
    return 256 * 1024 * 1024;
  case _SC_OPEN_MAX:
    return 20;
  case _SC_LINE_MAX:
    return 256;
  default:
    fprintf(stderr, "TODO: sysconf(%d)\n", name);
    return -1;
  }
}
