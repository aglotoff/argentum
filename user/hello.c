#include <stdio.h>
#include <unistd.h>

int
main(void)
{
  printf("Hello world!\n");
  printf("I am process %d\n", getpid());
  return 0;
}
