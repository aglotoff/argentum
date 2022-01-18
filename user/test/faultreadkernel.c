#include <stdio.h>

// Buggy program - try to read from the kernel address space.
int
main(void)
{
  printf("Read %08x from address 0x80010000!\n", *(int *) 0x80010000);
  return 0;
}
