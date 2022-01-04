#include <stdio.h>

// Buggy program - try to read from address 0.
int
main(void)
{
  printf("Read %08x from address 0x0!\n", *(int *) 0x0);
  return 0;
}
